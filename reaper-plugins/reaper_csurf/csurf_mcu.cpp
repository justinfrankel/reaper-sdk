/*
** reaper_csurf
** MCU support
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/


#include "csurf.h"
#include "../../WDL/ptrlist.h"

/*
MCU documentation:

MCU=>PC:
  The MCU seems to send, when it boots (or is reset) F0 00 00 66 14 01 58 59 5A 57 18 61 05 57 18 61 05 F7

  Ex vv vv    :   volume fader move, x=0..7, 8=master, vv vv is int14
  B0 1x vv    :   pan fader move, x=0..7, vv has 40 set if negative, low bits 0-31 are move amount
  B0 3C vv    :   jog wheel move, 01 or 41

  to the extent the buttons below have LEDs, you can set them by sending these messages, with 7f for on, 1 for blink, 0 for off.
  90 0x vv    :   rec arm push x=0..7 (vv:..)
  90 0x vv    :   solo push x=8..F (vv:..)
  90 1x vv    :   mute push x=0..7 (vv:..)
  90 1x vv    :   selected push x=8..F (vv:..)
  90 2x vv    :   pan knob push, x=0..7 (vv:..)
  90 28 vv    :   assignment track
  90 29 vv    :   assignment send
  90 2A vv    :   assignment pan/surround
  90 2B vv    :   assignment plug-in
  90 2C vv    :   assignment EQ
  90 2D vv    :   assignment instrument
  90 2E vv    :   bank down button (vv: 00=release, 7f=push)
  90 2F vv    :   channel down button (vv: ..)
  90 30 vv    :   bank up button (vv:..)
  90 31 vv    :   channel up button (vv:..)
  90 32 vv    :   flip button
  90 33 vv    :   global view button
  90 34 vv    :   name/value display button
  90 35 vv    :   smpte/beats mode switch (vv:..)
  90 36 vv    :   F1
  90 37 vv    :   F2
  90 38 vv    :   F3
  90 39 vv    :   F4
  90 3A vv    :   F5
  90 3B vv    :   F6
  90 3C vv    :   F7
  90 3D vv    :   F8
  90 3E vv    :   Global View : midi tracks
  90 3F vv    :   Global View : inputs
  90 40 vv    :   Global View : audio tracks
  90 41 vv    :   Global View : audio instrument
  90 42 vv    :   Global View : aux
  90 43 vv    :   Global View : busses
  90 44 vv    :   Global View : outputs
  90 45 vv    :   Global View : user
  90 46 vv    :   shift modifier (vv:..)
  90 47 vv    :   option modifier
  90 48 vv    :   control modifier
  90 49 vv    :   alt modifier
  90 4A vv    :   automation read/off
  90 4B vv    :   automation write
  90 4C vv    :   automation trim
  90 4D vv    :   automation touch
  90 4E vv    :   automation latch
  90 4F vv    :   automation group
  90 50 vv    :   utilities save
  90 51 vv    :   utilities undo
  90 52 vv    :   utilities cancel
  90 53 vv    :   utilities enter
  90 54 vv    :   marker
  90 55 vv    :   nudge
  90 56 vv    :   cycle
  90 57 vv    :   drop
  90 58 vv    :   replace
  90 59 vv    :   click
  90 5a vv    :   solo
  90 5b vv    :   transport rewind (vv:..)
  90 5c vv    :   transport ffwd (vv:..)
  90 5d vv    :   transport pause (vv:..)
  90 5e vv    :   transport play (vv:..)
  90 5f vv    :   transport record (vv:..)
  90 60 vv    :   up arrow button  (vv:..)
  90 61 vv    :   down arrow button 1 (vv:..)
  90 62 vv    :   left arrow button 1 (vv:..)
  90 63 vv    :   right arrow button 1 (vv:..)
  90 64 vv    :   zoom button (vv:..)
  90 65 vv    :   scrub button (vv:..)

  90 6x vv    :   fader touch x=8..f
  90 70 vv    :   master fader touch

PC=>MCU:

  F0 00 00 66 14 12 xx <data> F7   : update LCD. xx=offset (0-112), string. display is 55 chars wide, second line begins at 56, though.
  F0 00 00 66 14 08 00 F7          : reset MCU
  F0 00 00 66 14 20 0x 03 F7       : put track in VU meter mode, x=track  

  90 73 vv : rude solo light (vv: 7f=on, 00=off, 01=blink)

  B0 3x vv : pan display, x=0..7, vv=1..17 (hex) or so
  B0 4x vv : right to left of LEDs. if 0x40 set in vv, dot below char is set (x=0..11)

  D0 yx    : update VU meter, y=track, x=0..d=volume, e=clip on, f=clip off
  Ex vv vv : set volume fader, x=track index, 8=master


*/

#ifdef SPACELAB
#define SPLASH_MESSAGE "Spacelab Recording Studio"
#else
#define SPLASH_MESSAGE "REAPER! Initializing... Please wait..."
#endif

static double charToVol(unsigned char val)
{
  double pos=((double)val*1000.0)/127.0;
  pos=SLIDER2DB(pos);
  return DB2VAL(pos);

}

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

static double charToPan(unsigned char val)
{
  double pos=((double)val*1000.0+0.5)/127.0;

  pos=(pos-500.0)/500.0;
  if (fabs(pos) < 0.08) pos=0.0;

  return pos;
}

static unsigned char panToChar(double pan)
{
  pan = (pan+1.0)*63.5;

  if (pan<0.0)pan=0.0;
  else if (pan>127.0)pan=127.0;

  return (unsigned char)(pan+0.5);
}

/*
static unsigned int get_midi_evt_code( MIDI_event_t *evt ) {
  unsigned int code = 0;
  code |= (evt->midi_message[0]<<24);
  code |= (evt->midi_message[1]<<16);
  code |= (evt->midi_message[2]<<8);
  code |= evt->size > 3 ? evt->midi_message[3] : 0;
  return code;
}
*/

class CSurf_MCU;
static WDL_PtrList<CSurf_MCU> m_mcu_list;
static bool g_csurf_mcpmode;
static int m_flipmode;
static int m_allmcus_bank_offset;

typedef void (CSurf_MCU::*ScheduleFunc)();

struct ScheduledAction {
  ScheduledAction( DWORD time, ScheduleFunc func ) {
    this->next = NULL;
    this->time = time;
    this->func = func;
  }
  
  ScheduledAction *next;
  DWORD time;
  ScheduleFunc func;
};

#define CONFIG_FLAG_FADER_TOUCH_MODE 1
#define CONFIG_FLAG_MAPF1F8TOMARKERS 2
#define CONFIG_FLAG_NOBANKOFFSET 4

#define DOUBLE_CLICK_INTERVAL 250 /* ms */

class TrackIterator {
  int m_index;
  int m_len;
public:
  TrackIterator() {
    m_index = 1;
    m_len = CSurf_NumTracks(false);
  }
  MediaTrack* operator*() {
    return CSurf_TrackFromID(m_index,false);
  }
  TrackIterator &operator++() {
    if ( m_index <= m_len ) ++m_index;
    return *this;
  }
  bool end() {
    return m_index > m_len;
  }
};

MediaTrack* TrackFromGUID( const GUID &guid ) {
  for ( TrackIterator ti; !ti.end(); ++ti ) {
    MediaTrack *tr = *ti;
    const GUID *tguid=GetTrackGUID(tr);
    
    if (tr && tguid && !memcmp(tguid,&guid,sizeof(GUID)))
      return tr;
  }
  return NULL;
}

struct SelectedTrack {
  SelectedTrack( const GUID *guid ) {
    this->next = NULL;
    this->guid = *guid;
  }
  MediaTrack *track() {
    return TrackFromGUID( this->guid );
  }
  SelectedTrack *next;
  GUID guid;
};

