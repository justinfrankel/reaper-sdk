/*
** reaper_csurf
** MCU support - Modified for 01X support by REAPER forum member: Deric (April 2008)
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/


#include "csurf.h"
#include "../../WDL/ptrlist.h"

/*
01X documentation:

Note1: This file is mapped for the 01X key-codes generated when the 01X is in 'cubendo' DAW mode.
The DAW mode on an 01X is set as follows:
1.Press the 'Utility' button - this enters configuration mode.
2.Press the knob (Pan knob - below the display) on channel 1 - this is below the screen displaying 'REMOTE'.
3.Press the knob on channel 3 - below screen display 'C*BASE'.
4.If prompted (if already in 'cubendo' mode you will not be prompted) press the knob on channel 8 - screen displays 'YES'.
5.To ensure the 01X always boots up into the above mode you need to save this setting. Hold down the SHIFT key then press UTILITY.

Note2: 'Cubendo' mode is not as versatile as 'General' mode. In General mode all of the 01Xs buttons are available and in any combination.
In 'Cubendo' mode many of the buttons are unavailable externally, and some of the buttons generate the same key-codes.
'EDIT' generates 90 33 7F/00 which is the same code as SHIFT F4. 
All of the EQ buttons generate the same code - 90 4C 7F/00 in 'Cubendo' mode.
'GROUP' button is unavailable, as is 'DYNAMICS' (but SHIFT+DYNAMICS (labelled PLUG-IN) is available), AUDIO,INST,MIDI,BUS/AUX,OTHER,SELECTED CHANNEL,
PAGE SHIFT,MARKER, are all unavailable. 
SHIFT only generates a key-code with certain combinations of other keys.

Note3: At the time of writing I have yet to Address the 01X display in General mode (it is not the same as 'Cubendo' mode).
This is why this mapping uses 'Cubendo' mode. I have placed two separate support requests with YAMAHA Global and YAMAHA UK support but have heard nothing as yet.

Note4: Many of the keys generate different key-codes in General mode compared to 'Cubendo' mode - editing of the some of the values
of this file if General mode is used will be required.

Note5: General mode is preferred as all of the keys are available, in any combination, and all have discrete values.

Note6: This is a hacked-about version of the original Cockos MCU file CSurf_MCU.cpp and there are probably still many enries surplus to requirements.

Note7: The REAPER functionality provided herein greatly surpasses that of the 01X as factory supplied with any DAW, with the exception that, currently,
no FX control is currently possible.

Note8: The following lists the key-codes generated in both 'Cubendo' and Genereal modes of 01X operation:

'Cubendo' mode key-codes:	GENERAL mode key-codes:
=========================	=======================
90 xx 7F/00 codes, xx:		

Flip			0x32		0x32
F1			0x36		0x36
F2			0x37		0x37
F3			0x38		0x38
F4			0x39		0x39
F5			0x3a		0x3a
F6			0x3b		0x3b
F7			0x3c		0x3c
F8			0x3d		0x3d
Shift+F1		0x4d		(see F1)
Shift+F2		0x4e		(see F2)
Shift+F4 (Edit) 	0x33		(see F4)
Loop			0x56		0x56
<< (REW)		0x5b		0x5b
>> (FFW)		0x5c		0x5c
Stop			0x5d		0x5d
Play			0x5e		0x5e
Record			0x5f		0x5f
Arrow ^			0x60		0x60
Arrow v			0x61		0x61
Arrow <			0x62		0x62
Arrow >			0x63		0x63
Zoom			0x64		0x64
Scrub			0x65		0x65

EQ(all)			0x2c		

EQ(Low)			(0x2c)		0x29
EQ(Low-Mid)		(0x2c)		0x2a
EQ(High-Mid)		(0x2c)		0x2b
EQ(High)		(0x2c)		0x2c

SHIFT+Plug-in	0x2b		Dynamics/Plug-in=0x2f
Send			0x2d		0x2e
SHIFT+Send		0x50		(see Send:0x2e)
Pan/Ch-Param		0x2a		0x2d
Display v		0x29		0x5a
Display ^		0x28		0x59
Name/Value		0x34		0x34
Effect			0x4c		0x31
Marker+<<		0x58		(see <<)
Marker+>>		0x5a		(see >>)
Write			0x59		0x33
Save			0x48		0x4f
Shift+Save		0x49		(see Save)
Undo			0x46		0x4c
Shift+Undo		0x47		(see Undo)
Edit			0x33		0x45
SHIFT			0x53		0x46
Bank <			0x2e		0x54
Bank >			0x2f		0x55

SelectedChlLib		none		0x28
Group/Folders		none		0x30
Audio			none		0x3e
Inst			none		0x3f
MIDI			none		0x40
Bus/Aux			none		0x41
Other			none		0x42
Rec Rdy			none		0x48
Auto R/W		none		0x49
Marker			none		0x52
Pageshift		none		0x58

Jog/Shuttle:	B0 3c 41/01 (L/R clockwise/anticlockwise) (same for both modes)


Pan knobs:		B0 1x 41/01 (L/R) where 'x'=0 for track-1, 
						'x'=7 for track-8.

Volume faders:
Touch:			90 6y 7f/00	(default=2000mS) where	'y'=8 for track-1, 
								'y'=f for track-8,
								value=70 for Master.

Volume/Level:	ex 00-70 00-7f	where	'x'=0 for track-1, 
										'x'=7 for track-8,
										'x'=8 for master.
										ex 00 00 = -oo (minimum)
										ex 70 7f = full-scale (maximum)

Track Select (SEL buttons):				90 1x 7f/00 where	'x'=8 for track-1,
										'x'=f for track-8.

Track Solo (ON buttons)('Solo' LED=ON):	90 0x 7f/00 where	'x'=8 for track-1,
								'x'=f for track-8.

Track Mute (ON buttons) ('Solo'LED=OFF):90 1x 7f/00 where	'x'=0 for track-1,
								'x'=7 for track-8.


'Cubendo' mode SYSEX: F0 00 00 66 14 01 58 59 5A 00 00 00 00 00 00 00 00 F7

Note9:	To achieve the level of functionality available here many of the keys have multiple functions.
Whilst effort has been made to make this as intuitive as possible - do yourself a favour and watch the
accompanying demo/manual - I've produced a very detailed, and useful,video demo and reference 'manual' 
- you WILL miss out on a lot of great time-saving features otherwise.
Of course if you can just read the code, with sufficient understanding of every mapped-function and combination, and remember it all then forget the video :)

Note10: The Jog wheel has been utilised extensively - use it for fastest-ever project navigation, 
project zooming (both in and out, both horizontally and vertically), scrolling both up and down, and left and right.
You can also use it to create loops and time selections - and adjust existing loops.

Note11:	Rew and FFW (<< & >>) can be used for item navigation, transient navigation, as well as loop and project navigation, etc.

Note12: If you manage to successfully write to the 01X screen in General mode please drop a note into the REAPER forum in order for others
to be able to make use this, similarly if you come up with any useful mods, or suggestions.

/End 01X documentation.
*/
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

