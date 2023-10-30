/*
** reaper_csurf
** AlphaTrack support
** Copyright (C) 2007-2008 Cockos Incorporated
** License: LGPL.
*/

#include "csurf.h"

/*
** Todo: automation status, automation mode setting using "auto" button, more
*/

static bool g_csurf_mcpmode=false; // we may wish to allow an action to set this




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



class CSurf_AlphaTrack : public IReaperControlSurface
{
  int m_midi_in_dev,m_midi_out_dev;
  midi_Output *m_midiout;
  midi_Input *m_midiin;

  int m_vol_lastpos;
  int m_alphatrack_flipmode;
  int m_alphatrack_nfingers,m_alphatrack_fingerpos,m_alphatrack_fingerposcenter;

  char m_fader_touchstate;
  int m_alpha_fx, m_alpha_fxparm;
  int m_alpha_fxcap;
  int m_bank_offset;
  int m_tranz_shiftstate;

  DWORD m_frameupd_lastrun;
  DWORD m_frameupd_lastrun2;
  int m_arrowstates,m_button_states;
  DWORD m_buttonstate_lastrun;

  double m_pan_lastpos WDL_FIXALIGN;
  double m_vol_lastpos_dbl;
  int m_pan_touchstate;
  int m_tranz_anysolo_state;
  char m_tranz_oldbuf[128];

  WDL_String descspace;
  char configtmp[1024];