class CSurf_MCU : public IReaperControlSurface
{
    bool m_is_mcuex;
    int m_midi_in_dev,m_midi_out_dev;
    int m_offset, m_size;
    midi_Output *m_midiout;
    midi_Input *m_midiin;

    int m_vol_lastpos[256];
    int m_pan_lastpos[256];
    char m_mackie_lasttime[10];
    int m_mackie_lasttime_mode;
    int m_mackie_modifiers;
    int m_cfg_flags;  //CONFIG_FLAG_FADER_TOUCH_MODE etc
    int m_last_miscstate; // &1=metronome

    char m_fader_touchstate[256];
    unsigned int m_fader_lasttouch[256]; // m_fader_touchstate changes will clear this, moves otherwise set it. if set to -1, then totally disabled
    unsigned int m_pan_lasttouch[256];

    WDL_String m_descspace;
    char m_configtmp[1024];

    double m_mcu_meterpos[8];
    DWORD m_mcu_timedisp_lastforce, m_mcu_meter_lastrun;
    int m_mackie_arrow_states;
    unsigned int m_buttonstate_lastrun;
    unsigned int m_frameupd_lastrun;
    ScheduledAction *m_schedule;
    SelectedTrack *m_selected_tracks;
    
    // If user accidentally hits fader, we want to wait for user
    // to stop moving fader and then reset it to it's orginal position
    #define FADER_REPOS_WAIT 250
    bool m_repos_faders;
    DWORD m_fader_lastmove;
    
    int m_button_last;
    DWORD m_button_last_time;
    
    void ScheduleAction( DWORD time, ScheduleFunc func ) {
      ScheduledAction *action = new ScheduledAction( time, func );
      // does not handle wrapping timestamp
      if ( m_schedule == NULL ) {
        m_schedule = action;
      }
      else if ( action->time < m_schedule->time ) {
        action->next = m_schedule;
        m_schedule = action;
      }
      else {
        ScheduledAction *curr = m_schedule;
        while( curr->next != NULL && curr->next->time < action->time )
          curr = curr->next;
        action->next = curr->next;
        curr->next = action;
      }
    }

    int GetBankOffset() const
    {
      return (m_cfg_flags&CONFIG_FLAG_NOBANKOFFSET) ? (m_offset + 1) : (m_offset + 1 + m_allmcus_bank_offset);
    }
    
    void MCUReset()
    {
      memset(m_mackie_lasttime,0,sizeof(m_mackie_lasttime));
      memset(m_fader_touchstate,0,sizeof(m_fader_touchstate));
      memset(m_fader_lasttouch,0,sizeof(m_fader_lasttouch));
      memset(m_pan_lasttouch,0,sizeof(m_pan_lasttouch));
      m_mackie_lasttime_mode=-1;
      m_mackie_modifiers=0;
      m_last_miscstate=0;
      m_buttonstate_lastrun=0;
      m_mackie_arrow_states=0;

      memset(m_vol_lastpos,0xff,sizeof(m_vol_lastpos));
      memset(m_pan_lastpos,0xff,sizeof(m_pan_lastpos));


      if (m_midiout)
      {
        if (!m_is_mcuex)
        {
          m_midiout->Send(0x90, 0x32,m_flipmode?1:0,-1);
          m_midiout->Send(0x90, 0x33,g_csurf_mcpmode?0x7f:0,-1);
    
          m_midiout->Send(0x90, 0x64,(m_mackie_arrow_states&64)?0x7f:0,-1);
          m_midiout->Send(0x90, 0x65,(m_mackie_arrow_states&128)?0x7f:0,-1);

          m_midiout->Send(0xB0,0x40+11,'0'+(((m_allmcus_bank_offset+1)/10)%10),-1);
          m_midiout->Send(0xB0,0x40+10,'0'+((m_allmcus_bank_offset+1)%10),-1);
        }

        UpdateMackieDisplay(0,SPLASH_MESSAGE,56*2);

        int x;
        for (x = 0; x < 8; x ++)
        {
          struct
          {
            MIDI_event_t evt;
            char data[9];
          }
          evt;
          evt.evt.frame_offset=0;
          evt.evt.size=9;
          unsigned char *wr = evt.evt.midi_message;
          wr[0]=0xF0;
          wr[1]=0x00;
          wr[2]=0x00;
          wr[3]=0x66;
          wr[4]=m_is_mcuex ? 0x15 : 0x14;
          wr[5]=0x20;
          wr[6]=0x00+x;
          wr[7]=0x03;
          wr[8]=0xF7;
          Sleep(5);
          m_midiout->SendMsg(&evt.evt,-1);
        }
        Sleep(5);
        for (x = 0; x < 8; x ++)
        {
          m_midiout->Send(0xD0,(x<<4)|0xF,0,-1);
        }
      }

    }


    void UpdateMackieDisplay(int pos, const char *text, int pad)
    {
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
      wr[2]=0x00;
      wr[3]=0x66;
      wr[4]=m_is_mcuex ? 0x15 :  0x14;
      wr[5]=0x12;
      wr[6]=(unsigned char)pos;
      evt.evt.size=7;

      int l=strlen(text);
      if (pad<l)l=pad;
      if (l > 200)l=200;

      int cnt=0;
      while (cnt < l)
      {
        wr[evt.evt.size++]=*text++;
        cnt++;
      }
      while (cnt++<pad)  wr[evt.evt.size++]=' ';
      wr[evt.evt.size++]=0xF7;
      Sleep(5);
      m_midiout->SendMsg(&evt.evt,-1);
    }

    typedef bool (CSurf_MCU::*MidiHandlerFunc)(MIDI_event_t*);
    
    bool OnMCUReset(MIDI_event_t *evt) {
      unsigned char onResetMsg[]={0xf0,0x00,0x00,0x66,0x14,0x01,0x58,0x59,0x5a,};
      onResetMsg[4]=m_is_mcuex ? 0x15 : 0x14; 
      if (evt->midi_message[0]==0xf0 && evt->size >= sizeof(onResetMsg) && !memcmp(evt->midi_message,onResetMsg,sizeof(onResetMsg)))
      {
        // on reset
        MCUReset();
        TrackList_UpdateAllExternalSurfaces();
        return true;
      }
      return false;
    }
    