class CSurf_01X;
static WDL_PtrList<CSurf_01X> m_01X_list;
static bool g_csurf_mcpmode;
static int m_flipmode;
static int m_allmcus_bank_offset;


class CSurf_01X : public IReaperControlSurface
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

	unsigned char m_01x_jogstate;
	unsigned short int m_01x_keystate;
	unsigned short int m_01x_automode;
		
    char m_fader_touchstate[256];
    DWORD m_pan_lasttouch[256];

    WDL_String descspace;
    char configtmp[1024];

    double m_mcu_meterpos[8];
    DWORD m_mcu_timedisp_lastforce, m_mcu_meter_lastrun;
    DWORD m_frameupd_lastrun;

    void MCUReset()
    {
      memset(m_mackie_lasttime,0,sizeof(m_mackie_lasttime));
      memset(m_fader_touchstate,0,sizeof(m_fader_touchstate));
      memset(m_pan_lasttouch,0,sizeof(m_pan_lasttouch));
      m_mackie_lasttime_mode=-1;
      m_mackie_modifiers=0;
      m_01x_jogstate=0;
	  m_01x_automode=40400;
	  m_01x_keystate=0;


      memset(m_vol_lastpos,0xff,sizeof(m_vol_lastpos));
      memset(m_pan_lastpos,0xff,sizeof(m_pan_lastpos));
	  if (m_midiout) m_midiout->Send(0x90, 0x5e,0,-1);
	  if (m_midiout) m_midiout->Send(0x90, 0x5f,0,-1);
	  
	 
      if (m_midiout)
		//if  (!m_is_mcuex)
      {
        UpdateMackieDisplay(0," YAMAHA 01X.....     ***REAPER***     .....YAMAHA 01X  ",56*2);

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

    void OnMIDIEvent(MIDI_event_t *evt)
    {
    #if 0
        char buf[512];
        sprintf(buf,"message %02x, %02x, %02x\n",evt->midi_message[0],evt->midi_message[1],evt->midi_message[2]);
        OutputDebugString(buf);
    #endif
   
        unsigned char onResetMsg[]={0xf0,0x00,0x00,0x66,0x14,0x01,0x58,0x59,0x5a,};
        onResetMsg[4]=m_is_mcuex ? 0x15 : 0x14; 

        if (evt->midi_message[0]==0xf0 && evt->size >= sizeof(onResetMsg) && !memcmp(evt->midi_message,onResetMsg,sizeof(onResetMsg)))
        {
          // on reset
          MCUReset();
          TrackList_UpdateAllExternalSurfaces();
          return;
        }
        if ((evt->midi_message[0]&0xf0) == 0xe0) // volume fader move
        {
          int tid=evt->midi_message[0]&0xf;
          if (tid == 8) tid=0; // master offset, master=0
          else tid+= 1+m_offset+m_allmcus_bank_offset;

          MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);

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
        else if ((evt->midi_message[0]&0xf0) == 0xb0) // pan, jog wheel movement
        {
          if (evt->midi_message[1] >= 0x10 && evt->midi_message[1] < 0x18) // pan
          {
            int tid=evt->midi_message[1]-0x10;

            m_pan_lasttouch[tid&7]=timeGetTime();

            if (tid == 8) tid=0; // adjust for master
            else tid+=1+m_offset+m_allmcus_bank_offset;
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
		  }
      else if (evt->midi_message[1] == 0x3c) // jog wheel
      {
			  if (m_01x_jogstate==1)
			  {
				  if (evt->midi_message[2] == 0x41) SendMessage(g_hwnd,WM_COMMAND,40112,0); // V-Out Zoom
				  else  if (evt->midi_message[2] == 0x01) SendMessage(g_hwnd,WM_COMMAND,40111,0); // V-In Zoom
			  }
			  else if (m_01x_jogstate==2)
			  {
				  if (evt->midi_message[2] == 0x41) SendMessage(g_hwnd,WM_COMMAND,40138,0); // scroll-UP
				  else  if (evt->midi_message[2] == 0x01) SendMessage(g_hwnd,WM_COMMAND,40139,0); // scroll-DN
			  }
			  else if (m_01x_jogstate==3)
			  {
				  if (evt->midi_message[2] == 0x41) SendMessage(g_hwnd,WM_COMMAND,40140,0); // scroll-L
				  else  if (evt->midi_message[2] == 0x01) SendMessage(g_hwnd,WM_COMMAND,40141,0); // scroll-R
			  }
			  else if (m_01x_jogstate==4)
			  {
				  if (evt->midi_message[2] == 0x41) SendMessage(g_hwnd,WM_COMMAND,1011,0); // H-Out Zoom
				  else  if (evt->midi_message[2] == 0x01) SendMessage(g_hwnd,WM_COMMAND,1012,0); // H-In Zoom
			  }
			  else if ((m_01x_jogstate==5) && (m_01x_keystate&2))
			  {
				  if (evt->midi_message[2] == 0x41) SendMessage(g_hwnd,WM_COMMAND,40102,0); // set time sel. left
				  else  if (evt->midi_message[2] == 0x01) SendMessage(g_hwnd,WM_COMMAND,40103,0); // set time sel. right
			  }
			  else if (evt->midi_message[2] >= 0x41)  CSurf_OnRewFwd(0,0x40-(int)evt->midi_message[2]);
			  else if (evt->midi_message[2] > 0 && evt->midi_message[2] < 0x40)  CSurf_OnRewFwd(0,evt->midi_message[2]);
		  }
		
		}
        
		
		else if ((evt->midi_message[0]&0xf0) == 0x90) // button pushes
        {
          int allow_passthru=0;
          if (evt->midi_message[2] >= 0x40)
          {
           	if (evt->midi_message[1] >= 0x2e && evt->midi_message[1] <= 0x31)
            {
              int maxfaderpos=0;
              int movesize=8;
              int x;
              for (x = 0; x < m_01X_list.GetSize(); x ++)
              {
                CSurf_01X *item=m_01X_list.Get(x);
                if (item)
                {
                  if (item->m_offset+8 > maxfaderpos)
                    maxfaderpos=item->m_offset+8;
                }
              }

              if (evt->midi_message[1] & 1) // increase by X
              {
                int msize=CSurf_NumTracks(g_csurf_mcpmode);
                if (movesize>1)
                {
                  if (m_allmcus_bank_offset+maxfaderpos >= msize) return;
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

              for (x = 0; x < m_01X_list.GetSize(); x ++)
              {
                CSurf_01X *item=m_01X_list.Get(x);
                if (item && !item->m_is_mcuex && item->m_midiout)
                {
                  item->m_midiout->Send(0xB0,0x40+11,'0'+(((m_allmcus_bank_offset+1)/10)%10),-1);
                  item->m_midiout->Send(0xB0,0x40+10,'0'+((m_allmcus_bank_offset+1)%10),-1);
                }
              }
			}
            
            else if (evt->midi_message[1] >= 0x20 && evt->midi_message[1] < 0x28) // pan knob push
            {
              int trackid=evt->midi_message[1]-0x20;
              m_pan_lasttouch[trackid]=timeGetTime();

              trackid+=1+m_allmcus_bank_offset+m_offset;


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
			}
            else if (evt->midi_message[1] < 0x08) // rec arm push
            {
              int tid=evt->midi_message[1];
              tid+=1+m_allmcus_bank_offset+m_offset;
              MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
              if (tr)
                CSurf_OnRecArmChange(tr,-1);
            }
            else if (evt->midi_message[1] >= 0x08 && evt->midi_message[1] < 0x18) // mute/solopush
            {
              int tid=evt->midi_message[1]-0x08;
              int ismute=(tid&8);
              tid&=7;
              tid+=1+m_allmcus_bank_offset+m_offset;

              MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
              if (tr)
              {
                if (ismute)
                  CSurf_SetSurfaceMute(tr,CSurf_OnMuteChange(tr,-1),NULL);
                else
                  CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,-1),NULL);
              }
            }
            else if (evt->midi_message[1] >= 0x18 && evt->midi_message[1] <= 0x1f) // sel push
            {
              int tid=evt->midi_message[1]-0x18;
              tid&=7;
              tid+=1+m_allmcus_bank_offset+m_offset;
              MediaTrack *tr=CSurf_TrackFromID(tid,g_csurf_mcpmode);
              if (tr) CSurf_OnSelectedChange(tr,-1); // this will automatically update the surface
            }

			/*else if (evt->midi_message[1] == 0x33) // edit
			Note: same code generated as Shift+F4... boo! */

			else if (evt->midi_message[1] == 0x56) // loop
			{
				if ((m_01x_keystate&2) && (GetSetRepeat(-1)==0))
					{
						SendMessage(g_hwnd,WM_COMMAND,40031,0);//zoom time selection
					}
				SendMessage(g_hwnd,WM_COMMAND,1068,0);// loop
			}
			else if (evt->midi_message[1] == 0x53)// shift
			{
				(m_01x_keystate&1)?(m_01x_keystate^=1):(m_01x_keystate|=1);
			}
			if (evt->midi_message[1] == 0x2a)// PAN CH-PARAM
            {
				(m_01x_keystate&8)?m_01x_keystate^=8:m_01x_keystate|=8;
				if (m_01x_keystate^8) 
				{
					if (m_01x_keystate&128) (m_01x_keystate^=128);
					if (m_01x_keystate&64) (m_01x_keystate^=64);
					if (m_01x_keystate&32) (m_01x_keystate^=32);
					if (m_01x_keystate&16) (m_01x_keystate^=16);
				}
				if (m_midiout) m_midiout->Send(0x90, 0x2a,(m_01x_keystate&8)?0x01:0,-1);
            }
			else if (evt->midi_message[1] == 0x2d) // send
            {
				SendMessage(g_hwnd,WM_COMMAND,40293,0); //track i/o
            }
			else if (evt->midi_message[1] == 0x50) // s-send
            {
				SendMessage(g_hwnd,WM_COMMAND,40251,0); // routing matrix
            }
			else if (evt->midi_message[1] == 0x2b) // s-plugin
			{
				SendMessage(g_hwnd,WM_COMMAND,40291,0);// track FX
            }
			else if (evt->midi_message[1] == 0x4c) // effect
			{
				(SendMessage(g_hwnd,WM_COMMAND,40549,0)); //show mixer inserts
				(SendMessage(g_hwnd,WM_COMMAND,40557,0)); //show mixer sends
			}
			else if (evt->midi_message[1] == 0x34) // name/value
			{
				SendMessage(g_hwnd,WM_COMMAND,m_01x_automode,0);	// cycle automation modes
				(m_01x_automode<=40403)?(m_01x_automode++):(m_01x_automode=40400);
			}
			else if (evt->midi_message[1] == 0x28)// display ^
			{
				SendMessage(g_hwnd,WM_COMMAND,1041,0);// cycle track folder state
			}
			else if (evt->midi_message[1] == 0x29)// display v
			{
				SendMessage(g_hwnd,WM_COMMAND,1042,0);// cycle folder collapsed state
			}
			else if (evt->midi_message[1] == 0x2c) // eq-all
			{
				SendMessage(g_hwnd,WM_COMMAND,40016,0); //preferences
            }
			else if (evt->midi_message[1] == 0x5b) // <<
			{
				if (m_01x_keystate>15)
				{
					(m_01x_keystate&1)?SendMessage(g_hwnd,WM_COMMAND,40376,0):SendMessage(g_hwnd,WM_COMMAND,40416,0);
				}	//prev.transient/select+move to prev.item
				else if (m_01x_keystate&1) 
				{
					(GetSetRepeat(-1)&1)?SendMessage(g_hwnd,WM_COMMAND,40632,0):SendMessage(g_hwnd,WM_COMMAND,40042,0);
				}	//start of loop & proj.start
				else 
				{
					SendMessage(g_hwnd,WM_COMMAND,40646,0);//rew next grid
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
            }
			else if (evt->midi_message[1] == 0x5c) // >>
			{
				if (m_01x_keystate>15)	
				{
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40375:40417,0);
				}	//next transient & sel.move.to next item
				else if (m_01x_keystate&1) 
				{
					SendMessage(g_hwnd,WM_COMMAND,(GetSetRepeat(-1)&1)?40633:40043,0);
				}	//end of loop & end of project
				else 
				{
					SendMessage(g_hwnd,WM_COMMAND,40647,0); // fwd to next grid pos./loop-end
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
			}
			else if (evt->midi_message[1] == 0x59) // write
			{
					SendMessage(g_hwnd,WM_COMMAND,40157,0); //write marker
            }
			else if (evt->midi_message[1] == 0x58) // marker <
			{
				SendMessage(g_hwnd,WM_COMMAND,40172,0); // prev marker
            }
			else if (evt->midi_message[1] == 0x5a)// marker >
			{
				SendMessage(g_hwnd,WM_COMMAND,40173,0);// next marker
            }
			else if (evt->midi_message[1] == 0x46)// undo
			{
				SendMessage(g_hwnd,WM_COMMAND,40029,0); //undo
            }	
			else if (evt->midi_message[1] == 0x47) // shift-undo
			{
				if (m_01x_keystate&128)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,40640,0);// remove item FX
				}
				if (m_01x_keystate&64) 	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,40653,0);// reset item pitch
				}
				if (m_01x_keystate&32)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,40652,0);// reset item rate
				}
				if (m_01x_keystate&16)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,40415,0);// reset selected envelope points to zero/centre
				}
				SendMessage(g_hwnd,WM_COMMAND,40030,0);//redo
            }
			else if (evt->midi_message[1] == 0x48) // save
			{
				(m_01x_keystate&128)?(SendMessage(g_hwnd,WM_COMMAND,40392,0)):(SendMessage(g_hwnd,WM_COMMAND,40026,0));
				//save project or save track-template
            }
			else if (evt->midi_message[1] == 0x49) // shift-save
			{
				(m_01x_keystate&128)?(SendMessage(g_hwnd,WM_COMMAND,40394,0)):(SendMessage(g_hwnd,WM_COMMAND,40022,0));
				//saves-as project or save project-template
            }
			else if (evt->midi_message[1] == 0x36) // f1
			{
				if (m_01x_keystate&8)
				{
					(m_01x_keystate|=128);
					if (m_midiout) m_midiout->Send(0x90, 0x2a,0x7f,-1); 
				}
				else SendMessage(g_hwnd,WM_COMMAND,40001,0); //add track
            }
			else if (evt->midi_message[1] == 0x4d) // shift-f1
			{
				SendMessage(g_hwnd,WM_COMMAND,46000,0); // add template-track
            }
			else if (evt->midi_message[1] == 0x37) // f2
			{
				if (m_01x_keystate&8)
				{
					(m_01x_keystate|=64);
					if (m_midiout) m_midiout->Send(0x90, 0x2a,0x7f,-1); 
				}
				else SendMessage(g_hwnd,WM_COMMAND,40279,0); // docker toggle
				if (m_01x_keystate&1) (m_01x_keystate^=1);
            }
			else if (evt->midi_message[1] == 0x4e) // fshift-f2
			{
				SendMessage(g_hwnd,WM_COMMAND,40078,0); // mixer toggle
            }
			else if (evt->midi_message[1] == 0x38) // f3
			{
				if (m_01x_keystate&8)
				{
					(m_01x_keystate|=32);
					if (m_midiout) m_midiout->Send(0x90, 0x2a,0x7f,-1); 
				}
				else 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40344:40298,0);// toggle fx bypass track/all
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
            }
			else if (evt->midi_message[1] == 0x39) // f4
			{
				if (m_01x_keystate&8)
				{
					(m_01x_keystate|=16);
					if (m_midiout) m_midiout->Send(0x90, 0x2a,0x7f,-1); 
				}
				else { ((m_01x_keystate&4)?m_01x_keystate^=4:m_01x_keystate|=4); 
				(m_01x_keystate&4)?SendMessage(g_hwnd,WM_COMMAND,40490,0):SendMessage(g_hwnd,WM_COMMAND,40491,0);}
            }
			else if (evt->midi_message[1] == 0x33) // shift-f4 or Edit
			{
				SendMessage(g_hwnd,WM_COMMAND,40495,0); // rec-mon cycle
			}
			else if (evt->midi_message[1] == 0x3a) // f5
			{
				if (m_01x_keystate&128)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40530:40529,0);// sel./toggle items
				}
				else if (m_01x_keystate&64)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40207:40206,0);// pitch up/down cent
				}
				else if (m_01x_keystate&32)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40520:40519,0);// itemrate up/down 10cents
				}
				else if (m_01x_keystate&16) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40052:40406,0);// view vol env/toggle vol env active
				}
				else 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40061:40196,0);//split items at edit-play cursor/time sel.
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
			}
			else if (evt->midi_message[1] == 0x3b) // f6
			{
				if (m_01x_keystate&128)
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40639:40638,0);// show item-take FX chain/duplicate item/take
				}
				else if (m_01x_keystate&64) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40205:40204,0);// pitch up/down semitone
				}
				else if	(m_01x_keystate&32) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40518:40517,0);// itemrate up/down semitone
				}
				else if (m_01x_keystate&16) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40053:40407,0);//view pan env/toggle pan env. active
				}
				else 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40202:40109,0);// open pri./sec. editor
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
            }
			else if (evt->midi_message[1] == 0x3c) // f7
			{
				if (m_01x_keystate&128)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40209:40361,0);// apply track FX to item (mono O/P)/apply track fx to item (stereo O/P)
				}
				else if (m_01x_keystate&64) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40516:40515,0);// pitch up/down octave
				}
				else if (m_01x_keystate&32) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40525:40524,0);// playrate up/down 10cents
				}
				else if (m_01x_keystate&16) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40050:40408,0);// view pre-fx vol/pan envs
				}
				else 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40642:40643,0);// explode takes in order/place
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
			}
			else if (evt->midi_message[1] == 0x3d) // f8
			{
				if (m_01x_keystate&128)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40011:40009,0);// view item properties/source properties
				}
				else if (m_01x_keystate&64) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40637:40377,0);// view virt.midi-keyb./all input to vkb
				}
				else if (m_01x_keystate&32)	
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40523:40522,0);// transport playrate up/down semitone
				}
				else if (m_01x_keystate&16) 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40051:40409,0);//view pre-fx pan env/toggle pre-fx vol.pan active
				}
				else 
				{ 
					SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&1)?40011:40009,0);// view item properties/source properties
				}
				if (m_01x_keystate&1) (m_01x_keystate^=1);
	         }
			else if (evt->midi_message[1] == 0x64) // zoom
			{
				(m_01x_keystate&2)?m_01x_keystate^=2:m_01x_keystate|=2;
				if (m_01x_keystate^2) (m_01x_jogstate=0);
				if (m_midiout) m_midiout->Send(0x90, 0x64,(m_01x_keystate&2)?0x01:0,-1);	
			}
			else if (evt->midi_message[1] == 0x60) // ^
			{
				if (m_01x_keystate&2)
				{
					(m_01x_jogstate=1);
					if (m_midiout) m_midiout->Send(0x90, 0x64,0x7f,-1);
				}
				else if (m_01x_keystate&128)		//mode f1
				{
					SendMessage(g_hwnd,WM_COMMAND,40117,0);	//Move items up one track
				}
				else SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate>15)?40418:40286,0); //sel.&move to item in prev track
			}
			else if (evt->midi_message[1] == 0x61) // v
			{
				if (m_01x_keystate&2)
				{
					(m_01x_jogstate=2);
					if (m_midiout) m_midiout->Send(0x90, 0x64,0x7f,-1); // mode led
				}
				else if (m_01x_keystate&128)
				{
					SendMessage(g_hwnd,WM_COMMAND,40118,0);	//Move items down one track
				}
				else SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate>16)?40419:40285,0); 
			}		//sel.&move-to item in next track / next track
			else if (evt->midi_message[1] == 0x62) // <
			{
				if (m_01x_keystate&2)
				{
					(m_01x_jogstate=3);
					if (m_midiout) m_midiout->Send(0x90, 0x64,0x7f,-1);
				}
				else SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&128)?40120:40416,0);	
			}		//Move items L/sel.move-to prev item	
			else if (evt->midi_message[1] == 0x63) // >
			{
				if (m_01x_keystate&2)
				{
					(m_01x_jogstate=4);
					if (m_midiout) m_midiout->Send(0x90, 0x64,0x7f,-1);
				}
				else SendMessage(g_hwnd,WM_COMMAND,(m_01x_keystate&128)?40119:40417,0);	
			}		//Move items R/sel.move-to next item
            else if (evt->midi_message[1] == 0x5f) // rec
            {
              SendMessage(g_hwnd,WM_COMMAND,1013,0);
            }
            else if (evt->midi_message[1] == 0x5e) // play
            {
              SendMessage(g_hwnd,WM_COMMAND,1007,0);
            }
            else if (evt->midi_message[1] == 0x5d) // stop
            {
				SendMessage(g_hwnd,WM_COMMAND,1016,0);
			}
            else if (evt->midi_message[1] == 0x65) // scrub button
            {
				(m_01x_jogstate!=5)?m_01x_jogstate=5:m_01x_jogstate=0;
				if (m_midiout) m_midiout->Send(0x90, 0x65,(m_01x_jogstate==5)?0x7f:0,-1);
            }
			
			else if (evt->midi_message[1] == 0x32) // flip button
            {
              m_flipmode=!m_flipmode;
              if (m_midiout) m_midiout->Send(0x90, 0x32,m_flipmode?1:0,-1);
              CSurf_ResetAllCachedVolPanStates();
              TrackList_UpdateAllExternalSurfaces();
            }
           else
            {
              allow_passthru=1;
            }
          }
          else if (evt->midi_message[1] >= 0x68 && evt->midi_message[1] < 0x71) // touch state
          {
            m_fader_touchstate[evt->midi_message[1]-0x68]=evt->midi_message[2]>0x40;
          }
          else if (allow_passthru && evt->midi_message[2]>=0x40)
          {
            int a=evt->midi_message[1];
            MIDI_event_t evt={0,3,{0xbf-(m_mackie_modifiers&15),a,0}};
            kbd_OnMidiEvent(&evt,-1);
          }
        }
	}