  void OnMIDIEvent(MIDI_event_t *evt)
  {
    if (evt->midi_message[0] == 0xe0)
    {
      MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

      if (tr)
      {
        if (m_alphatrack_flipmode)
        {
          CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,int14ToPan(evt->midi_message[2],evt->midi_message[1]),false),NULL);
        }
        else
          CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,int14ToVol(evt->midi_message[2],evt->midi_message[1]),false),NULL);
      }
    }
    if (evt->midi_message[0] == 0xe9 && evt->midi_message[1] == 0)
    {
      m_alphatrack_fingerpos=evt->midi_message[2];
      if (m_alphatrack_fingerposcenter<0)
        m_alphatrack_fingerposcenter=evt->midi_message[2];
    }

    if (evt->midi_message[0] == 0xb0)
    {
      if (evt->midi_message[1] == 0x11 || evt->midi_message[1] == 0x12)
      {
        int adj=(evt->midi_message[2]&0x3f);
        if (evt->midi_message[2]&0x40) adj=-adj;
      
        MediaTrack *t=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
        if (m_alpha_fx<0)m_alpha_fx=0;          
        int nfx=TrackFX_GetCount(t);
        if (t && nfx>0)
        {
          if (m_alpha_fx>=nfx) m_alpha_fx=nfx-1;
          if (m_alpha_fx >= 0)
          {
            int np=TrackFX_GetNumParams(t,m_alpha_fx);
            if (m_alpha_fxparm<0)m_alpha_fxparm=0;
            else if (m_alpha_fxparm>=np) m_alpha_fxparm=np-1;

            if (evt->midi_message[1]==0x12)
            {
              double dadj=adj/16.0;
              // tweak parm
              if (m_alpha_fxcap&16) dadj*=0.1;

              double v,oov=0;
              if (m_alpha_fxparm == np-2) // bypass
              {
                v=adj>0?1.0:0.0;
              }
              else
              {

                double minval=0.0, maxval=1.0;
                oov=v=TrackFX_GetParam(t,m_alpha_fx,m_alpha_fxparm, &minval, &maxval);

                v += dadj * (maxval-minval);
                if (v<minval)v=minval;
                else if (v>maxval) v=maxval;
              }
              TrackFX_SetParam(t,m_alpha_fx,m_alpha_fxparm,v);
              if (m_alpha_fxparm != np-2) // if not bypass env
              {
                double minval,maxval;
                double ov=TrackFX_GetParam(t,m_alpha_fx,m_alpha_fxparm, &minval, &maxval);
                if ((fabs(ov-minval)<0.001 ||fabs(ov-maxval)<0.001) && fabs(oov-ov)<0.001)
                  TrackFX_SetParam(t,m_alpha_fx,m_alpha_fxparm,adj>0?maxval:minval);
              }

            }
            else if (evt->midi_message[1]==0x11)
            {
              if (m_alpha_fxcap&8)
              {
                m_alpha_fxparm=0;
                m_alpha_fx+=adj;
              }
              else
              {
                if (adj<0)
                {
                  if ((m_alpha_fxparm+=adj) < 0)
                  {
                    if (m_alpha_fx>0)
                    {
                      m_alpha_fxparm=TrackFX_GetNumParams(t,--m_alpha_fx)-1;
                      if (m_alpha_fxparm<0)m_alpha_fxparm=0;
                    }
                    else
                      m_alpha_fxparm=0;
                  }
                }
                else
                {
                  if ((m_alpha_fxparm += adj)>=np)
                  {
                    if (m_alpha_fx+1 < TrackFX_GetCount(t))
                    {
                      ++m_alpha_fx;
                      m_alpha_fxparm=0;
                    }
                    else m_alpha_fxparm=np-1;
                  }
                }
              }
            }
          }
          else if (evt->midi_message[1]==0x11) 
          { 
            m_alpha_fx += adj; m_alpha_fxparm=0; 
          }
          m_frameupd_lastrun=0; // force immediate redraw
          m_frameupd_lastrun2=0;
        }
      }
      else if (evt->midi_message[1] == 0x10)
      {
        MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

        if (tr)
        {
          double adj=(evt->midi_message[2]&0x3f);
          if (evt->midi_message[2]&0x40) adj=-adj;
          if (m_alphatrack_flipmode)
          {
            CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,adj*.5,true),NULL);
          }
          else
          {
            CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,adj*.05,true),NULL);
          }
        }
      }
    }
    else if (evt->midi_message[0] == 0x90)
    {
      bool ispress=evt->midi_message[2] == 0x7f;
      switch (evt->midi_message[1])
      {
        case 0x00: // rec arm
          if (ispress) 
          {
            if (m_tranz_shiftstate)
            {
              ClearAllRecArmed();
            }
            else
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              CSurf_SetSurfaceRecArm(tr,CSurf_OnRecArmChange(tr,-1),NULL);
            }
            m_tranz_shiftstate&=5;
          }
        break;
        case 0x32:

          if (ispress)
          {
            m_alphatrack_flipmode=!m_alphatrack_flipmode;
            if (m_midiout) m_midiout->Send(0x90, 0x32,m_alphatrack_flipmode?0x7f:0,-1);
            CSurf_ResetAllCachedVolPanStates();
            TrackList_UpdateAllExternalSurfaces();
          }

        break;

        case 0x10: // mute
          if (ispress) 
          {
            if (m_tranz_shiftstate)
            {
              MuteAllTracks(false);
            }
            else
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              if (tr) CSurf_SetSurfaceMute(tr,CSurf_OnMuteChange(tr,-1),NULL);
            }
            m_tranz_shiftstate&=5;
          }
        break;
        case 0x08: //solo
          if (ispress) 
          {
            if (m_tranz_shiftstate)
            {
              SoloAllTracks(0);
            }
            else
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              if (tr) CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,-1),NULL);
            }
            m_tranz_shiftstate&=5;
          }
        break;
        case 0x4c:
          if (ispress)
          {
            SendMessage(g_hwnd,WM_COMMAND,m_tranz_shiftstate?IDC_EDIT_REDO:IDC_EDIT_UNDO,0);
            m_tranz_shiftstate&=5;
          }
        break;
        case 0x52: // add marker
          if (ispress)
          {
            SendMessage(g_hwnd,WM_COMMAND,m_tranz_shiftstate?ID_INSERT_MARKERRGN:ID_INSERT_MARKER,0);
            m_tranz_shiftstate&=5;
          }
        break;
        case 0x54: // prev marker
          if (ispress)
            SendMessage(g_hwnd,WM_COMMAND,ID_MARKER_PREV,0);
        break;
        case 0x55:  // next marker
          if (ispress)
            SendMessage(g_hwnd,WM_COMMAND,ID_MARKER_NEXT,0);
        break;
        case 0x56:
          if (ispress)
          {
            SendMessage(g_hwnd,WM_COMMAND,IDC_REPEAT,0);
          
          }
        break;
        case 0x57:
          if (ispress) 
          {
            if (m_tranz_shiftstate)
              SendMessage(g_hwnd,WM_COMMAND,ID_LOOP_SETSTART,0);
            else
            {
              AdjustBankOffset(-1,true);
              TrackList_UpdateAllExternalSurfaces();
            }
            m_tranz_shiftstate&=5;
          }
          else m_arrowstates=0;
        break;
        case 0x58:
          if (ispress) 
          {
            if (m_tranz_shiftstate)
              SendMessage(g_hwnd,WM_COMMAND,ID_LOOP_SETEND,0);
            else
            {
              AdjustBankOffset(1,true);
              TrackList_UpdateAllExternalSurfaces();
            }
            m_tranz_shiftstate&=5;
          }
          else m_arrowstates=0;
        break;
        case 0x5d:
          if (ispress) CSurf_OnStop();
        break;
        case 0x5e:
          if (ispress) CSurf_OnPlay();
        break;
        case 0x5f:
          if (ispress) CSurf_OnRecord();
        break;
        case 0x5b:
          if (ispress && m_tranz_shiftstate)
          {
            CSurf_GoStart();
            m_button_states=0;
            m_tranz_shiftstate&=5;
          }
          else m_button_states=ispress?1:0;
        break;
        case 0x5c:
          if (ispress && m_tranz_shiftstate)
          {
            CSurf_GoEnd();
            m_button_states=0;
            m_tranz_shiftstate&=5;
          }
          else m_button_states=ispress?2:0;
        break;
        case 0x46:
          if (ispress && (m_tranz_shiftstate&4)) m_tranz_shiftstate=1; // if pressed and a stuck shift state is on, remove it
          else if (!ispress && (m_tranz_shiftstate&2)) // end of press, shift state wasnt touched, stick it
            m_tranz_shiftstate=4;
          else m_tranz_shiftstate=ispress?3:0;
          if (m_midiout) m_midiout->Send(0x90,0x46,m_tranz_shiftstate?0x7f:0,-1);
        break;
        case 0x68:
          m_fader_touchstate=ispress;
        break;
        case 0x78:
          if (!ispress) m_pan_touchstate=0;
          else if (!m_pan_touchstate) 
          {
            m_pan_touchstate=1;
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
            if (tr) 
            {
              if (m_alphatrack_flipmode)
              {
                CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,m_vol_lastpos_dbl,false),NULL);
              }
              else
              {
                CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,m_pan_lastpos,false),NULL);
              }
            }
          }
        break;
        case 0x79:
        case 0x7a:
          {
            int mask=evt->midi_message[1]==0x79 ? 1:2;
            if (ispress) 
            {
              if (mask == 2 && !(m_alpha_fxcap&mask)) 
              {                
                // first touch!
                MediaTrack *t=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
                if (m_alpha_fx<0)m_alpha_fx=0;          
                int nfx=TrackFX_GetCount(t);
                if (t && nfx>0)
                {
                  if (m_alpha_fx>=nfx) m_alpha_fx=nfx-1;
                  if (m_alpha_fx >= 0)
                  {
                    int np=TrackFX_GetNumParams(t,m_alpha_fx);
                    if (m_alpha_fxparm<0)m_alpha_fxparm=0;
                    else if (m_alpha_fxparm>=np) m_alpha_fxparm=np-1;

                    double minval=0.0, maxval=1.0;
                    double v=TrackFX_GetParam(t,m_alpha_fx,m_alpha_fxparm, &minval, &maxval);

                    TrackFX_SetParam(t,m_alpha_fx,m_alpha_fxparm,v);
                  }
                }
              }
              m_alpha_fxcap|=mask;
            }
            else 
            {
              if (mask == 2 && (m_alpha_fxcap&mask)) 
              {
                // untouch
                if (TrackFX_EndParamEdit)
                {
                  MediaTrack *t=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
                  if (m_alpha_fx<0)m_alpha_fx=0;          
                  int nfx=TrackFX_GetCount(t);
                  if (t && nfx>0)
                  {
                    if (m_alpha_fx>=nfx) m_alpha_fx=nfx-1;
                    if (m_alpha_fx >= 0)
                    {
                      int np=TrackFX_GetNumParams(t,m_alpha_fx);
                      if (m_alpha_fxparm<0)m_alpha_fxparm=0;
                      else if (m_alpha_fxparm>=np) m_alpha_fxparm=np-1;
                      TrackFX_EndParamEdit(t,m_alpha_fx,m_alpha_fxparm);
                    }
                  }
                }
              }
              m_alpha_fxcap&=~mask;
            }
          }
        break;
        case 0x74:
        case 0x7b:
          {
            int mask=(evt->midi_message[1]==0x74)?1:2;
            if (ispress)
              m_alphatrack_nfingers|=mask;
            else 
              m_alphatrack_nfingers&=~mask;

            if (!m_alphatrack_nfingers)
            {
              m_alphatrack_fingerpos=-1;
              m_alphatrack_fingerposcenter=-1;
            }
          }
        break;
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
          if (ispress)
          {
            int a=evt->midi_message[1]-0x36;
            if (m_tranz_shiftstate) a+= 4;
            MIDI_event_t evt={0,3,{0xbf,a,0}};
            kbd_OnMidiEvent(&evt,-1);
          }
        break;
        case 0x21:
        case 0x22:
          if (ispress) m_alpha_fxcap |= (8<<(evt->midi_message[1]-0x21));
          else m_alpha_fxcap &=  ~(8<<(evt->midi_message[1]-0x21));
        break;
        case 0x20:
          if (ispress)
          {
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
            if (tr)
            {
              if (m_alphatrack_flipmode)
              {
                CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,1.0,false),NULL);
              }
              else
              {
                CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,0.0,false),NULL);
              }
            }
          }
        break;
      }
    }
  }

  void AdjustBankOffset(int amt, bool dosel)
  {
    if (!amt) return;

    if (amt<0)
    {
      if (m_bank_offset>0) 
      {
        m_bank_offset += amt;
        if (m_bank_offset<0) m_bank_offset=0;

        if (dosel)
        {
          int x;
          MediaTrack *t=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
          for (x = 0; ; x ++)
          {
            int f=0;
            if (!GetTrackInfo(x-1,&f)) break;

            MediaTrack *tt=CSurf_TrackFromID(x,false);
            bool sel=tt == t;
            if (tt && !(f&2) == sel)
            {
              SetTrackSelected(tt,sel);
            }
          }
        }
      }
    }
    else
    {
      int msize=CSurf_NumTracks(g_csurf_mcpmode);

      if (m_bank_offset<msize) 
      {
        m_bank_offset += amt;
        if (m_bank_offset>msize) m_bank_offset=msize;

        if (dosel)
        {
          int x;
          MediaTrack *t=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
          for (x = 0; ; x ++)
          {
            int f=0;
            if (!GetTrackInfo(x-1,&f)) break;

            MediaTrack *tt=CSurf_TrackFromID(x,false);
            bool sel=tt == t;
            if (tt && !(f&2) == sel)
            {
              SetTrackSelected(tt,sel);
            }
          }
        }
      }
    }

  }

  void UpdateTranzDisplay(int pos, const char *text, int pad, char *oldbuf)
  {
    // compare oldbuf to text
    oldbuf += pos;

    int l=strlen(text);
    if (pad<l)l=pad;
    int ml=0x20;
    if (l > ml-pos)l=ml-pos;

    int minpos=256;
    int maxpos=0;

    const char *p=text;
    char *obp=oldbuf;
    int cnt=0;
    while (cnt < l || cnt < pad)
    {
      signed char c=cnt < l ? *p++ : ' ';
      if (c<0) c=0;
      if (c != *obp) 
      {
        if (cnt < minpos) minpos=cnt;
        if (cnt > maxpos) maxpos=cnt;
        *obp = c;
      }
      obp++;
      cnt++;
    }
    if (maxpos < minpos) return; // do nothing!

    oldbuf += minpos;
    l=(maxpos-minpos)+1;
  
    struct
    {
      MIDI_event_t evt;
      char data[512];
    }
    evt;
    evt.evt.frame_offset=0;
    evt.evt.size=0;
    unsigned char *wr = evt.evt.midi_message;
    wr[0]=0xF0;
    wr[1]=0x00;
    wr[2]=0x01;
    wr[3]=0x40;
    wr[4]=0x20;
    wr[5]=0x00;

    wr[6]=(unsigned char) (pos+minpos);
    evt.evt.size=7;

    while (l-->0) wr[evt.evt.size++]=*oldbuf++;
    wr[evt.evt.size++]=0xF7;

    m_midiout->SendMsg(&evt.evt,-1);
  }