    bool OnFaderMove(MIDI_event_t *evt) {
      if ((evt->midi_message[0]&0xf0) == 0xe0) // volume fader move
      {
        m_fader_lastmove = timeGetTime();

        int tid=evt->midi_message[0]&0xf;
        if (tid>=0&&tid<9 && m_fader_lasttouch[tid]!=0xffffffff)
          m_fader_lasttouch[tid]=m_fader_lastmove;

        if (tid == 8) tid=0; // master offset, master=0
        else tid+=GetBankOffset();

        MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);

        if (tr)
        {
          if ( (m_cfg_flags&CONFIG_FLAG_FADER_TOUCH_MODE) && !GetTouchState(tr) ) {
            m_repos_faders = true;
          }
          else if (m_flipmode)
          {
            CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,int14ToPan(evt->midi_message[2],evt->midi_message[1]),false),NULL);
          }
          else
            CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,int14ToVol(evt->midi_message[2],evt->midi_message[1]),false),NULL);
        }
        return true;
      } 
      return false;
    }

	bool OnRotaryEncoder( MIDI_event_t *evt ) {
	  if ( (evt->midi_message[0]&0xf0) == 0xb0 && 
	      evt->midi_message[1] >= 0x10 && 
	      evt->midi_message[1] < 0x18 ) // pan
	  {
	    int tid=evt->midi_message[1]-0x10;

	    m_pan_lasttouch[tid&7]=timeGetTime();

	    if (tid == 8) tid=0; // adjust for master
	    else tid+=GetBankOffset();
	    MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
	    if (tr)
	    {
	      double adj=(evt->midi_message[2]&0x3f)/31.0;
	      if (evt->midi_message[2]&0x40) adj=-adj;
	      if (m_flipmode)
	      {
	        CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,adj*11.0,true),NULL);
	      }
	      else
	      {
	        CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,adj,true),NULL);
	      }
	    }
	    return true;
	  }
	  return false;
	}
	
	bool OnJogWheel( MIDI_event_t *evt ) {
    if ( (evt->midi_message[0]&0xf0) == 0xb0 &&
         evt->midi_message[1] == 0x3c ) // jog wheel
     {
       if (evt->midi_message[2] >= 0x41)  
         CSurf_OnRewFwd(m_mackie_arrow_states&128, 0x40 - (int)evt->midi_message[2]);
       else if (evt->midi_message[2] > 0 && evt->midi_message[2] < 0x40)  
         CSurf_OnRewFwd(m_mackie_arrow_states&128, evt->midi_message[2]);
       return true;
     }
    return false;
	}
	
	bool OnAutoMode( MIDI_event_t *evt ) {
    #if 0
	  UpdateMackieDisplay( 0, "ok", 2 );
    #endif

	  int mode=-1;
	  int a=evt->midi_message[1]-0x4a;
	  if (!a) mode=AUTO_MODE_READ;
	  else if (a==1) mode=AUTO_MODE_WRITE;
	  else if (a==2) mode=AUTO_MODE_TRIM;
	  else if (a==3) mode=AUTO_MODE_TOUCH;
	  else if (a==4) mode=AUTO_MODE_LATCH;

	  if (mode>=0)
	    SetAutomationMode(mode,!IsKeyDown(VK_CONTROL));

	  return true;
	}
	
	bool OnBankChannel( MIDI_event_t *evt ) {
	  int maxfaderpos=0;
	  int movesize=8;
	  int x;
	  for (x = 0; x < m_mcu_list.GetSize(); x ++)
	  {
	    CSurf_MCU *item=m_mcu_list.Get(x);
	    if (item)
	    {
	      if (item->m_offset+8 > maxfaderpos)
	        maxfaderpos=item->m_offset+8;
	    }
	  }

	  if (evt->midi_message[1]>=0x30) movesize=1;
	  else  movesize=8; // maxfaderpos?


	  if (evt->midi_message[1] & 1) // increase by X
	  {
	    int msize=CSurf_NumTracks(g_csurf_mcpmode);
	    if (movesize>1)
	    {
	      if (m_allmcus_bank_offset+maxfaderpos >= msize) return true;
	    }

	    m_allmcus_bank_offset+=movesize;

	    if (m_allmcus_bank_offset >= msize) m_allmcus_bank_offset=msize-1;
	  }
	  else
	  {
	    m_allmcus_bank_offset-=movesize;
	    if (m_allmcus_bank_offset<0)m_allmcus_bank_offset=0;
	  }
	  // update all of the sliders
	  TrackList_UpdateAllExternalSurfaces();

	  for (x = 0; x < m_mcu_list.GetSize(); x ++)
	  {
	    CSurf_MCU *item=m_mcu_list.Get(x);
	    if (item && !item->m_is_mcuex && item->m_midiout)
	    {
	      item->m_midiout->Send(0xB0,0x40+11,'0'+(((m_allmcus_bank_offset+1)/10)%10),-1);
	      item->m_midiout->Send(0xB0,0x40+10,'0'+((m_allmcus_bank_offset+1)%10),-1);
	    }
	  }
	  return true;
	}
	
        bool OnSMPTEBeats( MIDI_event_t *evt ) {
          int *tmodeptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode2);
          if (tmodeptr && *tmodeptr < 0)
            tmodeptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode);

          if (tmodeptr) switch (*tmodeptr & 0xff)
          {
            case 0: *tmodeptr = (*tmodeptr & ~0xff) | 1; break;
            case 1: case 2: case 6: case 7: *tmodeptr = (*tmodeptr & ~0xff) | 3; break;
            case 3: *tmodeptr = (*tmodeptr & ~0xff) | 4; break;
            case 4: *tmodeptr = (*tmodeptr & ~0xff) | 5; break;
            case 5: *tmodeptr = (*tmodeptr & ~0xff) | 8; break;
            default: *tmodeptr = (*tmodeptr & ~0xff); break;
          }
          UpdateTimeline();
          Main_UpdateLoopInfo(0);

          return true;
        }
	
	bool OnRotaryEncoderPush( MIDI_event_t *evt ) {
	  int trackid=evt->midi_message[1]-0x20;
	  m_pan_lasttouch[trackid]=timeGetTime();

	  trackid+=GetBankOffset();

	  MediaTrack *tr=CSurf_TrackFromID(trackid,g_csurf_mcpmode);
	  if (tr)
	  {
	    if (m_flipmode)
	    {
	      CSurf_SetSurfaceVolume(tr,CSurf_OnVolumeChange(tr,1.0,false),NULL);
	    }
	    else
	    {
	      CSurf_SetSurfacePan(tr,CSurf_OnPanChange(tr,0.0,false),NULL);
	    }
	  }
	  return true;
	}
	
	bool OnRecArm( MIDI_event_t *evt ) {
	  int tid=evt->midi_message[1];
	  tid+=GetBankOffset();
	  MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
	  if (tr)
	    CSurf_OnRecArmChange(tr,-1);
	  return true;
	}
	
	bool OnMuteSolo( MIDI_event_t *evt ) {
	  int tid=evt->midi_message[1]-0x08;
	  int ismute=(tid&8);
	  tid&=7;
	  tid+=GetBankOffset();

	  MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
	  if (tr)
	  {
	    if (ismute)
	      CSurf_SetSurfaceMute(tr,CSurf_OnMuteChange(tr,-1),NULL);
	    else
	      CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,-1),NULL);
	  }
	  return true;
	}
	
	bool OnSoloDC( MIDI_event_t *evt ) {
	  int tid=evt->midi_message[1]-0x08;
	  tid+=GetBankOffset();
	  MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
	  SoloAllTracks(0);
	  CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,1),NULL);
	  return true;
	}
	
	bool OnChannelSelect( MIDI_event_t *evt ) {
	  int tid=evt->midi_message[1]-0x18;
	  tid&=7;
	  tid+=GetBankOffset();
	  MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
	  if (tr) CSurf_OnSelectedChange(tr,-1); // this will automatically update the surface
	  return true;
	}
	
	bool OnChannelSelectDC( MIDI_event_t *evt ) {
	  int tid=evt->midi_message[1]-0x18;
	  tid&=7;
	  tid+=GetBankOffset();
	  MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
	  
	  // Clear already selected tracks
	  SelectedTrack *i = m_selected_tracks;
	  while(i) {
	    // Call to OnSelectedChange will cause 'i' to be destroyed, so go ahead
	    // and get 'next' now
	    SelectedTrack *next = i->next;
	    MediaTrack *track = i->track();
	    if( track ) CSurf_OnSelectedChange( track, 0 );
	    i = next;
	  }
	  
	  // Select this track
	  CSurf_OnSelectedChange( tr, 1 );
	  
	  return true;
	}

	bool OnTransport( MIDI_event_t *evt ) {
    switch(evt->midi_message[1]) {
    case 0x5f:
       CSurf_OnRecord();
       break;
    case 0x5e:
      CSurf_OnPlay();
      break;
    case 0x5d:
      CSurf_OnStop();
      break;
    case 0x5b:
      SendMessage(g_hwnd,WM_COMMAND,ID_MARKER_PREV,0);
      break;
    case 0x5c:
      SendMessage(g_hwnd,WM_COMMAND,ID_MARKER_NEXT,0);
    }
    return true;
	}
	
	bool OnMarker( MIDI_event_t *evt ) {
    SendMessage(g_hwnd,WM_COMMAND,IsKeyDown(VK_SHIFT)?ID_INSERT_MARKERRGN:ID_INSERT_MARKER,0);
    return true;
	}
	
	bool OnCycle( MIDI_event_t *evt ) {
    SendMessage(g_hwnd,WM_COMMAND,IDC_REPEAT,0);
    return true;
	}
	
	bool OnClick( MIDI_event_t *evt ) {
	  SendMessage(g_hwnd,WM_COMMAND,ID_METRONOME,0);
	  return true;
	}
	
	void ClearSaveLed() {
    if (m_midiout) 
      m_midiout->Send(0x90,0x50,0,-1);	  
	}
	
	bool OnSave( MIDI_event_t *evt ) {
    if (m_midiout) 
      m_midiout->Send(0x90,0x50,0x7f,-1);
    SendMessage(g_hwnd,WM_COMMAND,IsKeyDown(VK_SHIFT)?ID_FILE_SAVEAS:ID_FILE_SAVEPROJECT,0);
    ScheduleAction( timeGetTime() + 1000, &CSurf_MCU::ClearSaveLed );
    return true;
	}
	
  void ClearUndoLed() {
    if (m_midiout) 
      m_midiout->Send(0x90,0x51,0,-1);    
  }
  
	bool OnUndo( MIDI_event_t *evt ) {
    if (m_midiout) 
      m_midiout->Send(0x90,0x51,0x7f,-1);
    SendMessage(g_hwnd,WM_COMMAND,IsKeyDown(VK_SHIFT)?IDC_EDIT_REDO:IDC_EDIT_UNDO,0);
    ScheduleAction( timeGetTime() + 150, &CSurf_MCU::ClearUndoLed );
    return true;
	}
	
	bool OnZoom( MIDI_event_t *evt ) {
    m_mackie_arrow_states^=64;
    if (m_midiout) 
      m_midiout->Send(0x90, 0x64,(m_mackie_arrow_states&64)?0x7f:0,-1);
    return true;
	}
	
	bool OnScrub( MIDI_event_t *evt ) {
    m_mackie_arrow_states^=128;
    if (m_midiout) 
      m_midiout->Send(0x90, 0x65,(m_mackie_arrow_states&128)?0x7f:0,-1);
    return true;
	}
	
	bool OnFlip( MIDI_event_t *evt ) {
	  m_flipmode=!m_flipmode;
	  if (m_midiout) 
	    m_midiout->Send(0x90, 0x32,m_flipmode?1:0,-1);
	  CSurf_ResetAllCachedVolPanStates();
	  TrackList_UpdateAllExternalSurfaces();
	  return true;
	}

	bool OnGlobal( MIDI_event_t *evt ) {
    g_csurf_mcpmode=!g_csurf_mcpmode;
    if (m_midiout) 
      m_midiout->Send(0x90, 0x33,g_csurf_mcpmode?0x7f:0,-1);
    TrackList_UpdateAllExternalSurfaces();
    WritePrivateProfileString("csurf","mcu_mcp",g_csurf_mcpmode?"1":"0",get_ini_file());
    return true;
	}
	
	bool OnKeyModifier( MIDI_event_t *evt ) {
	  int mask=(1<<(evt->midi_message[1]-0x46));
	  if (evt->midi_message[2] >= 0x40)
	    m_mackie_modifiers|=mask;
	  else
	    m_mackie_modifiers&=~mask;
	  return true;
	}
	
	bool OnScroll( MIDI_event_t *evt ) {
	  if (evt->midi_message[2]>0x40)
	    m_mackie_arrow_states |= 1<<(evt->midi_message[1]-0x60);
	  else
	    m_mackie_arrow_states &= ~(1<<(evt->midi_message[1]-0x60));
	  return true;
	}
	
	bool OnTouch( MIDI_event_t *evt ) {
	  int fader = evt->midi_message[1]-0x68;
    m_fader_touchstate[fader]=evt->midi_message[2]>=0x7f;
    m_fader_lasttouch[fader]=0xFFFFFFFF; // never use this again!
    return true;
	}
	
	bool OnFunctionKey( MIDI_event_t *evt ) {
    if (!(m_cfg_flags&CONFIG_FLAG_MAPF1F8TOMARKERS)) return false;

	  int fkey = evt->midi_message[1] - 0x36;
	  int command = ( IsKeyDown(VK_CONTROL) ? ID_SET_MARKER1 : ID_GOTO_MARKER1 ) + fkey;
    SendMessage(g_hwnd,WM_COMMAND, command, 0);
    return true;
	}

	bool OnSoloButton( MIDI_event_t *evt ) {
	  SoloAllTracks(0);
	  return true;
	}
	
	struct ButtonHandler {
	  unsigned int evt_min;
	  unsigned int evt_max; // inclusive
	  MidiHandlerFunc func;
	  MidiHandlerFunc func_dc;
	};

	bool OnButtonPress( MIDI_event_t *evt ) {
	  if ( (evt->midi_message[0]&0xf0) != 0x90 )  
	    return false;

	  static const int nHandlers = 23;
	  static const int nPressOnlyHandlers = 20;
	  static const ButtonHandler handlers[nHandlers] = {
	      // Press down only events
	      { 0x4a, 0x4e, &CSurf_MCU::OnAutoMode,           NULL },
	      { 0x2e, 0x31, &CSurf_MCU::OnBankChannel,        NULL },
	      { 0x35, 0x35, &CSurf_MCU::OnSMPTEBeats,         NULL },
	      { 0x20, 0x27, &CSurf_MCU::OnRotaryEncoderPush,  NULL },
	      { 0x00, 0x07, &CSurf_MCU::OnRecArm,             NULL },
	      { 0x08, 0x0f, NULL,                             &CSurf_MCU::OnSoloDC },
	      { 0x08, 0x17, &CSurf_MCU::OnMuteSolo,           NULL },
	      { 0x18, 0x1f, &CSurf_MCU::OnChannelSelect,      &CSurf_MCU::OnChannelSelectDC },
	      { 0x5b, 0x5f, &CSurf_MCU::OnTransport,          NULL },
	      { 0x54, 0x54, &CSurf_MCU::OnMarker,             NULL },
	      { 0x56, 0x56, &CSurf_MCU::OnCycle,              NULL },
	      { 0x59, 0x59, &CSurf_MCU::OnClick,              NULL },
	      { 0x50, 0x50, &CSurf_MCU::OnSave,               NULL },
	      { 0x51, 0x51, &CSurf_MCU::OnUndo,               NULL },
	      { 0x64, 0x64, &CSurf_MCU::OnZoom,               NULL },
	      { 0x65, 0x65, &CSurf_MCU::OnScrub,              NULL },
	      { 0x32, 0x32, &CSurf_MCU::OnFlip,               NULL },
	      { 0x33, 0x33, &CSurf_MCU::OnGlobal,             NULL },
	      { 0x36, 0x3d, &CSurf_MCU::OnFunctionKey,        NULL },
	      { 0x5a, 0x5a, &CSurf_MCU::OnSoloButton,         NULL },
	      
	      // Press and release events
	      { 0x46, 0x49, &CSurf_MCU::OnKeyModifier },
	      { 0x60, 0x63, &CSurf_MCU::OnScroll },
	      { 0x68, 0x70, &CSurf_MCU::OnTouch },
	  };

	  unsigned int evt_code = evt->midi_message[1];  //get_midi_evt_code( evt );
	  
	  #if 0
	  char buf[512];
	  sprintf( buf, "   0x%08x %02x %02x %02x %02x 0x%08x 0x%08x %s", evt_code,
	      evt->midi_message[0], evt->midi_message[1], evt->midi_message[2], evt->midi_message[3],
	      handlers[0].evt_min, handlers[0].evt_max, 
	      handlers[0].evt_min <= evt_code && evt_code <= handlers[0].evt_max ? "yes" : "no" );
	  UpdateMackieDisplay( 0, buf, 56 );
    #endif
	  
	  // For these events we only want to track button press
	  if ( evt->midi_message[2] >= 0x40 ) {
	    // Check for double click
	    DWORD now = timeGetTime();
	    bool double_click = (int)evt_code == m_button_last && 
	        now - m_button_last_time < DOUBLE_CLICK_INTERVAL;
	    m_button_last = evt_code;
	    m_button_last_time = now;

	    // Find event handler
	    for ( int i = 0; i < nPressOnlyHandlers; i++ ) { 
	      ButtonHandler bh = handlers[i];
	      if ( bh.evt_min <= evt_code && evt_code <= bh.evt_max ) {
	        // Try double click first
	        if ( double_click && bh.func_dc != NULL )
	          if ( (this->*bh.func_dc)(evt) )
	            return true;

	        // Single click (and unhandled double clicks)
	        if ( bh.func != NULL )
	          if ( (this->*bh.func)(evt) ) 
	            return true;
	      }
	    }
	  }
	  
	  // For these events we want press and release
	  for ( int i = nPressOnlyHandlers; i < nHandlers; i++ )
      if ( handlers[i].evt_min <= evt_code && evt_code <= handlers[i].evt_max )
        if ( (this->*handlers[i].func)(evt) ) return true;

	  // Pass thru if not otherwise handled
	  if ( evt->midi_message[2]>=0x40 ) {
	    int a=evt->midi_message[1];
	    MIDI_event_t evt={0,3,{0xbf-(m_mackie_modifiers&15),a,0}};
	    kbd_OnMidiEvent(&evt,-1);
	  }
	  
	  return true;
	}

    void OnMIDIEvent(MIDI_event_t *evt)
    {
        #if 0
        char buf[512];
        sprintf(buf,"message %02x, %02x, %02x\n",evt->midi_message[0],evt->midi_message[1],evt->midi_message[2]);
        OutputDebugString(buf);
        #endif

        static const int nHandlers = 5;
        static const MidiHandlerFunc handlers[nHandlers] = {
            &CSurf_MCU::OnMCUReset,
            &CSurf_MCU::OnFaderMove,
            &CSurf_MCU::OnRotaryEncoder,
            &CSurf_MCU::OnJogWheel,
            &CSurf_MCU::OnButtonPress,
        };
        for ( int i = 0; i < nHandlers; i++ )
          if ( (this->*handlers[i])(evt) ) return;
    }

