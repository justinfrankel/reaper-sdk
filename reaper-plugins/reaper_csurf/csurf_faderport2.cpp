// included by csurf_faderport.cpp until we are less lazy

class CSurf_FaderPort_FP2 : public IReaperControlSurface
{
  int m_midi_in_dev,m_midi_out_dev;
  midi_Output *m_midiout;
  midi_Input *m_midiin;

  int m_vol_lastpos;
  int m_flipmode;
  int m_faderport_buttonstates;

  char m_fader_touchstate;
  int m_bank_offset;

  DWORD m_frameupd_lastrun;
  int m_button_states;
  DWORD m_buttonstate_lastrun;
  DWORD m_pan_lasttouch;

  WDL_String descspace;
  char configtmp[128];

  void OnMIDIEvent(MIDI_event_t *evt)
  {
    if (evt->midi_message[0] == 0xb0 && evt->midi_message[1] == 0x10)
    {
      m_pan_lasttouch=timeGetTime();

      double adj=0.02 * (evt->midi_message[2]&0x3f);
      if (evt->midi_message[2]&0x40) adj=-adj;
      MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

      if (tr)
      {
        if (m_flipmode)
        {
          CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,adj*11.0,true),NULL);
        }
        else
          CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,adj,true),NULL);
      }
    }
    else if (evt->midi_message[0] == 0xe0)
    {
      MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

      if (tr)
      {
        if (m_flipmode)
        {
          CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,int14ToPan(evt->midi_message[2],evt->midi_message[1]),false),NULL);
        }
        else
          CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,int14ToVol(evt->midi_message[2],evt->midi_message[1]),false),NULL);
      }
    }
    else if (evt->midi_message[0] == 0x90)
    {
      const bool ishit = evt->midi_message[2] == 0x7f;
      bool sendback = false;
      switch (evt->midi_message[1])
      {
        case 0x68:
          if (ishit && !m_fader_touchstate && m_vol_lastpos>=0)
          {
            m_fader_touchstate=true;
            // this is a hack which might be better done within reaper
            // -- if a control surface enables touch without immediately
            // notifying of the controller position, then automation recording
            // is compromised. need to test an alphatrack / real MCU perhaps.
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
            if (tr)
            {
              if (m_flipmode)
                CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,int14ToPan(m_vol_lastpos>>7,m_vol_lastpos&127),false),this);
              else
                CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,int14ToVol(m_vol_lastpos>>7,m_vol_lastpos&127),false),this);
            }
          }
          m_fader_touchstate=ishit;
        break;
        case 0x46: // shift
          if (ishit)
            m_faderport_buttonstates|=2;
          else
            m_faderport_buttonstates&=~2;
          sendback=true;
        break;
        case 0x00: // rec arm key push
          if (ishit)
          {
            if (m_faderport_buttonstates&2)
              ClearAllRecArmed();
            else
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              if (tr) SetSurfaceRecArm(tr,CSurf_OnRecArmChange(tr,-1));
            }
          }
        break;
        case 0x03: // automation off
          if (ishit)
          {
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

            if (tr)
            {
              SetTrackAutomationMode(tr,0);
              CSurf_SetAutoMode(-1,NULL);
            }
          }
        break;
        case 0x4D: // automation touch/latch
          if (ishit)
          {
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

            if (tr)
            {
              SetTrackAutomationMode(tr, (m_faderport_buttonstates&2) ? 4 : 2);
              CSurf_SetAutoMode(-1,NULL);
            }
          }
        break;
        case 0x4B: // automation write/latch preview
          if (ishit)
          {
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

            if (tr)
            {
              SetTrackAutomationMode(tr,(m_faderport_buttonstates&2) ? 5 : 3);
              CSurf_SetAutoMode(-1,NULL);
            }
          }
        break;
        case 0x4A: // automation read/clear
          if (ishit)
          {
            MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);

            if (tr)
            {
              SetTrackAutomationMode(tr,(m_faderport_buttonstates&2) ? 0 : 1);
              CSurf_SetAutoMode(-1,NULL);
            }
          }
        break;
        case 0x5B: // rew
          m_button_states&=~1;
          if (ishit)
          {
            if ((m_faderport_buttonstates&2) || (m_button_states&2))
            {
              m_button_states &= ~2;
              CSurf_GoStart();
            }
            else
              m_button_states|=1;
          }
          sendback=true;
        break;
        case 0x5C: // fwd
          m_button_states&=~2;
          if (ishit)
          {
            if (m_button_states&1)
            {
              m_button_states &= ~1;
              CSurf_GoStart();
            }
            else if (m_faderport_buttonstates&2)
            {
              CSurf_GoEnd();
            }
            else m_button_states|=2;
          }
          sendback=true;
        break;
        case 0x5D: // stop
          if (ishit)
          {
            CSurf_OnStop();
          }
        break;
        case 0x56:
          if (ishit)
          {
            if ((m_faderport_buttonstates&2))
            {
              SendMessage(g_hwnd,WM_COMMAND,ID_INSERT_MARKER,0);
            }
            else
              SendMessage(g_hwnd,WM_COMMAND,IDC_REPEAT,0);
          }
        break;
        case 0x5E:
          if (ishit) CSurf_OnPlay();
        break;
        case 0x5F:
          if (ishit) CSurf_OnRecord();
        break;
        case 0x2E:
          if (ishit)
          {
            if (m_faderport_buttonstates&2)
              SendMessage(g_hwnd,WM_COMMAND,IDC_EDIT_UNDO,0);
            else
            {
              AdjustBankOffset((m_faderport_buttonstates&1)?-8:-1,true);
              TrackList_UpdateAllExternalSurfaces();
            }
          }
        break;
        case 0x2F:
          if (ishit)
          {
            if (m_faderport_buttonstates&2)
              SendMessage(g_hwnd,WM_COMMAND,IDC_EDIT_REDO,0);
            else
            {
              AdjustBankOffset((m_faderport_buttonstates&1)?8:1,true);
              TrackList_UpdateAllExternalSurfaces();
            }
          }
        break;
        case 0x2A:
          if (ishit)
          {
            m_flipmode=!m_flipmode;
            if (m_midiout) m_midiout->Send(0x90, 0x2A,m_flipmode?1:0,-1);
            CSurf_ResetAllCachedVolPanStates();
            TrackList_UpdateAllExternalSurfaces();
          }
        break;
        case 0x08:
          if (ishit)
          {
            if (m_faderport_buttonstates&2)
              SoloAllTracks(0);
            else
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              if (tr) SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,-1));
            }
          }
        break;
        case 0x10:
          if (ishit)
          {
            if (m_faderport_buttonstates&2)
              MuteAllTracks(false);
            else
            {
              MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
              if (tr) SetSurfaceMute(tr,CSurf_OnMuteChange(tr,-1));
            }
          }
        break;
        case 0x66:
          {
            MIDI_event_t e={0,3,{0x97,evt->midi_message[1],evt->midi_message[2]}};
            kbd_OnMidiEvent(&e,-1);
          }
        break;
        default:
          if (ishit)
          {
            int a=evt->midi_message[1],b = 0xbe;
            if (m_faderport_buttonstates&2) b++;
            MIDI_event_t evt={0,3,{b,a,0}};
            kbd_OnMidiEvent(&evt,-1);
          }
        break;
      }
      if (sendback && m_midiout) m_midiout->Send(evt->midi_message[0],evt->midi_message[1],evt->midi_message[2],-1);
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