public:
  CSurf_AlphaTrack(int indev, int outdev, int *errStats)
  {
    m_midi_in_dev=indev;
    m_midi_out_dev=outdev;
  
    m_alpha_fx=m_alpha_fxparm=0;
    m_alpha_fxcap=0;
    m_alphatrack_flipmode=0;
    m_alphatrack_nfingers=0;
    m_alphatrack_fingerposcenter=-1;
    m_alphatrack_fingerpos=-1;
    m_vol_lastpos=-1000;
    m_pan_lastpos=0;
    m_vol_lastpos_dbl=1.0;

    m_fader_touchstate=0;

    m_tranz_shiftstate=0;
    m_bank_offset=0;
    m_frameupd_lastrun=0;
    m_frameupd_lastrun2=0;
    m_arrowstates=0;
    m_button_states=0;
    m_buttonstate_lastrun=0;
    m_pan_touchstate=0;
    memset(m_tranz_oldbuf,' ',sizeof(m_tranz_oldbuf));
    m_tranz_anysolo_state=0;

    //create midi hardware access
    m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
    m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput(CreateMIDIOutput(m_midi_out_dev,false,NULL)) : NULL;

    if (errStats)
    {
      if (m_midi_in_dev >=0  && !m_midiin) *errStats|=1;
      if (m_midi_out_dev >=0  && !m_midiout) *errStats|=2;
    }

    if (m_midiin)
      m_midiin->start();

  }
  ~CSurf_AlphaTrack()
  {
    if (m_midiout)
    {
      UpdateTranzDisplay(0,"",0x20,m_tranz_oldbuf);
      Sleep(5);
    }
    DELETE_ASYNC(m_midiout);
    DELETE_ASYNC(m_midiin);
  }


  const char *GetTypeString() { return "ALPHATRACK"; }
  const char *GetDescString()
  {
    descspace.SetFormatted(512,__LOCALIZE_VERFMT("Frontier AlphaTrack (dev %d,%d)","csurf"),m_midi_in_dev,m_midi_out_dev);
    return descspace.Get();     
  }
  const char *GetConfigString() // string of configuration data
  {
    sprintf(configtmp,"0 0 %d %d",m_midi_in_dev,m_midi_out_dev);      
    return configtmp;
  }

  void CloseNoReset() 
  { 
    DELETE_ASYNC(m_midiout);
    DELETE_ASYNC(m_midiin);
    m_midiout=0;
    m_midiin=0;
  }

  void Run()
  {
    DWORD now=timeGetTime();
    if ((now-m_frameupd_lastrun2) > 10) // no reason to run this more than 100hz
    {
      if (m_alphatrack_fingerpos >=0 && m_alphatrack_fingerposcenter>=0 && m_alphatrack_nfingers && m_alphatrack_fingerposcenter != m_alphatrack_fingerpos)
      {
        if (m_alphatrack_nfingers&2) // two fingers, seek
        {
          double dpos=m_alphatrack_fingerposcenter-m_alphatrack_fingerpos;

//          if (dpos<0) dpos=-pow(-dpos,0.25);
  //        else dpos=pow(dpos,0.25);

          CSurf_ScrubAmt((dpos * (double)(now-m_frameupd_lastrun2))*-0.003);
        }
        else if (m_alphatrack_nfingers&1) //zoom
        {
          int zm = (*g_config_zoommode >= HZOOM_MOUSECUR ? HZOOM_EDITPLAYCUR : *g_config_zoommode);
          adjustZoom(((m_alphatrack_fingerposcenter-m_alphatrack_fingerpos) * (int)(now-m_frameupd_lastrun2))/-3000.0,0,true,zm);
          //
        }
      }
      m_frameupd_lastrun2=now;
    }
    if (now >= m_frameupd_lastrun+(1000/max(*g_config_csurf_rate,1)) || now < m_frameupd_lastrun-250)
    {
      if (m_midiout && m_tranz_shiftstate) 
      {
        m_midiout->Send(0x90,0x46,(now%1000)>200?0x7f:0,-1);
      }


      m_frameupd_lastrun=now;

      if (m_midiout)
      {
        if (m_tranz_anysolo_state&1)
        {
          int bla=(now%1000)>500;
          if (!!(m_tranz_anysolo_state&2) != bla)
          {
            m_tranz_anysolo_state^=2;
            m_midiout->Send(0x90,0x73,bla?0x7f:0,-1);
          }
        }
        double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();
        char timebuf[512];

        MediaTrack *t=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

        timebuf[0]=0;

        if (t && (m_alpha_fxcap&1))
        {
          // show fx/parm find
          strcpy(timebuf,"<no fx>");

          if (m_alpha_fx>=TrackFX_GetCount(t)) m_alpha_fx=TrackFX_GetCount(t)-1;
          if (m_alpha_fx<0)m_alpha_fx=0;

          int np=TrackFX_GetNumParams(t,m_alpha_fx);
          if (m_alpha_fxparm>=np) m_alpha_fxparm=np-1;
          if (m_alpha_fxparm<0)m_alpha_fxparm=0;

          char buf[32];
          buf[0]=0;
          TrackFX_GetParamName(t,m_alpha_fx,m_alpha_fxparm,buf,sizeof(buf));

          char tmpbuf[512];
          TrackFX_GetFXName(t,m_alpha_fx,tmpbuf,sizeof(tmpbuf));
          char *p=tmpbuf;
          if (p && *p && strstr(p,":"))
          {
            p=strstr(p,":");
            p++;
            while (*p == ' ') p++;
          }
          UpdateTranzDisplay(0,p,16,m_tranz_oldbuf);
          UpdateTranzDisplay(16,buf,16,m_tranz_oldbuf);
        }
        else if (t && (m_alpha_fxcap&2))
        {
          strcpy(timebuf,"<no fx>");
          if (m_alpha_fx>=TrackFX_GetCount(t)) m_alpha_fx=TrackFX_GetCount(t)-1;
          if (m_alpha_fx<0)m_alpha_fx=0;

          int np=TrackFX_GetNumParams(t,m_alpha_fx);
          if (m_alpha_fxparm>=np) m_alpha_fxparm=np-1;
          if (m_alpha_fxparm<0)m_alpha_fxparm=0;

          char buf[32];
          buf[0]=0;
          TrackFX_GetParamName(t,m_alpha_fx,m_alpha_fxparm,buf,sizeof(buf));
          double mv,mmv;
          double v=TrackFX_GetParam(t,m_alpha_fx,m_alpha_fxparm,&mv,&mmv);
          UpdateTranzDisplay(0,buf,16,m_tranz_oldbuf);
          timebuf[0]=0;
          TrackFX_GetFormattedParamValue(t,m_alpha_fx,m_alpha_fxparm,timebuf,sizeof(timebuf));
          if (!timebuf[0] && np>0) snprintf(timebuf,sizeof(timebuf),"%.3f",v);
          UpdateTranzDisplay(16,timebuf,16,m_tranz_oldbuf);
        }
        else
        {
          if (t == CSurf_TrackFromID(0,false)) sprintf(timebuf,"master");
          else if (t)
          {
            const char *name=GetTrackInfo((INT_PTR)t,NULL);
            sprintf(timebuf,"%d:%.12s",m_bank_offset,name?name:"");
          }
          UpdateTranzDisplay(0,timebuf,16,m_tranz_oldbuf);

          format_timestr_pos(pp,timebuf,sizeof(timebuf),-1);
          if (t)
          {
            {
              int showvol=m_fader_touchstate;
              int showpan=m_pan_touchstate;
              if (m_alphatrack_flipmode) 
              {
                int t=showvol;
                showvol=showpan;
                showpan=t;
              }

              double vol=1.0,pan=0.0;
              GetTrackUIVolPan(t,&vol,&pan);

              if (showvol) mkvolstr(timebuf,vol);
              else if (showpan) mkpanstr(timebuf,pan);
            }
          }
          UpdateTranzDisplay(16,timebuf,16,m_tranz_oldbuf);
        }

      }
    }
    if (m_midiin)
    {
      m_midiin->SwapBufs(timeGetTime());
      int l=0;
      MIDI_eventlist *list=m_midiin->GetReadBuf();
      MIDI_event_t *evts;
      while ((evts=list->EnumItems(&l))) OnMIDIEvent(evts);

      if (m_arrowstates||m_button_states)
      {
        DWORD now=timeGetTime();
        if ((now-m_buttonstate_lastrun) >= 100)
        {
          m_buttonstate_lastrun=now;


          if (m_arrowstates)
          {
            int iszoom=m_arrowstates&64;

            if (m_arrowstates&1) 
              CSurf_OnArrow(0,!!iszoom);
            if (m_arrowstates&2) 
              CSurf_OnArrow(1,!!iszoom);
            if (m_arrowstates&4) 
              CSurf_OnArrow(2,!!iszoom);
            if (m_arrowstates&8) 
              CSurf_OnArrow(3,!!iszoom);

          }

          if ((m_button_states&3) != 3)
          {
            if (m_button_states&1)
            {
              CSurf_OnRewFwd(1,-1);
            }
            else if (m_button_states&2)
            {
              CSurf_OnRewFwd(1,1);
            }
          }

        }
      }
    }
  }

  void SetTrackListChange() { } // not used