public:

    CSurf_MCU(bool ismcuex, int offset, int size, int indev, int outdev, int cfgflags, int *errStats) 
    {
      m_cfg_flags=cfgflags;

      m_mcu_list.Add(this);

      m_is_mcuex=ismcuex; 
      m_offset=offset;
      m_size=size;
      m_midi_in_dev=indev;
      m_midi_out_dev=outdev;


      // init locals
      int x;
      for (x = 0; x < sizeof(m_mcu_meterpos)/sizeof(m_mcu_meterpos[0]); x ++)
        m_mcu_meterpos[x]=-100000.0;
      m_mcu_timedisp_lastforce=0;
      m_mcu_meter_lastrun=0;
      memset(m_fader_touchstate,0,sizeof(m_fader_touchstate));
      memset(m_fader_lasttouch,0,sizeof(m_fader_lasttouch));
      memset(m_pan_lasttouch,0,sizeof(m_pan_lasttouch));


      //create midi hardware access
      m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
      m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput(CreateMIDIOutput(m_midi_out_dev,false,NULL)) : NULL;

      if (errStats)
      {
        if (m_midi_in_dev >=0  && !m_midiin) *errStats|=1;
        if (m_midi_out_dev >=0  && !m_midiout) *errStats|=2;
      }

      MCUReset();

      if (m_midiin)
        m_midiin->start();
      
      m_repos_faders = false;
      m_schedule = NULL;
      m_selected_tracks = NULL;
    }
    
    ~CSurf_MCU() 
    {
      m_mcu_list.Delete(m_mcu_list.Find(this));
      if (m_midiout)
      {

        #if 1 // reset MCU to stock!, fucko enable this in dist builds, maybe?
        struct
        {
          MIDI_event_t evt;
          char data[5];
        }
        evt;
        evt.evt.frame_offset=0;
        evt.evt.size=8;
        unsigned char *wr = evt.evt.midi_message;
        wr[0]=0xF0;
        wr[1]=0x00;
        wr[2]=0x00;
        wr[3]=0x66;
        wr[4]=m_is_mcuex ? 0x15 : 0x14;
        wr[5]=0x08;
        wr[6]=0x00;
        wr[7]=0xF7;
        Sleep(5);
        m_midiout->SendMsg(&evt.evt,-1);
        Sleep(5);

        #elif 0
        char bla[11]={"          "};
        int x;
        for (x =0 ; x < sizeof(bla)-1; x ++)
          m_midiout->Send(0xB0,0x40+x,bla[x],-1);
        UpdateMackieDisplay(0,"",56*2);
        #endif


      }
      DELETE_ASYNC(m_midiout);
      DELETE_ASYNC(m_midiin);
      while( m_schedule != NULL ) {
        ScheduledAction *temp = m_schedule;
        m_schedule = temp->next;
        delete temp;
      }
      while( m_selected_tracks != NULL ) {
        SelectedTrack *temp = m_selected_tracks;
        m_selected_tracks = temp->next;
        delete temp;
      }
    }
    



    const char *GetTypeString() { return m_is_mcuex ? "MCUEX" : "MCU"; }
    const char *GetDescString()
    {
      m_descspace.SetFormatted(512,
          m_is_mcuex ? __LOCALIZE_VERFMT("Mackie Control Extended (dev %d,%d)","csurf") :
          __LOCALIZE_VERFMT("Mackie Control (dev %d,%d)","csurf"),m_midi_in_dev,m_midi_out_dev);
      return m_descspace.Get();     
    }
    const char *GetConfigString() // string of configuration data
    {
      snprintf(m_configtmp,sizeof(m_configtmp),"%d %d %d %d %d",m_offset,m_size,m_midi_in_dev,m_midi_out_dev,m_cfg_flags);      
      return m_configtmp;
    }

    void CloseNoReset() 
    { 
      DELETE_ASYNC(m_midiout);
      DELETE_ASYNC(m_midiin);
      m_midiout=0;
      m_midiin=0;
    }

    void RunOutput(DWORD now);

    void Run() 
    { 
      DWORD now=timeGetTime();

      if ((now - m_frameupd_lastrun) >= (1000/max((*g_config_csurf_rate),1)))
      {
        m_frameupd_lastrun=now;

        while( m_schedule && (now-m_schedule->time) < 0x10000000 ) {
          ScheduledAction *action = m_schedule;
          m_schedule = m_schedule->next;
          (this->*(action->func))();
          delete action;
        }
        
        RunOutput(now);
      }

      if (m_midiin)
      {
        m_midiin->SwapBufs(timeGetTime());
        int l=0;
        MIDI_eventlist *list=m_midiin->GetReadBuf();
        MIDI_event_t *evts;
        while ((evts=list->EnumItems(&l))) OnMIDIEvent(evts);

        if (m_mackie_arrow_states)
        {
          DWORD now=timeGetTime();
          if ((now-m_buttonstate_lastrun) >= 100)
          {
            m_buttonstate_lastrun=now;

            if (m_mackie_arrow_states)
            {
              int iszoom=m_mackie_arrow_states&64;

              if (m_mackie_arrow_states&1) 
                CSurf_OnArrow(0,!!iszoom);
              if (m_mackie_arrow_states&2) 
                CSurf_OnArrow(1,!!iszoom);
              if (m_mackie_arrow_states&4) 
                CSurf_OnArrow(2,!!iszoom);
              if (m_mackie_arrow_states&8) 
                CSurf_OnArrow(3,!!iszoom);

            }
          }
        }
      }
      
      if ( m_repos_faders && now >= m_fader_lastmove + FADER_REPOS_WAIT ) {
        m_repos_faders = false;
        TrackList_UpdateAllExternalSurfaces();
      }
    }

    void SetTrackListChange() 
    { 
      if (m_midiout)
      {
        int x;
        for (x = 0; x < 8; x ++)
        {
          MediaTrack *t=CSurf_TrackFromID(x+GetBankOffset(),g_csurf_mcpmode);
          if (!t || t == CSurf_TrackFromID(0,false))
          {
            // clear item
            int panint=m_flipmode ? panToInt14(0.0) : volToInt14(0.0);
            unsigned char volch=m_flipmode ? volToChar(0.0) : panToChar(0.0);

            m_midiout->Send(0xe0 + (x&0xf),panint&0x7f,(panint>>7)&0x7f,-1);
            m_midiout->Send(0xb0,0x30+(x&0xf),1+((volch*11)>>7),-1);
            m_vol_lastpos[x]=panint;


            m_midiout->Send(0x90, 0x10+(x&7),0,-1); // reset mute
            m_midiout->Send(0x90, 0x18+(x&7),0,-1); // reset selected

            m_midiout->Send(0x90, 0x08+(x&7),0,-1); //reset solo
            m_midiout->Send(0x90, 0x0+(x&7),0,-1); // reset recarm

            char buf[7]={0,};       
            UpdateMackieDisplay(x*7,buf,7); // clear display

            struct
            {
              MIDI_event_t evt;
              char data[9];
            }
            evt;
            evt.evt.frame_offset=0;
            evt.evt.size=9;
            unsigned char *wr = evt.evt.midi_message;
            wr[0]=0xF0;
            wr[1]=0x00;
            wr[2]=0x00;
            wr[3]=0x66;
            wr[4]=m_is_mcuex ? 0x15 : 0x14;
            wr[5]=0x20;
            wr[6]=0x00+x;
            wr[7]=0x03;
            wr[8]=0xF7;
            Sleep(5);
            m_midiout->SendMsg(&evt.evt,-1);
            Sleep(5);
            m_midiout->Send(0xD0,(x<<4)|0xF,0,-1);
          }
        }
      }
    }
