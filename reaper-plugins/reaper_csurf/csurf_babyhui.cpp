/*
** reaper_csurf
** "hui"-ish support
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/


#include "csurf.h"
#include <time.h>


static bool g_csurf_mcpmode=false; // we may wish to allow an action to set this
static int m_flipmode;


static double int14ToVol(unsigned char msb, unsigned char lsb)
{
  int val=lsb | (msb<<7);
  double pos=((double)val*1000.0)/16383.0;
  pos=SLIDER2DB(pos);
  return DB2VAL(pos);
}
static double int14ToPan(unsigned char msb, unsigned char lsb)
{
  int val=lsb | (msb<<7);
  return 1.0 - (val/(16383.0*0.5));
}

static int volToInt14(double vol)
{
  double d=(DB2SLIDER(VAL2DB(vol))*16383.0/1000.0);
  if (d<0.0)d=0.0;
  else if (d>16383.0)d=16383.0;

  return (int)(d+0.5);
}
static  int panToInt14(double pan)
{
  double d=((1.0-pan)*16383.0*0.5);
  if (d<0.0)d=0.0;
  else if (d>16383.0)d=16383.0;

  return (int)(d+0.5);
}
static  unsigned char volToChar(double vol)
{
  double d=(DB2SLIDER(VAL2DB(vol))*127.0/1000.0);
  if (d<0.0)d=0.0;
  else if (d>127.0)d=127.0;

  return (unsigned char)(d+0.5);
}

static unsigned char panToChar(double pan)
{
  pan = (pan+1.0)*63.5;

  if (pan<0.0)pan=0.0;
  else if (pan>127.0)pan=127.0;

  return (unsigned char)(pan+0.5);
}


static int m_allhui_bank_offset;




class CSurf_BabyHUI : public IReaperControlSurface
{
    int m_midi_in_dev,m_midi_out_dev;
    int m_offset, m_size;
    midi_Output *m_midiout;
    midi_Input *m_midiin;

    WDL_String descspace;
    char configtmp[1024];

    int m_babyhui_wt;
    unsigned char m_babyhui_track_msb[16][8];
    char m_fader_touchstate[256];
    int m_button_states; // 1=rew, 2=ffwd
    unsigned int m_pan_lasttouch[256];

    int m_vol_lastpos[256];
    int m_pan_lastpos[256];
    time_t m_last_keepalive;

    void OnMIDIEvent(MIDI_event_t *evt)
    {
      if ((evt->midi_message[0]&0xf0) == 0xB0)
      {    
        if (evt->midi_message[1] == 0x0f) // select track
        {
          m_babyhui_wt=evt->midi_message[2];
        }
        else if (evt->midi_message[1] == 0x2f)
        {
          if (m_babyhui_wt>=0)
          {
            int allow_passthru=0;
            if (m_babyhui_wt == 0xc && evt->midi_message[2] == 0x43)
            {
              if (m_offset<8)
              {
                // yamaha DM2000: toggle mcp mode with "fader" button
                g_csurf_mcpmode=!g_csurf_mcpmode;
                if (m_midiout) 
                {
                  m_midiout->Send(0xb0,0xc,0xc,-1);
                  m_midiout->Send(0xb0,0x2c,g_csurf_mcpmode ? 0x43 : 0x03,-1);
                }
                TrackList_UpdateAllExternalSurfaces();
              }
            }
            else if (m_babyhui_wt == 0xb && evt->midi_message[2] == 0x42)
            {
              if (m_offset<8)
              {
                // yamaha DM2000: toggle flip mode
                m_flipmode=!m_flipmode;
                if (m_midiout) 
                {
                  m_midiout->Send(0xb0,0xc,0xb,-1);
                  m_midiout->Send(0xb0,0x2c,m_flipmode ? 0x42 : 0x02,-1);
                }
                CSurf_ResetAllCachedVolPanStates();
                TrackList_UpdateAllExternalSurfaces();
              }
            }
            else if (m_babyhui_wt == 0xa && evt->midi_message[2] >= 0x40 && evt->midi_message[2] <= 0x43)
            {
              if (evt->midi_message[2] > 0x41) // increase by X
              {
                m_allhui_bank_offset+=(evt->midi_message[2]&1) ? 8 : 1;
                int msize=CSurf_NumTracks(g_csurf_mcpmode);

                if (m_allhui_bank_offset >= msize) m_allhui_bank_offset=msize-1;
              }
              else
              {
                m_allhui_bank_offset-=(evt->midi_message[2]&1) ? 8 : 1;
                if (m_allhui_bank_offset<0)m_allhui_bank_offset=0;
              }
              // update all of the sliders
              TrackList_UpdateAllExternalSurfaces();
            }
            else if (evt->midi_message[2] == 0x40)  // touch
            {
              if (m_babyhui_wt < 8) 
              {
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) 
                  trackid+=(evt->midi_message[0]&15)*8;
                m_fader_touchstate[trackid]=1;
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x0)  // un touch
            {
              if (m_babyhui_wt < 8) 
              {
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
                m_fader_touchstate[trackid]=0;
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x41) 
            {
              if (m_babyhui_wt == 0xe) 
              {
                m_button_states|=1; 
                if (m_midiout) 
                {
                  m_midiout->Send(0xb0,0x0c,0xe,-1);
                  m_midiout->Send(0xb0,0x2c,evt->midi_message[2],-1);
                }
              }
              else if (m_babyhui_wt<8)
              {
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;

                MediaTrack *tr=CSurf_TrackFromID(m_offset+m_allhui_bank_offset+trackid,g_csurf_mcpmode);

                if (tr) CSurf_OnSelectedChange(tr,-1);
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x42) // mute click (0x02 is when you let go)
            {
              if (m_babyhui_wt == 0xe) 
              {
                m_button_states|=2;
                if (m_midiout) 
                {
                  m_midiout->Send(0xb0,0x0c,0xe,-1);
                  m_midiout->Send(0xb0,0x2c,evt->midi_message[2],-1);
                }
              }
              else if (m_babyhui_wt < 8) 
              {
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
                MediaTrack *tr=CSurf_TrackFromID(m_allhui_bank_offset+m_offset+trackid,g_csurf_mcpmode);
                if (tr) CSurf_SetSurfaceMute(tr,CSurf_OnMuteChange(tr,-1),NULL);
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x43) // solo click (0x03 is when you let go)
            {
              if (m_babyhui_wt == 0xe) 
              {
                CSurf_OnStop();
              }
              else if (m_babyhui_wt < 8) 
              {
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
                MediaTrack *tr=CSurf_TrackFromID(m_allhui_bank_offset+m_offset+trackid,g_csurf_mcpmode);
                if (tr) CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,-1),NULL);
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x44)
            {
              if (m_babyhui_wt == 0xe) 
              {
                CSurf_OnPlay();
              }
              else if (m_babyhui_wt<8)
              {
resetpan:
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
                MediaTrack *tr=CSurf_TrackFromID(m_allhui_bank_offset+m_offset+trackid,g_csurf_mcpmode);
                if (tr) 
                {
                  if (m_flipmode)
                    CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,1.0,false),NULL);
                  else
                    CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,0.0,false),NULL);
                }
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x45) 
            {
              if (m_babyhui_wt >= 0 && m_babyhui_wt < 8) 
              {
                goto resetpan;
              }
              else if (m_babyhui_wt == 0xe) 
              {
                CSurf_OnRecord();
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x47) 
            {
              if (m_babyhui_wt<8) 
              {
                int trackid=m_babyhui_wt;
                if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;

                MediaTrack *tr=CSurf_TrackFromID(trackid+m_allhui_bank_offset+m_offset,g_csurf_mcpmode);
                if (tr) CSurf_OnRecArmChange(tr,-1);
      //          if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
    //            CSurf_SetSurfacePan(m_allhui_bank_offset+m_offset+trackid,CSurf_OnPanChange(m_allhui_bank_offset+m_offset+trackid,0.0));
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x01)
            {
              if (m_babyhui_wt == 0xe) 
              {
                m_button_states&=~1; 
                if (m_midiout) 
                {
                  m_midiout->Send(0xb0,0x0c,0xe,-1);
                  m_midiout->Send(0xb0,0x2c,evt->midi_message[2],-1);
                }
              }
              else allow_passthru=1;
            }
            else if (evt->midi_message[2] == 0x02)
            {
              if (m_babyhui_wt == 0xe) 
              {
                m_button_states&=~2; 
                if (m_midiout) 
                {
                  m_midiout->Send(0xb0,0x0c,0xe,-1);
                  m_midiout->Send(0xb0,0x2c,evt->midi_message[2],-1);
                }
              }
              else allow_passthru=1;
            }
            else allow_passthru=1;

            if (evt->midi_message[2] >= 0x40 && allow_passthru)
            {
              MIDI_event_t evt2={0,3,{evt->midi_message[0]^0x0f,((evt->midi_message[2]<<4)&0xf0)|m_babyhui_wt,0x40}};
              kbd_OnMidiEvent(&evt2,-1);
            }

          }
        }
        else if (evt->midi_message[1] >= 0x20 && evt->midi_message[1] < 0x28) // lsb
        {
          int trackid=evt->midi_message[1]-0x20;
          if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
          MediaTrack *tr=CSurf_TrackFromID(m_allhui_bank_offset+m_offset+trackid,g_csurf_mcpmode);
          if (tr) 
          {
            if (m_flipmode)
              CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,int14ToPan(m_babyhui_track_msb[evt->midi_message[0]&15][trackid],evt->midi_message[2]),false),NULL);
            else
              CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,int14ToVol(m_babyhui_track_msb[evt->midi_message[0]&15][trackid],evt->midi_message[2]),false),NULL);
          }
        }
        else if (evt->midi_message[1] >= 0x40 && evt->midi_message[1] < 0x48) // lsb
        {
          int trackid=evt->midi_message[1]-0x40;
          if ((evt->midi_message[0]&15)<(m_size/8)) trackid+=(evt->midi_message[0]&15)*8;
          double adj=(evt->midi_message[2]&0x3f)/-63.0;
          if (evt->midi_message[2]&0x40) adj=-adj;

          MediaTrack *tr=CSurf_TrackFromID(trackid+m_allhui_bank_offset+m_offset,g_csurf_mcpmode);
          if (tr) 
          {
            if (m_flipmode)
              CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,adj*11.0,true),NULL);
            else
              CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,adj,true),NULL);
          }
          m_pan_lasttouch[trackid&0xff]=timeGetTime();
        }
        else if (evt->midi_message[1] < 0x8) // msb
        {
          m_babyhui_track_msb[evt->midi_message[0]&15][evt->midi_message[1]-0x0]=evt->midi_message[2];
        }

      }
    }

public:
  CSurf_BabyHUI(int offset, int size, int indev, int outdev, int *errStats)
  {
    m_last_keepalive=0;
    m_offset=offset;
    m_size=size;
    m_midi_in_dev=indev;
    m_midi_out_dev=outdev;
  
    m_button_states=0;
    m_babyhui_wt=-1;
    memset(m_babyhui_track_msb,0,sizeof(m_babyhui_track_msb));
    memset(m_fader_touchstate,0,sizeof(m_fader_touchstate));
    memset(m_pan_lasttouch,0,sizeof(m_pan_lasttouch));
    memset(m_vol_lastpos,0xff,sizeof(m_vol_lastpos));
    memset(m_pan_lastpos,0xff,sizeof(m_pan_lastpos));

    //create midi hardware access
    m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
    m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput(CreateMIDIOutput(m_midi_out_dev,false,NULL)) : NULL;

    if (errStats)
    {
      if (m_midi_in_dev >=0  && !m_midiin) *errStats|=1;
      if (m_midi_out_dev >=0  && !m_midiout) *errStats|=2;
    }

    if (m_midiout)
    {
      m_midiout->Send(0xb0,0xc,0xc,-1);
      m_midiout->Send(0xb0,0x2c,g_csurf_mcpmode ? 0x43 : 0x03,-1);
      m_midiout->Send(0xb0,0xc,0xb,-1);
      m_midiout->Send(0xb0,0x2c,m_flipmode ? 0x42 : 0x02,-1);
    }

    if (m_midiin)
      m_midiin->start();

  }
  ~CSurf_BabyHUI()
  {
    CloseNoReset();
  }


  const char *GetTypeString() { return "HUI"; }
  const char *GetDescString()
  {
    descspace.SetFormatted(512,__LOCALIZE_VERFMT("HUI(partial) (dev %d,%d)","csurf"),m_midi_in_dev,m_midi_out_dev);
    return descspace.Get();     
  }
  const char *GetConfigString() // string of configuration data
  {
    sprintf(configtmp,"%d %d %d %d",m_offset,m_size,m_midi_in_dev,m_midi_out_dev);      
    return configtmp;
  }

  void CloseNoReset() 
  { 
    int i;
    for (i = 0; i < 256; ++i) {
      SetTrackTitleImpl(i, "");
    }

    DELETE_ASYNC(m_midiout);
    DELETE_ASYNC(m_midiin);
    m_midiout=0;
    m_midiin=0;
  }

  void Run()
  {
    if (m_midiout)
    {
      time_t now = time(NULL);
      if (now != m_last_keepalive)
      {
        m_last_keepalive=now;
        m_midiout->Send(0x90,0,0,-1);
      }
    }
    if (m_midiin)
    {
      m_midiin->SwapBufs(timeGetTime());
      int l=0;
      MIDI_eventlist *list=m_midiin->GetReadBuf();
      MIDI_event_t *evts;
      while ((evts=list->EnumItems(&l))) OnMIDIEvent(evts);
    }
  }

  void SetTrackListChange()
  {
    int trackid;
    for (trackid = 0; trackid < 256; ++trackid) {
      MediaTrack* tr = CSurf_TrackFromID(m_offset+m_allhui_bank_offset+trackid,g_csurf_mcpmode);
      if (!tr) SetTrackTitleImpl(trackid, "");    
    }
  }


#define FIXID(id) const int oid=CSurf_TrackToID(trackid,g_csurf_mcpmode); const int id=oid - (m_offset+m_allhui_bank_offset);

  void SetSurfaceVolume(MediaTrack *trackid, double volume) 
  {
    FIXID(id)
    if (m_midiout && id >= 0 && id < 256 && id < m_size)
    {
      if (m_flipmode)
      {
        int volint=volToChar(volume);
        m_midiout->Send(0xb0+(id/8),0x10+(id&7),1+((volint*11)>>7),-1);
      }
      else
      {
        int volint=volToInt14(volume);

        if (m_vol_lastpos[id]!=volint)
        {
          m_vol_lastpos[id]=volint;
          m_midiout->Send(0xb0+(id/8),id&7,(volint>>7)&0x7f,-1);
          m_midiout->Send(0xb0+(id/8),0x20+(id&7),volint&0x7f,-1);
        }
      }
    }
  }
  void SetSurfacePan(MediaTrack *trackid, double pan) 
  {
    FIXID(id)
    if (m_midiout && id >= 0 && id < 256 && id < m_size)
    {
      unsigned char panch=panToChar(pan);
      if (m_pan_lastpos[id] != panch)
      {
        m_pan_lastpos[id]=panch;
        if (m_flipmode)
        {
          int panint=panToInt14(pan);
          if (m_vol_lastpos[id]!=panint)
          {
            m_vol_lastpos[id]=panint;
            m_midiout->Send(0xb0+(id/8),id&7,(panint>>7)&0x7f,-1);
            m_midiout->Send(0xb0+(id/8),0x20+(id&7),panint&0x7f,-1);
          }
        }
        else
        {
          m_midiout->Send(0xb0+(id/8),0x10+(id&7),1+((panch*11)>>7),-1);
        }
      }
    }
  }
  void SetSurfaceMute(MediaTrack *trackid, bool mute) 
  { 
    FIXID(id)
    if (m_midiout && id>=0 && id < 256 && id < m_size)
    {
      m_midiout->Send(0xB0+(id/8),0x0c,id&7,-1);
      m_midiout->Send(0xB0+(id/8),0x2c,mute?0x42:0x02,-1);
    }
  }
  void SetSurfaceSelected(MediaTrack *trackid, bool selected) 
  {
    FIXID(id)
    if (m_midiout && id >= 0 && id < 256 && id < m_size)
    {
      m_midiout->Send(0xb0+(id/8),0x0c,id&7,-1);
      m_midiout->Send(0xb0+(id/8),0x2c,selected?0x41:0x01,-1);
    }
  }
  void SetSurfaceSolo(MediaTrack *trackid, bool solo) 
  { 
    FIXID(id)
    if (m_midiout && id>=0 && id < 256 && id < m_size)
    {
      if (!oid) solo=!!(GetMasterMuteSoloFlags()&2);
      m_midiout->Send(0xb0+(id/8),0x0c,id&7,-1);
      m_midiout->Send(0xb0+(id/8),0x2c,solo?0x43:0x03,-1);
    }
  }
  void SetSurfaceRecArm(MediaTrack *trackid, bool recarm) 
  { 
    FIXID(id)
    if (m_midiout && id>=0 && id < 256 && id < m_size)
    {
      m_midiout->Send(0xb0+(id/8),0x0c,id&7,-1);
      m_midiout->Send(0xb0+(id/8),0x2c,recarm?0x47:0x07,-1);
    }
  }
  void SetPlayState(bool play, bool pause, bool rec) 
  { 
    if (m_midiout)
    {
      m_midiout->Send(0xb0,0x0c,0xe,-1);
      m_midiout->Send(0xb0,0x2c,play?0x44:0x04,-1);
      m_midiout->Send(0xb0,0x0c,0xe,-1);
      m_midiout->Send(0xb0,0x2c,pause?0x43:0x03,-1);
      m_midiout->Send(0xb0,0x0c,0xe,-1);
      m_midiout->Send(0xb0,0x2c,rec?0x45:0x05,-1);
    }
  }
  void SetRepeatState(bool rep) 
  { 
    // not used
  }

  void SetTrackTitleImpl(int trackid, const char* title)
  {
    if (m_midiout && trackid >= 0 && trackid < 256 && trackid < m_size) {
      #define SYSEX_TRACKTITLE_LEN 13
      unsigned char sysex[SYSEX_TRACKTITLE_LEN] = { 0xF0, 0x00, 0x00, 0x66, 0x05, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7 };
      sysex[7] = trackid;
      int i, len = (title?strlen(title):0);
      for (i = 0; i < min(len, 4) && title[i]; ++i) {
        sysex[8+i] = title[i];
      }
      
      char buf[sizeof(MIDI_event_t)+SYSEX_TRACKTITLE_LEN];
      MIDI_event_t* msg = (MIDI_event_t*) buf;
      msg->frame_offset = -1;
      msg->size = SYSEX_TRACKTITLE_LEN;
      memcpy(msg->midi_message, sysex, SYSEX_TRACKTITLE_LEN);
      m_midiout->SendMsg(msg, -1);
    }
  }

  void SetTrackTitle(MediaTrack *trackid, const char *title) 
  { 
    FIXID(id)
    SetTrackTitleImpl(id, title);
  }

  bool GetTouchState(MediaTrack *trackid, int isPan) 
  { 
    if (isPan != 0 && isPan != 1) return false;

    FIXID(id)
    if (!m_flipmode != !isPan) 
    {
      if (id >= 0 && id < 256 && id < m_size)
      {
        if ((timeGetTime()-m_pan_lasttouch[id]) < 3000) // fake touch, go for 3s after last movement
          return true;
      }
      return false;
    }
    if (id >= 0 && id < 256 && id < m_size)
      return !!m_fader_touchstate[id];
    return false;
  }

  void SetAutoMode(int mode) { }

  void ResetCachedVolPanStates() 
  { 
    memset(m_vol_lastpos,0xff,sizeof(m_vol_lastpos));
    memset(m_pan_lastpos,0xff,sizeof(m_pan_lastpos));
  }
  void OnTrackSelection(MediaTrack *trackid) 
  { 
  }
  
  bool IsKeyDown(int key) 
  { 
    return false; 
  }

  virtual int Extended(int call, void *parm1, void *parm2, void *parm3)
  {
    DEFAULT_DEVICE_REMAP()
    return 0;
  }

};


static void parseParms(const char *str, int parms[4])
{
  parms[0]=1;
  parms[1]=8;
  parms[2]=parms[3]=-1;

  const char *p=str;
  if (p)
  {
    int x=0;
    while (x<4)
    {
      while (*p == ' ') p++;
      if ((*p < '0' || *p > '9') && *p != '-') break;
      parms[x++]=atoi(p);
      while (*p && *p != ' ') p++;
    }
  }  
}

static IReaperControlSurface *createFunc(const char *type_string, const char *configString, int *errStats)
{
  int parms[4];
  parseParms(configString,parms);

  return new CSurf_BabyHUI(parms[0],parms[1],parms[2],parms[3],errStats);
}


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        int parms[4];
        parseParms((const char *)lParam,parms);
        WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO2));
        WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO3));

        int n=GetNumMIDIInputs();
        int x=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("None","csurf"));
        SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETITEMDATA,x,-1);
        x=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("None","csurf"));
        SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETITEMDATA,x,-1);
        for (x = 0; x < n; x ++)
        {
          char buf[512];
          if (GetMIDIInputName(x,buf,sizeof(buf)))
          {
            int a=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)buf);
            SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETITEMDATA,a,x);
            if (x == parms[2]) SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_SETCURSEL,a,0);
          }
        }
        n=GetNumMIDIOutputs();
        for (x = 0; x < n; x ++)
        {
          char buf[512];
          if (GetMIDIOutputName(x,buf,sizeof(buf)))
          {
            int a=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)buf);
            SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETITEMDATA,a,x);
            if (x == parms[3]) SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_SETCURSEL,a,0);
          }
        }
        SetDlgItemInt(hwndDlg,IDC_EDIT1,parms[0],TRUE);
        SetDlgItemInt(hwndDlg,IDC_EDIT2,parms[1],FALSE);
      }
    break;
    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        char tmp[512];

        int indev=-1, outdev=-1, offs=1, size=8;
        int r=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETCURSEL,0,0);
        if (r != CB_ERR) indev = SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETITEMDATA,r,0);
        r=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETCURSEL,0,0);
        if (r != CB_ERR)  outdev = SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETITEMDATA,r,0);

        BOOL t;
        r=GetDlgItemInt(hwndDlg,IDC_EDIT1,&t,TRUE);
        if (t) offs=r;
        r=GetDlgItemInt(hwndDlg,IDC_EDIT2,&t,FALSE);
        if (t) 
        {
          if (r<1)r=1;
          else if(r>256)r=256;
          size=r;
        }

        sprintf(tmp,"%d %d %d %d",offs,size,indev,outdev);
        lstrcpyn((char *)lParam, tmp,wParam);
        
      }
    break;
  }
  return 0;
}

static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
  return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_SURFACEEDIT_HUI),parent,dlgProc,(LPARAM)initConfigString);
}


reaper_csurf_reg_t csurf_hui_reg = 
{
  "HUI",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "HUI (partial)",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