#define FIXID(id) const int oid=CSurf_TrackToID(trackid,g_csurf_mcpmode); const int id = oid - m_bank_offset;

  void SetSurfaceVolume(MediaTrack *trackid, double volume) 
  {
    FIXID(id)
    if (m_midiout && !id)
    {
      m_vol_lastpos_dbl = volume;
      if (!m_alphatrack_flipmode)
      {
        int volint=volToInt14(volume);
        if (m_vol_lastpos!=volint)
        {
          m_vol_lastpos=volint;
          m_midiout->Send(0xe0,volint&127,volint>>7,-1);        
        }
      }
    }
  }
  void SetSurfacePan(MediaTrack *trackid, double pan) 
  {
    FIXID(id)
    if (m_midiout && !id)
    {
      m_pan_lastpos=pan;
      if (m_alphatrack_flipmode)
      {
        int panint=panToInt14(pan);
        if (m_vol_lastpos!=panint)
        {
          m_vol_lastpos=panint;
          m_midiout->Send(0xe0,panint&127,panint>>7,-1);    
        }
      }
    }
  }
  void SetSurfaceMute(MediaTrack *trackid, bool mute) 
  { 
    FIXID(id)
    if (m_midiout && !id) m_midiout->Send(0x90,0x10,mute?0x7f:0,-1);
  }
  void SetSurfaceSelected(MediaTrack *trackid, bool selected) 
  {
    // not used
  }
  void SetSurfaceSolo(MediaTrack *trackid, bool solo) 
  { 
    FIXID(id)
    if (m_midiout)
    {
      if (!oid) m_midiout->Send(0x90, 0x73,(m_tranz_anysolo_state=!!solo)?0x7f:0,-1);
      
      if (!id) m_midiout->Send(0x90,0x08,solo?0x7f:0,-1);
    }
  }
  void SetSurfaceRecArm(MediaTrack *trackid, bool recarm) 
  { 
    FIXID(id)
    if (m_midiout)
    {
      if (!id)
        m_midiout->Send(0x90,0x00,recarm?0x7f:0,-1);

    }
  }
  void SetPlayState(bool play, bool pause, bool rec) 
  { 
    if (m_midiout) m_midiout->Send(0x90,0x5f,rec ? 0x7f:0,-1);
  }
  void SetRepeatState(bool rep) 
  { 
    if (m_midiout) m_midiout->Send(0x90,0x56,rep ? 0x7f:0,-1);
  }

  void SetTrackTitle(MediaTrack *trackid, const char *title) { }

  bool GetTouchState(MediaTrack *trackid, int isPan) 
  { 
    if (isPan != 0 && isPan != 1) return false;

    FIXID(id)
    if (!id)
    {
      if (!m_alphatrack_flipmode != !isPan) 
      {
        if (m_pan_touchstate)
        {
          return true;
        }
        return false;
      }
      return !!m_fader_touchstate;
    }
    return false;
  }

  void SetAutoMode(int mode) { }

  void ResetCachedVolPanStates() 
  { 
    m_vol_lastpos=-1000;
  }
  void OnTrackSelection(MediaTrack *trackid) 
  { 
    int newpos=CSurf_TrackToID(trackid,g_csurf_mcpmode);
    if (newpos>=0 && newpos != m_bank_offset)
    {
      AdjustBankOffset(newpos-m_bank_offset,false);
      TrackList_UpdateAllExternalSurfaces();
    }
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
  parms[0]=0;
  parms[1]=9;
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

  return new CSurf_AlphaTrack(parms[2],parms[3],errStats);
}


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        int parms[4];
        parseParms((const char *)lParam,parms);

        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT1),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT1_LBL),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT2),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT2_LBL),SW_HIDE);
        ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT2_LBL2),SW_HIDE);

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
      }
    break;
    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        char tmp[512];

        int indev=-1, outdev=-1;
        int r=SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETCURSEL,0,0);
        if (r != CB_ERR) indev = SendDlgItemMessage(hwndDlg,IDC_COMBO2,CB_GETITEMDATA,r,0);
        r=SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETCURSEL,0,0);
        if (r != CB_ERR)  outdev = SendDlgItemMessage(hwndDlg,IDC_COMBO3,CB_GETITEMDATA,r,0);

        sprintf(tmp,"0 0 %d %d",indev,outdev);
        lstrcpyn((char *)lParam, tmp,wParam);
        
      }
    break;
  }
  return 0;
}

static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
  return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_SURFACEEDIT_MCU),parent,dlgProc,(LPARAM)initConfigString);
}


reaper_csurf_reg_t csurf_alphatrack_reg = 
{
  "ALPHATRACK",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "Frontier AlphaTrack",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