#define FIXID(id) const int oid=CSurf_TrackToID(trackid,g_csurf_mcpmode); \
    int id=oid; \
    if (id>0) { id -= GetBankOffset(); if (id==8) id=-1; } else if (id==0) id=8;

    void SetSurfaceVolume(MediaTrack *trackid, double volume) 
    { 
      FIXID(id)
      if (m_midiout && id >= 0 && id < 256 && id < m_size)
      {
        if (m_flipmode)
        {
          unsigned char volch=volToChar(volume);
          if (id<8)
            m_midiout->Send(0xb0,0x30+(id&0xf),1+((volch*11)>>7),-1);
        }
        else
        {
          int volint=volToInt14(volume);

          if (m_vol_lastpos[id]!=volint)
          {
            m_vol_lastpos[id]=volint;
            m_midiout->Send(0xe0 + (id&0xf),volint&0x7f,(volint>>7)&0x7f,-1);
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
              m_midiout->Send(0xe0 + (id&0xf),panint&0x7f,(panint>>7)&0x7f,-1);
            }
          }
          else
          {
            if (id<8)
              m_midiout->Send(0xb0,0x30+(id&0xf),1+((panch*11)>>7),-1);
          }
        }
      }
    }
    void SetSurfaceMute(MediaTrack *trackid, bool mute) 
    {
      FIXID(id)
      if (m_midiout && id>=0 && id < 256 && id < m_size)
      {
        if (id<8)
          m_midiout->Send(0x90, 0x10+(id&7),mute?0x7f:0,-1);
      }     
    }

    void SetSurfaceSelected(MediaTrack *trackid, bool selected) 
    { 
      if ( selected ) 
        selectTrack(trackid);
      else
        deselectTrack(trackid);
      
      FIXID(id)
      if (m_midiout && id>=0 && id < 256 && id < m_size)
      {
        if (id<8)
          m_midiout->Send(0x90, 0x18+(id&7),selected?0x7f:0,-1);
      }
      UpdateAutoModes();
    }
    
    void selectTrack( MediaTrack *trackid ) {
      const GUID *guid = GetTrackGUID(trackid);
      if (!guid) return;

      // Empty list, start new list
      if ( m_selected_tracks == NULL ) {
        m_selected_tracks = new SelectedTrack(guid);
        return;
      }
      
      // This track is head of list
      if (!memcmp(&m_selected_tracks->guid,guid,sizeof(GUID)) )
        return;
      
      // Scan for track already selected
      SelectedTrack *i = m_selected_tracks;
      while ( i->next ) {
        i = i->next;
        if (!memcmp(&i->guid,guid,sizeof(GUID)) )
          return;
      }
      
      // Append at end of list if not already selected
      i->next = new SelectedTrack(guid);
    }
    
    void deselectTrack( MediaTrack *trackid ) {
      const GUID *guid = GetTrackGUID(trackid);
      if (!guid) return;
      
      // Empty list?
      if ( m_selected_tracks ) {
        // This track is head of list?
        if (!memcmp(&m_selected_tracks->guid,guid,sizeof(GUID)) ) {
          SelectedTrack *tmp = m_selected_tracks;
          m_selected_tracks = m_selected_tracks->next;
          delete tmp;
        }
        
        // Search for this track
        else {
          SelectedTrack *i = m_selected_tracks;
          while( i->next ) {
            if (!memcmp(&i->next->guid,guid,sizeof(GUID)) ) {
              SelectedTrack *tmp = i->next;
              i->next = i->next->next;
              delete tmp;
              break;
            }
            i = i->next;
          }
        }
      }
    }
    
    void SetSurfaceSolo(MediaTrack *trackid, bool solo) 
    { 
      FIXID(id)
      if (m_midiout && id>=0 && id < 256 && id < m_size)
      {
        if (id < 8)
          m_midiout->Send(0x90, 0x08+(id&7),solo?1:0,-1); //blink
        else if (id == 8) {
          // Hmm, seems to call this with id 8 to tell if any
          // tracks are soloed.
          m_midiout->Send(0x90, 0x73,solo?1:0,-1);     // rude solo light
          m_midiout->Send(0x90, 0x5a,solo?0x7f:0,-1);  // solo button led
        }
      }
    }

    void SetSurfaceRecArm(MediaTrack *trackid, bool recarm) 
    { 
      FIXID(id)
      if (m_midiout && id>=0 && id < 256 && id < m_size)
      {
        if (id < 8)
        {
          m_midiout->Send(0x90, 0x0+(id&7),recarm?0x7f:0,-1);
        }
      }
    }
    void SetPlayState(bool play, bool pause, bool rec) 
    { 
      if (m_midiout && !m_is_mcuex)
      {
        m_midiout->Send(0x90, 0x5f,rec?0x7f:0,-1);
        m_midiout->Send(0x90, 0x5e,play||pause?0x7f:0,-1);
        m_midiout->Send(0x90, 0x5d,!play?0x7f:0,-1); 
      }
    }
    void SetRepeatState(bool rep) 
    {
      if (m_midiout && !m_is_mcuex)
      {
        m_midiout->Send(0x90, 0x56,rep?0x7f:0,-1);
      }
      
    }

    void SetTrackTitle(MediaTrack *trackid, const char *title) 
    {
      FIXID(id)
      if (m_midiout && id >= 0 && id < 8)
      {
        char buf[32];
        strncpy(buf,title,6);
        buf[6]=0;
        if ( strlen(buf) == 0 ) {
          int trackno = CSurf_TrackToID(trackid,g_csurf_mcpmode);
          if ( trackno < 100 ) 
            snprintf( buf, sizeof(buf), "  %02d  ", trackno );
          else
            snprintf( buf, sizeof(buf), "  %d ", trackno );
        }
        UpdateMackieDisplay(id*7,buf,7);
      }
    }
    bool GetTouchState(MediaTrack *trackid, int isPan=0)
    {
      if (isPan != 0 && isPan != 1) return false;

      FIXID(id)
      if (!m_flipmode != !isPan) 
      {
        if (id >= 0 && id < 8)
        {
          if (m_pan_lasttouch[id]==1 || (timeGetTime()-m_pan_lasttouch[id]) < 3000) // fake touch, go for 3s after last movement
          {
            return true;
          }
        }
        return false;
      }
      if (id>=0 && id < 9)
      {
        if (!(m_cfg_flags&CONFIG_FLAG_FADER_TOUCH_MODE) && !m_fader_touchstate[id] && m_fader_lasttouch[id] && m_fader_lasttouch[id]!=0xffffffff)
        {
          if ((timeGetTime()-m_fader_lasttouch[id]) < 3000) return true;
          return false;
        }

        return !!m_fader_touchstate[id];
      }
  
      return false; 
    }

    void SetAutoMode(int mode) 
    { 
      UpdateAutoModes();
    }

    void UpdateAutoModes() {
      if ( m_midiout && !m_is_mcuex ) {
        int modes[5] = { 0, 0, 0, 0, 0 };
        for ( SelectedTrack *i = m_selected_tracks; i; i = i->next ) {
          MediaTrack *track = i->track();
          if (!track) continue;
          int mode = GetTrackAutomationMode(track);
          if ( 0 <= mode && mode < 5 )
            modes[mode] = 1;
        }
        bool multi = ( modes[0] + modes[1] + modes[2] + modes[3] + modes[4] ) > 1;
        m_midiout->Send(0x90, 0x4A, modes[AUTO_MODE_READ] ? ( multi ? 1:0x7f ) : 0, -1 );
        m_midiout->Send(0x90, 0x4B, modes[AUTO_MODE_WRITE] ? ( multi ? 1:0x7f ) : 0, -1 );
        m_midiout->Send(0x90, 0x4C, modes[AUTO_MODE_TRIM] ? ( multi ? 1:0x7f ) : 0, -1 );
        m_midiout->Send(0x90, 0x4D, modes[AUTO_MODE_TOUCH] ? ( multi ? 1:0x7f ) : 0, -1 );
        m_midiout->Send(0x90, 0x4E, modes[AUTO_MODE_LATCH] ? ( multi ? 1:0x7f ) : 0, -1 );
      }
    }

    void ResetCachedVolPanStates() 
    { 
      memset(m_vol_lastpos,0xff,sizeof(m_vol_lastpos));
      memset(m_pan_lastpos,0xff,sizeof(m_pan_lastpos));
    }
    
    void OnTrackSelection(MediaTrack *trackid) 
    { 
      if (m_cfg_flags&CONFIG_FLAG_NOBANKOFFSET) return; // ignore if not using bank offset

      int tid=CSurf_TrackToID(trackid,g_csurf_mcpmode);
      // if no normal MCU's here, then slave it
      int x;
      int movesize=8;
      for (x = 0; x < m_mcu_list.GetSize(); x ++)
      {
        CSurf_MCU *mcu=m_mcu_list.Get(x);
        if (mcu && !(mcu->m_cfg_flags&CONFIG_FLAG_NOBANKOFFSET))
        {
          if (mcu->m_offset+8 > movesize)
            movesize=mcu->m_offset+8;
        }
      }

      int newpos=tid-1;
      if (newpos >= 0 && (newpos < m_allmcus_bank_offset || newpos >= m_allmcus_bank_offset+movesize))
      {
        int no = newpos - (newpos % movesize);

        if (no!=m_allmcus_bank_offset)
        {
          m_allmcus_bank_offset=no;
          // update all of the sliders
          TrackList_UpdateAllExternalSurfaces();
          for (x = 0; x < m_mcu_list.GetSize(); x ++)
          {
            CSurf_MCU *mcu=m_mcu_list.Get(x);
            if (mcu && !mcu->m_is_mcuex && mcu->m_midiout)
            {
              mcu->m_midiout->Send(0xB0,0x40+11,'0'+(((m_allmcus_bank_offset+1)/10)%10),-1);
              mcu->m_midiout->Send(0xB0,0x40+10,'0'+((m_allmcus_bank_offset+1)%10),-1);
            }
          }
        }
      }
    }
    
    bool IsKeyDown(int key) 
    { 
      if (m_midiin && !m_is_mcuex)
      {
        if (key == VK_SHIFT) return !!(m_mackie_modifiers&1);
        if (key == VK_CONTROL) return !!(m_mackie_modifiers&4);
        if (key == VK_MENU) return !!(m_mackie_modifiers&8);
      }

      return false; 
    }
  virtual int Extended(int call, void *parm1, void *parm2, void *parm3)
  {
    DEFAULT_DEVICE_REMAP()
    return 0;
  }
};