public:

    CSurf_01X(bool ismcuex, int offset, int size, int indev, int outdev, int *errStats) 
    {
      m_01X_list.Add(this);

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

    }
    ~CSurf_01X() 
    {
      m_01X_list.Delete(m_01X_list.Find(this));
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
    }

    const char *GetTypeString() { return "01X";}
    const char *GetDescString()
    {
      descspace.SetFormatted(512,__LOCALIZE_VERFMT("YAMAHA 01X (dev %d,%d)","csurf"),m_midi_in_dev,m_midi_out_dev);
      return descspace.Get();     
    }
    const char *GetConfigString() // string of configuration data
    {
      sprintf(configtmp,"%d %d %d %d",m_offset,m_size,m_midi_in_dev,m_midi_out_dev);      
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
      const DWORD now=timeGetTime();
      if ((now - m_frameupd_lastrun) >= (1000/max((*g_config_csurf_rate),1)))
      {
        m_frameupd_lastrun=now;

        if (m_midiout)
        {
          if (!m_is_mcuex)
          {
            /* JF> this doesn't appear to do anything, oops
            double pp=(GetPlayState()&1) ? GetPlayPosition() : GetCursorPosition();
            unsigned char bla[10];
            memset(bla,0,sizeof(bla));
            */
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
              int idx=m_offset+m_allmcus_bank_offset+x+1;
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
      if (m_midiout)
      {
        int x;
        for (x = 0; x < 8; x ++)
        {
          MediaTrack *t=CSurf_TrackFromID(x+m_offset+m_allmcus_bank_offset+1,g_csurf_mcpmode);
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
#define FIXID(id) const int oid=CSurf_TrackToID(trackid,g_csurf_mcpmode); int id=oid; \
  if (id>0) { id -= m_offset+m_allmcus_bank_offset+1; if (id==8) id=-1; } else if (id==0) id=8; 

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
      FIXID(id)
      if (m_midiout && id>=0 && id < 256 && id < m_size)
      {
        if (id<8)
          m_midiout->Send(0x90, 0x18+(id&7),selected?0x7f:0,-1);
      }
    }

    void SetSurfaceSolo(MediaTrack *trackid, bool solo) 
    { 
      FIXID(id)
      if (m_midiout && id>=0 && id < 256 && id < m_size)
      {
        if (id < 8)
          m_midiout->Send(0x90, 0x08+(id&7),solo?1:0,-1); //blink
        else if (id == 8)
          m_midiout->Send(0x90, 0x73,solo?1:0,-1);
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
      if (m_midiout)
      {
        m_midiout->Send(0x90, 0x5f,rec?0x7f:0,-1);
        m_midiout->Send(0x90, 0x5e,play?0x7f:0,-1);
        m_midiout->Send(0x90, 0x5d,pause?0x7f:0,-1);
      }
    }
   void SetRepeatState(bool rep) 
    {
      if (m_midiout)
	  {
		  m_midiout->Send(0x90, 0x56,rep?0x7f:0,-1);
	  }
    }

   void SetTrackTitle(MediaTrack *trackid, const char *title) 
    {
      FIXID(id)
      if (m_midiout && id >= 0 && id < 8)
      {
        char buf[7];
        memcpy(buf,title,6);
        buf[6]=0;
        UpdateMackieDisplay(id*7,buf,7);
      }
    }
    bool GetTouchState(MediaTrack *trackid, int isPan) 
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
        return !!m_fader_touchstate[id];
  
      return false; 
    }
   void ResetCachedVolPanStates() 
    { 
      memset(m_vol_lastpos,0xff,sizeof(m_vol_lastpos));
      memset(m_pan_lastpos,0xff,sizeof(m_pan_lastpos));
    }
    void OnTrackSelection(MediaTrack *trackid) 
    { 
      int tid=CSurf_TrackToID(trackid,g_csurf_mcpmode);
      // if no normal MCU's here, then slave it
      int x;
      int movesize=8;
      for (x = 0; x < m_01X_list.GetSize(); x ++)
      {
        CSurf_01X *item=m_01X_list.Get(x);
        if (item)
        {
          if (item->m_offset+8 > movesize)
            movesize=item->m_offset+8;
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
          for (x = 0; x < m_01X_list.GetSize(); x ++)
          {
            CSurf_01X *item=m_01X_list.Get(x);
            if (item && !item->m_is_mcuex && item->m_midiout)
            {
              item->m_midiout->Send(0xB0,0x40+11,'0'+(((m_allmcus_bank_offset+1)/10)%10),-1);
              item->m_midiout->Send(0xB0,0x40+10,'0'+((m_allmcus_bank_offset+1)%10),-1);
            }
          }
        }
      }
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

  return new CSurf_01X(!strcmp(type_string,"MCUEX"),parms[0],parms[1],parms[2],parms[3],errStats);
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

        sprintf(tmp,"%d %d %d %d",offs,size,indev,outdev);
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


reaper_csurf_reg_t csurf_01X_reg = 
{
  "01X",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "YAMAHA 01X",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
