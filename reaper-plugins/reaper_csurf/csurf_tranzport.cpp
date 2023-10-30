/*
** reaper_csurf
** TranzPort support
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/

#include "csurf.h"


static bool g_csurf_mcpmode=false; // we may wish to allow an action to set this




class CSurf_TranzPort : public IReaperControlSurface
{
  int m_midi_in_dev,m_midi_out_dev;
  midi_Output *m_midiout;
  midi_Input *m_midiin;

  int m_tranz_shiftstate;
  int m_tranz_anysolo_state;
  int m_tranz_curmode;
  int m_arrowstates,m_button_states;
  int m_bank_offset;
  DWORD m_pan_lasttouch,m_vol_lasttouch;
  DWORD m_buttonstate_lastrun;
  DWORD m_frameupd_lastrun;
  char m_tranz_oldbuf[128];

  WDL_String descspace;
  char configtmp[1024];

  void OnMIDIEvent(MIDI_event_t *evt)
  {
    if (evt->midi_message[0] == 0xb0)
    {
      if (evt->midi_message[1] == 0x3c)
      {
        int v=evt->midi_message[2]&0x3f;
        if (evt->midi_message[2]&0x40) v=-v;
        if (v)
        {
          if (m_tranz_shiftstate)
          {
            if (v > 0)
            {
              m_tranz_curmode++;
              if (m_tranz_curmode>2)m_tranz_curmode=0;
            }
            else if (v < 0)
            {
              m_tranz_curmode--;
              if (m_tranz_curmode<0)m_tranz_curmode=2;
            }
          }
          else
          {
            if (m_tranz_curmode==0)
            {
              // seek
              CSurf_OnRewFwd(128,v);
            }
            else if (m_tranz_curmode==1)
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              m_vol_lasttouch=timeGetTime();
              if (tr)
                CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,v/3.0,true),this);
            }
            else if (m_tranz_curmode == 2)
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              m_pan_lasttouch=timeGetTime();
              if (tr) CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,v/20.0,true),this);
            }
          }
          m_tranz_shiftstate&=5;
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
            if (m_tranz_shiftstate) m_arrowstates=64|8;
            else
            {
              SendMessage(g_hwnd,WM_COMMAND,ID_LOOP_SETSTART,0);
              // set loop region to start at this position
            }
            m_tranz_shiftstate&=5;
          }
          else m_arrowstates=0;
        break;
        case 0x58:
          if (ispress) 
          {
            if (m_tranz_shiftstate) m_arrowstates=64|4;
            else
            {
              // set loop region to end at this position
              SendMessage(g_hwnd,WM_COMMAND,ID_LOOP_SETEND,0);
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
        case 0x30: // prev track
          if (ispress)
          {
            if (m_tranz_shiftstate)
            {
              m_tranz_curmode--;
              if (m_tranz_curmode<0)m_tranz_curmode=2;
            }
            else 
            {
              AdjustBankOffset(-1,true);
              TrackList_UpdateAllExternalSurfaces();
            }
          }
        break;
        case 0x31: // next track
          if (ispress)
          {
            if (m_tranz_shiftstate)
            {
              m_tranz_curmode++;
              if (m_tranz_curmode>2)m_tranz_curmode=0;
            }
            else 
            {
              AdjustBankOffset(1,true);
              TrackList_UpdateAllExternalSurfaces();
            }
          }
        break;
        case 0x79:
          m_tranz_shiftstate=ispress;
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
    int ml=0x28; // alphatrack is 0x20, tranzport 0x28
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
    unsigned char *wr = evt.evt.midi_message;
    wr[0]=0xF0;
    wr[1]=0x00;
    wr[2]=0x01;
    wr[3]=0x40;
    wr[4]=0x10;
    wr[5]=0x00;
    wr[6]=(unsigned char)(pos+minpos);
    evt.evt.size=7;

    while (l-->0) wr[evt.evt.size++]=*oldbuf++;
    wr[evt.evt.size++]=0xF7;

    m_midiout->SendMsg(&evt.evt,-1);
  }


public:
  CSurf_TranzPort(int indev, int outdev, int *errStats)
  {
    m_midi_in_dev=indev;
    m_midi_out_dev=outdev;
  
    memset(m_tranz_oldbuf,' ',sizeof(m_tranz_oldbuf));
    m_frameupd_lastrun=0;
    m_bank_offset=0;
    m_pan_lasttouch=0;
    m_vol_lasttouch=0;
    m_tranz_shiftstate=0;
    m_tranz_curmode=0;
    m_tranz_anysolo_state=0;
    m_arrowstates=0;
    m_button_states=0;
    m_buttonstate_lastrun=0;

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
  ~CSurf_TranzPort()
  {
    if (m_midiout)
    {
      UpdateTranzDisplay(0,"",0x28,m_tranz_oldbuf);
      Sleep(5);
    }
    DELETE_ASYNC(m_midiout);
    DELETE_ASYNC(m_midiin);
  }


  const char *GetTypeString() { return "TRANZPORT"; }
  const char *GetDescString()
  {
    descspace.SetFormatted(512,__LOCALIZE_VERFMT("Frontier TranzPort (dev %d,%d)","csurf"), m_midi_in_dev,m_midi_out_dev);
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

    if ((now - m_frameupd_lastrun) >= (1000/max(*g_config_csurf_rate,1)))
    {
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

        memset(timebuf,' ',40);
        timebuf[40]=0;

        if (t == CSurf_TrackFromID(0,false)) memcpy(timebuf,"master",6);
        else if (t)
        {
          char tmp[64];
          const char *name=GetTrackInfo((INT_PTR)t,NULL);
          sprintf(tmp,"%d)%.9s",m_bank_offset,name?name:"");
          memcpy(timebuf,tmp,strlen(tmp));
        }

        if (t)
        {
          double vol=1.0,pan=0.0;
          GetTrackUIVolPan(t,&vol,&pan);

          double dv=VAL2DB(vol);
          char vb[128];
          if (m_tranz_curmode==1) vb[0]=0x7e;
          else vb[0]=' ';
          char pb[64];
          if (m_tranz_curmode==2) pb[0]=0x7e;
          else pb[0]=' ';

          if (fabs(dv)>=10.0) snprintf(vb+1,sizeof(vb)-1,"%+.0f",dv);
          else snprintf(vb+1,sizeof(vb)-1,"%+.1f",dv);

          mkpanstr(pb+1,pan);
          if (!strnicmp(pb+1,"cent",4)) strcpy(pb+1,"ctr");
          else if (strstr(pb+1,"%"))
          {
            char *p=strstr(pb+1,"%");
            p[0]=p[1];
            p[1]=0;
          }          
          strcat(vb,pb);

          if (strlen(vb)<20)
            memcpy(timebuf+20-strlen(vb),vb,strlen(vb));

        }


        char buf[256];

        format_timestr_pos(pp,buf,sizeof(buf),-1);

        char *p;
        int x;

        memset(timebuf+20,' ',20);
        if (strlen(buf)>=13)
          memcpy(p=timebuf+20+7,buf,13);
        else memcpy(p=timebuf+20+20-strlen(buf),buf,strlen(buf));
        if (m_tranz_curmode==0) p[-1]=0x7e;
      
        // vu meter!
        int v[2]={0,0};
        if (t)
        {
          double p1,p2;
        
          p1=VAL2DB(Track_GetPeakInfo(t,0));
          p2=VAL2DB(Track_GetPeakInfo(t,1));
          v[0]=(int)((p1+60.0)/5.0);
          v[1]=(int)((p2+60.0)/5.0);
        }
        for (x = 0; x < 6; x ++)
        {
          int pos1=v[0]-x*2;
          int pos2=v[1]-x*2;
          if (pos1>2)pos1=2;
          else if (pos1<0)pos1=0;
          if (pos2>2)pos2=2;
          else if (pos2<0)pos2=0;

          char c;
          if (!pos1 && !pos2) continue;
        
          if (pos1)
          {
            if (pos2==0) c=pos1-1;
            else if (pos2==1) c=2+pos1;
            else c=5+pos1;
          }
          else 
          {
            if (pos2==1) c=2;
            else c=5;
          }
          if (!c)c=-128;


          timebuf[20+x]=c;
        }
        if (v[0]>=12 || v[1]>=12)
          timebuf[20+x]='*';
        else
          timebuf[20+x]='|';
        timebuf[40]=0;
      
        UpdateTranzDisplay(0,timebuf,40,m_tranz_oldbuf);

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

#define FIXID(id) const int oid=CSurf_TrackToID(trackid,g_csurf_mcpmode); const int id=oid - m_bank_offset;

  void SetSurfaceVolume(MediaTrack *trackid, double volume) 
  {
  }
  void SetSurfacePan(MediaTrack *trackid, double pan) 
  {
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
      DWORD lt=m_vol_lasttouch;
      if (isPan==1) lt=m_pan_lasttouch;
      if ((timeGetTime()-lt) < 3000) // fake touch, go for 3s after last movement
        return true;
    }
    return false;
  }

  void SetAutoMode(int mode) { }

  void ResetCachedVolPanStates() 
  { 
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

  return new CSurf_TranzPort(parms[2],parms[3],errStats);
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


reaper_csurf_reg_t csurf_tranzport_reg = 
{
  "TRANZPORT",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "Frontier Tranzport",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