static void parseParms(const char *str, int parms[5])
{
  parms[0]=0;
  parms[1]=9;
  parms[2]=parms[3]=-1;
  parms[4]=0;

  const char *p=str;
  if (p)
  {
    int x=0;
    while (x<5)
    {
      while (*p == ' ') p++;
      if ((*p < '0' || *p > '9') && *p != '-') break;
      parms[x++]=atoi(p);
      while (*p && *p != ' ') p++;
    }
  }  
}


void CSurf_MCU::RunOutput(DWORD now)
{
  if (!m_midiout) return;

  if (!m_is_mcuex)
  {
    double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();
    char buf[128];
    unsigned char bla[10];
//      bla[-2]='A';//first char of assignment
//    bla[-1]='Z';//second char of assignment

    // if 0x40 set, dot below item

    memset(bla,0,sizeof(bla));


    int *tmodeptr=(int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode2);

    int tmode=0;
    
    if (tmodeptr && (*tmodeptr)>=0) tmode = *tmodeptr & 0xff;
    else
    {
      tmodeptr=(int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode);
      if (tmodeptr)
        tmode=*tmodeptr & 0xff;
    }

    if (tmode==3) // seconds
    {
      double *toptr = (double*)projectconfig_var_addr(NULL,__g_projectconfig_timeoffs);

      if (toptr) pp+=*toptr;
      snprintf(buf,sizeof(buf),"%d %02d",(int)pp, ((int)(pp*100.0))%100);
      if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
      else
        memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));

    }
    else if (tmode==4) // samples
    {
      format_timestr_pos(pp,buf,sizeof(buf),4);
      if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
      else
        memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
    }
    else if (tmode==5 || tmode == 8) // frames
    {
      format_timestr_pos(pp,buf,sizeof(buf),tmode);
      char *p=buf;
      char *op=buf;
      int ccnt=0;
      while (*p)
      {
        if (*p == ':')
        {
          ccnt++;
          if (tmode == 5 && ccnt!=3) 
          {
            p++;
            continue;
          }
          *p=' ';
        }

        *op++=*p++;
      }
      *op=0;
      if (strlen(buf)>sizeof(bla)) memcpy(bla,buf+strlen(buf)-sizeof(bla),sizeof(bla));
      else
        memcpy(bla+sizeof(bla)-strlen(buf),buf,strlen(buf));
    }
    else if (tmode>0)
    {
      int num_measures=0;
      double beats=TimeMap2_timeToBeats(NULL,pp,&num_measures,NULL,NULL,NULL)+ 0.000000000001;
      double nbeats = floor(beats);

      beats -= nbeats;

      int fracbeats = (int) (1000.0 * beats);

      int *measptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_measoffs);
      int nm=num_measures+1+(measptr ? *measptr : 0);
      if (nm >= 100) bla[0]='0'+(nm/100)%10;//bars hund
      if (nm >= 10) bla[1]='0'+(nm/10)%10;//barstens
      bla[2]='0'+(nm)%10;//bars

      int nb=(int)nbeats+1;
      if (nb >= 10) bla[3]='0'+(nb/10)%10;//beats tens
      bla[4]='0'+(nb)%10;//beats


      bla[7]='0' + (fracbeats/100)%10;
      bla[8]='0' + (fracbeats/10)%10;
      bla[9]='0' + (fracbeats%10); // frames
    }
    else
    {
      double *toptr = (double*)projectconfig_var_addr(NULL,__g_projectconfig_timeoffs);
      if (toptr) pp+=(*toptr);

      int ipp=(int)pp;
      int fr=(int)((pp-ipp)*1000.0);

      if (ipp >= 360000) bla[0]='0'+(ipp/360000)%10;//hours hundreds
      if (ipp >= 36000) bla[1]='0'+(ipp/36000)%10;//hours tens
      if (ipp >= 3600) bla[2]='0'+(ipp/3600)%10;//hours

      bla[3]='0'+(ipp/600)%6;//min tens
      bla[4]='0'+(ipp/60)%10;//min 
      bla[5]='0'+(ipp/10)%6;//sec tens
      bla[6]='0'+(ipp%10);//sec
      bla[7]='0' + (fr/100)%10;
      bla[8]='0' + (fr/10)%10;
      bla[9]='0' + (fr%10); // frames
    }

    if (m_mackie_lasttime_mode != tmode)
    {
      m_mackie_lasttime_mode=tmode;
      m_midiout->Send(0x90, 0x71, tmode==5?0x7F:0,-1); // set smpte light 
      m_midiout->Send(0x90, 0x72, m_mackie_lasttime_mode>0 && tmode<3?0x7F:0,-1); // set beats light 

    }

    //if (memcmp(m_mackie_lasttime,bla,sizeof(bla)))
    {
      bool force=false;
      if (now > m_mcu_timedisp_lastforce) 
      {
        m_mcu_timedisp_lastforce=now+2000;
        force=true;
      }
      int x;
      for (x =0 ; x < sizeof(bla) ; x ++)
      {
        int idx=sizeof(bla)-x-1;
        if (bla[idx]!=m_mackie_lasttime[idx]||force)
        {
          m_midiout->Send(0xB0,0x40+x,bla[idx],-1);
          m_mackie_lasttime[idx]=bla[idx];
        }
      }
    }

    // 0xD0 = level meter, hi nibble = channel index, low = level (F=clip, E=top)