public:
  CSurf_FaderPort_FP2(int indev, int outdev, int *errStats)
  {
    m_midi_in_dev=indev;
    m_midi_out_dev=outdev;


    m_faderport_buttonstates=0;
    m_flipmode=0;
    m_vol_lastpos=-1000;

    m_fader_touchstate=0;

    m_bank_offset=0;
    m_frameupd_lastrun=0;
    m_button_states=0;
    m_buttonstate_lastrun=0;
    m_pan_lasttouch=0;

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

    if (m_midiout)
    {
      LightsOut();
    }

  }
  ~CSurf_FaderPort_FP2()
  {
    LightsOut();
    if (m_midiout)
    {
      Sleep(5);
    }

    delete m_midiout;
    delete m_midiin;
  }

  void LightsOut()
  {
    if (m_midiout)
      for (int x=0;x<=0x5F;x++)
        m_midiout->Send(0x90,x,0,-1);
  }


  const char *GetTypeString() { return "FADERPORT2"; }
  const char *GetDescString()
  {
    descspace.SetFormatted(512,__LOCALIZE_VERFMT("PreSonus FaderPort FP2 (dev %d,%d)","csurf"),m_midi_in_dev,m_midi_out_dev);
    return descspace.Get();
  }
  const char *GetConfigString() // string of configuration data
  {
    snprintf(configtmp,sizeof(configtmp),"0 0 %d %d",m_midi_in_dev,m_midi_out_dev);
    return configtmp;
  }

  void CloseNoReset()
  {
    delete m_midiout;
    delete m_midiin;
    m_midiout=0;
    m_midiin=0;
  }

  void Run()
  {
    if (m_midiin)
    {
      m_midiin->SwapBufs(timeGetTime());
      int l=0;
      MIDI_eventlist *list=m_midiin->GetReadBuf();
      MIDI_event_t *evts;
      while ((evts=list->EnumItems(&l))) OnMIDIEvent(evts);

      if (m_button_states)
      {
        DWORD now=timeGetTime();
        if ((now-m_buttonstate_lastrun) >= 100)
        {
          m_buttonstate_lastrun=now;

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

  void SetTrackListChange()
  {
    SetAutoMode(0);
  }

#define FIXID(id) const int oid=CSurf_TrackToID(trackid,g_csurf_mcpmode); const int id=oid - m_bank_offset;

  void SetSurfaceVolume(MediaTrack *trackid, double volume)
  {
    FIXID(id)
    if (m_midiout && !id && !m_flipmode)
    {
      int volint=volToInt14(volume);

      if (m_vol_lastpos!=volint)
      {
        m_vol_lastpos=volint;
        m_midiout->Send(0xe0,volint&127,volint>>7,-1);
      }
    }
  }
  void SetSurfacePan(MediaTrack *trackid, double pan)
  {
    FIXID(id)
    if (m_midiout && !id && m_flipmode)
    {
      int volint=panToInt14(pan);

      if (m_vol_lastpos!=volint)
      {
        m_vol_lastpos=volint;
        m_midiout->Send(0xe0,volint&127,volint>>7,-1);
      }
    }
  }
  void SetSurfaceMute(MediaTrack *trackid, bool mute)
  {
    FIXID(id)
    if (!id && m_midiout)
      m_midiout->Send(0x90,0x10,mute?0x7f:0,-1);
  }
  void SetSurfaceSelected(MediaTrack *trackid, bool selected)
  {
  }
  void SetSurfaceSolo(MediaTrack *trackid, bool solo)
  {
    FIXID(id)
    if (!id && m_midiout)
      m_midiout->Send(0x90,0x08,solo?0x7f:0,-1);
  }
  void SetSurfaceRecArm(MediaTrack *trackid, bool recarm)
  {
    FIXID(id)
    if (!id && m_midiout)
      m_midiout->Send(0x90,0x0,recarm?0x7f:0,-1);
  }
  void SetPlayState(bool play, bool pause, bool rec)
  {
    if (m_midiout)
    {
      m_midiout->Send(0x90, 0x5F,rec?pause?1:0x7f:0,-1);
      m_midiout->Send(0x90, 0x5E,play?pause?1:0x7f:0,-1);
      m_midiout->Send(0x90, 0x5D,!play&&!pause&&!rec?0x7f:0,-1);
    }
  }
  void SetRepeatState(bool rep)
  {
    if (m_midiout) m_midiout->Send(0x90, 0x56,rep?0x7f:0,-1);

  }

  void SetTrackTitle(MediaTrack *trackid, const char *title) { }

  bool GetTouchState(MediaTrack *trackid, int isPan)
  {
    if (isPan != 0 && isPan != 1) return false;

    FIXID(id)
    if (!id)
    {
      if (!m_flipmode != !isPan)
      {
        if (m_pan_lasttouch==1 || (timeGetTime()-m_pan_lasttouch) < 3000) // fake touch, go for 3s after last movement
        {
          return true;
        }
        return false;
      }
      return !!m_fader_touchstate;
    }
    return false;
  }

  void SetAutoMode(int mode)
  {
    if (m_midiout)
    {
      MediaTrack *tr=CSurf_TrackFromID(m_bank_offset,g_csurf_mcpmode);
      if (tr) mode=GetTrackAutomationMode(tr);

      if (mode<0) return;

      m_midiout->Send(0xa0,0x10,0,-1);//mode==0); // dont set off light, it disables the fader?!
      m_midiout->Send(0xa0,0xf,mode==2||mode==4,-1);
      m_midiout->Send(0xa0,0xe,mode==3,-1);
      m_midiout->Send(0xa0,0xd,mode==1,-1);
    }
  }

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


};

static IReaperControlSurface *createFunc_FP2(const char *type_string, const char *configString, int *errStats)
{
  int parms[4];
  parseParms(configString,parms);

  return new CSurf_FaderPort_FP2(parms[2],parms[3],errStats);
}

reaper_csurf_reg_t csurf_faderport2_reg =
{
  "FADERPORT2",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "PreSonus FaderPort v2 (2018)",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc_FP2,
  configFunc,
};