//      m_midiout->Send(0xD0,0x1E,0);
//
    if (__g_projectconfig_metronome_en)
    {
      int *mp=(int*)projectconfig_var_addr(NULL,__g_projectconfig_metronome_en);
      int lmp = mp ? (*mp&1) : 0;
      if ((m_last_miscstate&1) != lmp)
      {
        m_last_miscstate = (m_last_miscstate&~1) | lmp;
        m_midiout->Send(0x90, 0x59,lmp?0x7f:0,-1);  // click (metronome) indicator
      }
    }
  }

  {
    int x;
#define VU_BOTTOM 70
    double decay=0.0;
    if (m_mcu_meter_lastrun) 
    {
      decay=VU_BOTTOM * (double) (now-m_mcu_meter_lastrun)/(1.4*1000.0);            // they claim 1.8s for falloff but we'll underestimate
    }
    m_mcu_meter_lastrun=now;
    for (x = 0; x < 8; x ++)
    {
      int idx=GetBankOffset()+x;
      MediaTrack *t;
      if ((t=CSurf_TrackFromID(idx,g_csurf_mcpmode)))
      {
        double pp=VAL2DB((Track_GetPeakInfo(t,0)+Track_GetPeakInfo(t,1)) * 0.5);

        if (m_mcu_meterpos[x] > -VU_BOTTOM*2) m_mcu_meterpos[x] -= decay;

        if (pp < m_mcu_meterpos[x]) continue;
        m_mcu_meterpos[x]=pp;
        int v=0xd; // 0xe turns on clip indicator, 0xf turns it off
        if (pp < 0.0)
        {
          if (pp < -VU_BOTTOM)
            v=0x0;
          else v=(int) ((pp+VU_BOTTOM)*13.0/VU_BOTTOM);
        }

        m_midiout->Send(0xD0,(x<<4)|v,0,-1);
      }
    }
  }
}

static IReaperControlSurface *createFunc(const char *type_string, const char *configString, int *errStats)
{
  int parms[5];
  parseParms(configString,parms);

  static bool init;
  if (!init)
  {
    init = true;
    g_csurf_mcpmode = !!GetPrivateProfileInt("csurf","mcu_mcp",0,get_ini_file());
  }

  return new CSurf_MCU(!strcmp(type_string,"MCUEX"),parms[0],parms[1],parms[2],parms[3],parms[4],errStats);
}


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        int parms[5];
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
        if (parms[4]&CONFIG_FLAG_FADER_TOUCH_MODE)
          CheckDlgButton(hwndDlg,IDC_CHECK1,BST_CHECKED);
        if (parms[4]&CONFIG_FLAG_MAPF1F8TOMARKERS)
          CheckDlgButton(hwndDlg,IDC_CHECK2,BST_CHECKED);
        if (parms[4]&CONFIG_FLAG_NOBANKOFFSET)
          CheckDlgButton(hwndDlg,IDC_CHECK3,BST_CHECKED);
        
      }
    break;
    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        char tmp[512];

        int indev=-1, outdev=-1, offs=0, size=9;
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
        int cflags=0;
        if (IsDlgButtonChecked(hwndDlg,IDC_CHECK1))
          cflags|=CONFIG_FLAG_FADER_TOUCH_MODE;
        if (IsDlgButtonChecked(hwndDlg,IDC_CHECK2))
          cflags|=CONFIG_FLAG_MAPF1F8TOMARKERS;
        if (IsDlgButtonChecked(hwndDlg,IDC_CHECK3))
          cflags|=CONFIG_FLAG_NOBANKOFFSET;

        snprintf(tmp,sizeof(tmp),"%d %d %d %d %d",offs,size,indev,outdev,cflags);
        lstrcpyn((char *)lParam, tmp,wParam);
        
      }
    break;
  }
  return 0;
}

static HWND configFunc(const char *type_string, HWND parent, const char *initConfigString)
{
  return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_SURFACEEDIT_MCU1),parent,dlgProc,(LPARAM)initConfigString);
}


reaper_csurf_reg_t csurf_mcu_reg = 
{
  "MCU",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "Mackie Control Universal",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
reaper_csurf_reg_t csurf_mcuex_reg = 
{
  "MCUEX",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "Mackie Control Extender",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
