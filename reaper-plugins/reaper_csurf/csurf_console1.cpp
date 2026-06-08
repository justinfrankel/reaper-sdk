/*
** reaper_csurf
** Console1 support
** Copyright (C) 2007-2025 Cockos Incorporated
** License: LGPL
*/

#include "csurf.h"
#include "../../WDL/lineparse.h"
#include "../../WDL/jsonparse.h"

#define METER_INTERVAL 100
#define MAX_SENDS 6
#define SEND_INTERVAL 50

class CSurf_Console1;
static CSurf_Console1 *s_inst; // only one instance of this surface is supported!

struct devpair {
  devpair(const char *n)
  {
    name.Set(n);
    indev_idx = outdev_idx = -1;
    indev = NULL;
    outdev = NULL;
  }
  ~devpair()
  {
    DELETE_ASYNC(indev);
    DELETE_ASYNC(outdev);
  }
  WDL_FastString name;
  int indev_idx, outdev_idx;
  midi_Input *indev;
  midi_Output *outdev;
};


static void findDevices(WDL_PtrList<devpair> *list)
{
  for (int isinput = 0; isinput < 2; isinput ++)
  {
    int n = isinput ? GetNumMIDIInputs() : GetNumMIDIOutputs();
    for (int dev=0; dev < n; ++dev)
    {
      char buf[512];
      if (isinput && !GetMIDIInputNameNoAlias(dev, buf, sizeof(buf))) continue;
      if (!isinput && !GetMIDIOutputNameNoAlias(dev, buf, sizeof(buf))) continue;

#if defined(__APPLE__)
      if (!strstr(buf,"DAW")) continue;
      if (strlen(buf) >= 5 && strcmp(buf + strlen(buf) - 5, " MIDI") == 0) continue; // ignore ending in MIDI
#elif defined(_WIN32)
      if (strstr(buf, "MIDIIN") || strstr(buf, "MIDIOUT")) continue;
#endif

      if (strncmp(buf,"Softube",7))
      {
        bool is_likely = (strstr(buf, "Console 1") && strstr(buf,"Mk III")) || strstr(buf,"Softube");
        if (is_likely && !strstr(buf,"Fader") && !strstr(buf,"Channel")) is_likely = false;
        if (!is_likely) continue;
      }

      devpair *l = NULL;
      for (int x = 0; ; x ++)
      {
        l = list->Get(x);
        if (!l || !strcmp(l->name.Get(),buf)) break;
      }
      if (!l) list->Add(l = new devpair(buf));
      if (isinput) l->indev_idx = dev;
      else l->outdev_idx = dev;
    }
  }
}

struct trackinf
{
  trackinf(MediaTrack *tr, int idx)
  {
    last_tr = tr;
    lastidx = idx;
    lastidx_sent = -1000;
    lastmeter[0] = -1.0;
    lastmeter[1] = 0.0;
    vol_touch = pan_touch = false;
    for (int x = 0; x < MAX_SENDS; x ++)
    {
      sendstates[x] = -1.0;
      sendok[x] = false;
    }
    color = -1;
    chans = 0;
    maxvol = -1.0;
    vol = -1.0;
    pan = -1000.0;
    mute = false;
    solo = false;
  }
  ~trackinf() { }

  static void dispose(trackinf *d) { delete d; }

  MediaTrack *last_tr; // avoid using! could be stale
  int lastidx, lastidx_sent; // -1 if master
  WDL_FastString name;
  int color, chans;
  float vol, pan;
  bool vol_touch, pan_touch, mute, solo;
  float lastmeter[2],maxvol;
  float sendstates[MAX_SENDS];
  bool sendok[MAX_SENDS];
};

static bool stringToGuid(const char *str, GUID *g)
{
  int Data1, Data2, Data3;
  int Data4[8];
  int n = sscanf( str, "{ %08x - %04x - %04x - %02x%02x - %02x%02x%02x%02x%02x%02x}",
    &Data1, &Data2, &Data3, Data4 + 0, Data4 + 1,
    Data4 + 2, Data4 + 3, Data4 + 4, Data4 + 5, Data4 + 6, Data4 + 7 );

  if (n != 11) return false;

  g->Data1 = Data1;
  g->Data2 = Data2;
  g->Data3 = Data3;
  g->Data4[0] = Data4[0];
  g->Data4[1] = Data4[1];
  g->Data4[2] = Data4[2];
  g->Data4[3] = Data4[3];
  g->Data4[4] = Data4[4];
  g->Data4[5] = Data4[5];
  g->Data4[6] = Data4[6];
  g->Data4[7] = Data4[7];
  return true;
}


class CSurf_Console1 : public IReaperControlSurface
{
  WDL_FastString m_desc;
  midi_Input *m_midi_in; // these reference m_potential_devices
  midi_Output *m_midi_out;
  int m_midi_in_devidx, m_midi_out_devidx;
  WDL_FastString m_midi_devname;
  WDL_HeapBuf m_sendstr; // composed MIDI_event_t
  WDL_FastString m_tmp; // current message (this could be updated to include the MIDI_event_t header)
  wdl_json_parser m_jsonparse;

  WDL_PtrList<devpair> m_potential_devices; // kept open, owning the devices
  DWORD m_scan_start_t;
  DWORD m_last_meter_t,m_last_send_t;

  WDL_MemKeyedArray<GUID, trackinf*> m_tracks; // GUID-keyed track list
  MediaTrack *m_last_sel;

public:

  CSurf_Console1(int *errStats) : m_tracks(trackinf::dispose)
  {
    s_inst = this;
    m_midi_in = NULL;
    m_midi_out = NULL;
    m_midi_in_devidx = m_midi_out_devidx = -1;
    m_last_meter_t = m_last_send_t = GetTickCount();
    m_scan_start_t = 0;
    findDevices(&m_potential_devices);

    int errstate = 0;
    for (int x = 0; x < m_potential_devices.GetSize(); x ++)
    {
      devpair *d = m_potential_devices.Get(x);
      if (d->indev_idx < 0 || d->outdev_idx<0)
      {
        m_potential_devices.Delete(x--,true);
        continue;
      }
      d->indev = CreateMIDIInput(d->indev_idx);
      if (!d->indev)
      {
        errstate |= 1;
        m_potential_devices.Delete(x--,true);
        continue;
      }
      midi_Output *out=CreateMIDIOutput(d->outdev_idx, false, NULL);
      if (out) d->outdev = CreateThreadedMIDIOutput(out);
      if (!out)
      {
        errstate |= 2;
        m_potential_devices.Delete(x--,true);
        continue;
      }
      d->indev->start();
    }
    sendHandshakes();

    if (m_potential_devices.GetSize() < 1)
    {
      if (errstate)
      {
        m_desc.Set(__LOCALIZE("Console1 Mk III (error opening devices)","csurf"));
        if (errStats) *errStats |= errstate;
      }
      else
        m_desc.Set(__LOCALIZE("Console1 Mk III (no matching devices found)","csurf"));
    }
    else
      m_desc.SetFormatted(512, __LOCALIZE_VERFMT("Console1 Mk III (scanning %d device pairs)","csurf"), m_potential_devices.GetSize());
  }

  void sendHandshakes()
  {
    m_scan_start_t = GetTickCount();
    for (int x = 0; x < m_potential_devices.GetSize(); x ++)
    {
      devpair *d = m_potential_devices.Get(x);
      SendCommand("\"handshake\": { \"dawName\": \"REAPER\", \"protocolVersion\": [ 1 ] }", d->outdev);
    }
  }

  ~CSurf_Console1()
  {
    if (m_midi_out) SendCommand("\"cmd\": \"RESET\"");
    m_midi_in = NULL;
    m_midi_out = NULL;
    m_potential_devices.Empty(true);
    s_inst = NULL;
  }

  const char *GetTypeString() { return "CONSOLE1"; }
  const char *GetDescString() { return m_desc.Get(); }
  const char *GetConfigString() { return "0"; }

  void SendCommand(const char *str, midi_Output *outdev=NULL) // always adds { }
  {
    int sl = (int)strlen(str);
    MIDI_event_t *evt = (MIDI_event_t*)m_sendstr.ResizeOK(sizeof(MIDI_event_t) + 9 + sl, false);
    evt->frame_offset = -1;
    evt->size = 9+sl;
    memcpy(evt->midi_message, "\xF0\x7Dstc1{", 7);
    memcpy(evt->midi_message+7, str, sl);
    evt->midi_message[7+sl] = '}';
    evt->midi_message[7+sl+1] = 0xf7;

#ifdef _DEBUG
    {
      static wdl_json_parser p;
      wdl_json_element *e = p.parse((const char *)evt->midi_message+6, sl+2);
      if (!e) wdl_log("error parsing msg that we will send: {%s}\n",str);
      WDL_ASSERT(e != NULL);
      p.dispose_element(e);
    }
#endif

    if (outdev) outdev->SendMsg(evt,-1);
    else if (m_midi_out) m_midi_out->SendMsg(evt,-1);
  }

  void CloseNoReset()
  {
    m_midi_in = NULL;
    m_midi_out = NULL;
    m_potential_devices.Empty(true);
  }

  void Run() // 30Hz
  {
    const DWORD t = timeGetTime();

    // check inputs for a handshake responses, reset messages
    for (int x = 0; x < m_potential_devices.GetSize(); x ++)
    {
      devpair *d = m_potential_devices.Get(x);
      d->indev->SwapBufs(t);
      if (d->indev == m_midi_in) continue;

      MIDI_eventlist *evtlist=d->indev->GetReadBuf();
      if (evtlist)
      {
        int bpos=0;
        MIDI_event_t *evt;
        while ((evt=evtlist->EnumItems(&bpos)))
        {
          if (evt->size>=7 &&
              WDL_NORMALLY(!memcmp(evt->midi_message,"\xf0\x7dstc1",6)) &&
              WDL_NORMALLY(evt->midi_message[evt->size-1]==0xf7))
          {
            wdl_json_element *e = m_jsonparse.parse((const char*)evt->midi_message+6,evt->size-7);
            if (e)
            {
              if (e->is_object())
              {
                wdl_json_element *cmd = e->get_item_by_name("handshake");
                if (cmd && cmd->is_object())
                {
                  wdl_json_element *ack = cmd->get_item_by_name("ack");
                  if (ack && ack->m_value && !strcmp(ack->m_value,"true"))
                  {
                    m_midi_in_devidx = d->indev_idx;
                    m_midi_out_devidx = d->outdev_idx;
                    m_midi_devname.Set(d->name.Get());
                    m_midi_in = d->indev;
                    m_midi_out = d->outdev;

                    m_desc.SetFormatted(512, __LOCALIZE_VERFMT("Console1 Mk III (devs %d,%d - %s)","csurf"),
                        m_midi_in_devidx,m_midi_out_devidx,m_midi_devname.Get());
                    OnReset();
                  }
                }

                cmd = e->get_item_by_name("cmd");
                if (cmd && cmd->m_value)
                {
                  wdl_log("csurf_console1: got cmd from other device %s\n",cmd->m_value);
                  if (!strcmp(cmd->m_value,"RESET"))
                  {
                    m_midi_in = NULL;
                    m_midi_out = NULL;
                    sendHandshakes();
                  }
                }
              }
              m_jsonparse.dispose_element(e);
            }
          }
        }
      }
    }

    if (!m_midi_in || !m_midi_out)
    {
      if ((GetTickCount() - m_scan_start_t) > 5000)
      {
        m_desc.Set(__LOCALIZE("Console1 Mk III (no devices responded)","csurf"));
      }
      return;
    }

    MIDI_eventlist *evtlist=m_midi_in->GetReadBuf();
    int bpos=0;
    MIDI_event_t *evt;
    while ((evt=evtlist->EnumItems(&bpos)))
    {
      ProcessMIDI(evt);
    }

    const DWORD now = GetTickCount();
    const bool do_meters = (now-m_last_meter_t) >= METER_INTERVAL;
    const bool do_sends = (now-m_last_send_t) >= SEND_INTERVAL;
    if (do_meters) m_last_meter_t = now;
    if (do_sends) m_last_send_t = now;

    if (do_meters || do_sends)
    {
      const int nt = GetNumTracks();
      for (int x = 0; x <= nt; x ++)
      {
        MediaTrack *tr = x==nt ? GetMasterTrack(NULL) : GetTrack(NULL,x);
        if (!tr) continue;
        GUID *guid=(GUID*)GetSetMediaTrackInfo((INT_PTR)tr, "GUID", NULL);
        if (WDL_NORMALLY(guid))
        {
          int sendstate = 0;
          m_tmp.Set("");
          trackinf *inf = m_tracks.Get(*guid);
          if (do_meters && WDL_NORMALLY(inf))
          {
            double pk1 = Track_GetPeakInfo(tr,0), pk2 = Track_GetPeakInfo(tr,1);
            if (fabs(pk1 - inf->lastmeter[0]) > 0.01 || fabs(pk2 - inf->lastmeter[1]) > 0.01)
            {
              if (!sendstate++) addField("trackId", guid);
              m_tmp.AppendFormatted(128,",\"meter\":[%.4f,%.4f]",pk1,pk2);
              inf->lastmeter[0]=pk1;
              inf->lastmeter[1]=pk2;
            }
          }
          if (do_sends && WDL_NORMALLY(inf))
          {
            const bool newsendmode = GetTrackNumSends(tr,0x10000001)>=0x10000000;
            const int sendflag = newsendmode ? 0x10000000 : 0;
            for (int x = 0; x < MAX_SENDS; x ++)
            {
              double sendv = 0.0;
              bool ok = GetTrackSendUIVolPan(tr,x | sendflag,&sendv,NULL);

              const float sendf = (float)sendv;
              if (ok != inf->sendok[x] || inf->sendstates[x] != sendf)
              {
                inf->sendstates[x] = sendf;
                inf->sendok[x] = ok;
                if (!sendstate++) addField("trackId", guid);
                char field1[16];
                snprintf(field1,sizeof(field1),"send%d",x+1);
                addFieldVolume(field1,inf->sendstates[x]);

                lstrcatn(field1,"On",sizeof(field1));
                addFieldBool(field1,inf->sendok[x]);
              }
            }
          }
          if (x == nt) // master track
          {
            const bool solo = !!(GetMasterMuteSoloFlags()&2);
            if (inf->solo != solo)
            {
              if (!sendstate++) addField("trackId", guid);
              addFieldBool("solo", inf->solo = solo);
            }
          }
          if (sendstate) SendCommand(m_tmp.Get());
        }
      }
    }
  }

  void processEvent(const wdl_json_element *elem, trackinf *trackinfo)
  {
    MediaTrack *tr = trackinfo->last_tr;
    for (int x = 0; ; x ++)
    {
      const wdl_json_element *v = elem->enum_item(x);
      if (!v) break;
      const char *n = elem->enum_item_name(x);
      if (!n) break;

      const char *p = v->m_value;
      if (p && *p)
      {
        if (!strcmp(n,"volume"))
        {
          const wdl_json_element *ts = elem->get_item_by_name("touchState");
          if (ts && ts->m_value && !strcmp(ts->m_value,"TOUCHED")) trackinfo->vol_touch = true;
          double val = !strnicmp(p,"-Inf",4) ? 0.0 : DB2VAL(atof(p));
          CSurf_SetSurfaceVolume(tr, CSurf_OnVolumeChange(tr,val,false),NULL);
          if (ts && ts->m_value && !strcmp(ts->m_value,"RELEASED")) trackinfo->vol_touch = false;
          continue;
        }
        if (!strcmp(n,"pan"))
        {
          const wdl_json_element *ts = elem->get_item_by_name("touchState");
          if (ts && ts->m_value && !strcmp(ts->m_value,"TOUCHED")) trackinfo->pan_touch = true;
          double val = atof(p)*2.0 - 1.0;
          CSurf_SetSurfacePan(tr, CSurf_OnPanChange(tr,val,false),NULL);
          if (ts && ts->m_value && !strcmp(ts->m_value,"RELEASED")) trackinfo->pan_touch = false;
          continue;
        }
        if (!strcmp(n,"mute") || !strcmp(n,"solo") || !strcmp(n,"selected"))
        {
          const int v = !strcmp(p,"true") ? 1 : !strcmp(p,"false") ? 0 : -1;
          if (v>=0)
          {
            if (n[0]=='m') CSurf_SetSurfaceMute(tr,CSurf_OnMuteChange(tr,v),NULL);
            else if (n[1] == 'e')
            {
              if (v)
              {
                CSurf_OnSelectedChange(tr,1000 /* exclusive */);
                m_last_sel = tr;
              }
              else if (m_last_sel == tr) m_last_sel = NULL;
            }
            else CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,v),NULL);
            continue;
          }
        }
        int sendidx;
        if (!strncmp(n,"send",4) && (sendidx=atoi(n+4))>0 && sendidx <= MAX_SENDS)
        {
          const wdl_json_element *ts = elem->get_item_by_name("touchState");
          int endflag = 0;
          if (ts && ts->m_value && !strcmp(ts->m_value,"RELEASED")) endflag = 1;
          double val = !strnicmp(p,"-Inf",4) ? 0.0 : DB2VAL(atof(p));

          const bool newsendmode = GetTrackNumSends(tr,0x10000001)>=0x10000000;
          const int sendflag = newsendmode ? 0x10000000 : 0;
          SetTrackSendUIVol(tr,(sendidx-1) | sendflag,val,endflag);
          continue;
        }
        if (!strcmp(n,"plugin") && p && *p && tr)
        {
          // the addition of VST3: prefixes might not be necessary, but force it anyway for known names
          if (!strcmp(p,"Console 1")) TrackFX_AddByName(tr, "VST3:Console 1", false, 1);
          else if (!strcmp(p,"Flow Mixing Suite")) TrackFX_AddByName(tr, "VST3:Flow Mixing Suite", false, 1);
          else TrackFX_AddByName(tr, p, false, 1);
          continue;
        }
      }
#ifdef _DEBUG
      if (strcmp(n,"touchState") &&
          strcmp(n,"trackId"))
        wdl_log("console1: received unknown item \"%s\":\"%s\"\n",n,v->m_value);
#endif
    }
  }

  void ProcessMIDI(MIDI_event_t *evt)
  {
    if (evt->size>=7 &&
        WDL_NORMALLY(!memcmp(evt->midi_message,"\xf0\x7dstc1",6)) &&
        WDL_NORMALLY(evt->midi_message[evt->size-1]==0xf7))
    {
      wdl_json_element *e = m_jsonparse.parse((const char*)evt->midi_message+6,evt->size-7);
      if (e)
      {
        if (e->is_object())
        {
          wdl_json_element *cmd = e->get_item_by_name("cmd");
          if (cmd && cmd->m_value)
          {
            wdl_log("csurf_console1: got cmd from main device %s\n",cmd->m_value);
            if (!strcmp(cmd->m_value,"PING"))
            {
              SendCommand("\"cmd\": \"PONG\"");
            }
            else if (!strcmp(cmd->m_value,"RESET"))
            {
              OnReset();
            }
          }
          else
          {
            wdl_json_element *trackid = e->get_item_by_name("trackId");
            GUID g;
            if (trackid && trackid->m_value && *trackid->m_value == '{' && stringToGuid(trackid->m_value,&g))
            {
              trackinf *inf = m_tracks.Get(g);
              if (inf && WDL_NORMALLY(inf->last_tr))
              {
                MediaTrack *tr = inf->lastidx == -1 ? GetMasterTrack(NULL) : GetTrack(NULL,inf->lastidx);
                if (WDL_NORMALLY(tr == inf->last_tr))
                {
                  processEvent(e,inf);
                }
                else
                {
                  // inconsistency, re-scan
                  RescanTracks();
                  inf = m_tracks.Get(g);
                  if (inf) processEvent(e,inf);
                }
              }
            }
          }
        }
        m_jsonparse.dispose_element(e);
      }
    }
  }

  void addSep()
  {
    if (m_tmp.GetLength() > 0 &&
        m_tmp.Get()[m_tmp.GetLength()-1] != '[' &&
        m_tmp.Get()[m_tmp.GetLength()-1] != '{') m_tmp.Append(",");

  }
  void addField(const char *field, const char *val)
  {
    if (!val) return;

    addSep();
    m_tmp.Append("\"");
    m_tmp.Append(field);
    m_tmp.Append("\": \"");
    while (*val)
    {
      int c=0, l=wdl_utf8_parsechar(val,&c);
      if (c<128) m_tmp.Append(val,1);
      else if (c<=0xffff) m_tmp.AppendFormatted(32,"\\u%04x",c);
      val+=l;
    }
    m_tmp.Append("\"");
  }
  void addField(const char *field, int val)
  {
    addSep();
    m_tmp.Append("\"");
    m_tmp.Append(field);
    m_tmp.AppendFormatted(64,"\": %d",val);
  }

  void addField(const char *field, double val)
  {
    addSep();
    m_tmp.Append("\"");
    m_tmp.Append(field);
    m_tmp.AppendFormatted(64,"\": %.10f",val);
  }
  void addField(const char *field, const GUID *val)
  {
    if (!val) return;
    char str[64];
    guidToString((GUID*)val,str);
    addField(field,str);
  }
  void addFieldBool(const char *field, bool v)
  {
    addSep();
    m_tmp.Append("\"");
    m_tmp.Append(field);
    m_tmp.AppendFormatted(64,"\": %s",v?"true":"false");
  }
  void addFieldVolume(const char *field, double volume)
  {
    if (volume <= 0.0) addField(field,"-Infinity");
    else addField(field, VAL2DB(volume));
  }

  bool beginForTrack(MediaTrack *tr, trackinf **inf=NULL)
  {
    if (!m_midi_out) return false;
    if (!tr) return false;
    GUID *guid=(GUID*)GetSetMediaTrackInfo((INT_PTR)tr, "GUID", NULL);
    if (!guid) return false;
    if (inf)
    {
      *inf = m_tracks.Get(*guid);
      if (!*inf) return false;
    }

    m_tmp.Set("");
    addField("trackId", guid);
    return true;
  }

  void OnReset()
  {
    m_last_sel = NULL;
    m_tracks.DeleteAll();
    // controller reset from fresh state
    RescanTracks();
  }

  void RescanTracks()
  {
    if (!m_midi_out)
    {
      m_last_sel = NULL;
      m_tracks.DeleteAll();
      return;
    }
    // garbage collect too
    for (int x = 0; ; x ++)
    {
      GUID k;
      trackinf *inf = m_tracks.Enumerate(x,&k);
      if (!inf) break;
      inf->lastidx = -2;
      inf->last_tr = NULL;
    }

    const int nt = GetNumTracks();
    for (int x = 0; x <= nt; x ++)
    {
      MediaTrack *tr = x==nt ? GetMasterTrack(NULL) : GetTrack(NULL,x);
      if (x < nt)
      {
        const bool *tcp = (const bool *)GetSetMediaTrackInfo((INT_PTR)tr,"B_SHOWINTCP",NULL);
        if (tcp && !*tcp)
        {
          const bool *mcp = (const bool *)GetSetMediaTrackInfo((INT_PTR)tr,"B_SHOWINMIXER",NULL);
          if (mcp && !*mcp) continue;
        }
      }
      GUID *guid=(GUID*)GetSetMediaTrackInfo((INT_PTR)tr, "GUID", NULL);
      if (WDL_NORMALLY(guid))
      {
        trackinf *inf = m_tracks.Get(*guid);
        if (inf)
        {
          inf->lastidx = x==nt ? -1 : x;
          inf->last_tr = tr;
        }
        else
        {
          trackinf *f = new trackinf(tr, x==nt ? -1 : x);
          m_tracks.Insert(*guid, f);
        }
      }
    }

    // remove stale tracks
    for (int x = 0; ; x ++)
    {
      GUID k;
      trackinf *inf = m_tracks.Enumerate(x,&k);
      if (!inf) break;
      if (inf->last_tr == NULL)
      {
        m_tmp.Set("");
        addField("trackId", &k);
        addFieldBool("isActive",false);
        SendCommand(m_tmp.Get());
        m_tracks.DeleteByIndex(x--);
      }
      else
      {
        MediaTrack *tr = inf->last_tr;
        const char *name = inf->lastidx == -1 ? "master" : (const char*)GetSetMediaTrackInfo((INT_PTR)tr, "P_NAME", NULL);
        int *color = (int *)GetSetMediaTrackInfo((INT_PTR)tr,"I_CUSTOMCOLOR",NULL);
        if (WDL_NOT_NORMALLY(!name)) continue;

        m_tmp.Set("");
        addField("trackId", &k);
        const int origlen = m_tmp.GetLength();

        const int idx = inf->lastidx == -1 ? 1+nt : 1+inf->lastidx;
        if (idx != inf->lastidx_sent)
        {
          if (inf->lastidx_sent == -1000)
          {
            addFieldBool("isActive",true);
            addField("meterType","PEAK");
          }
          addField("track", inf->lastidx_sent=idx);
        }
        if (strcmp(inf->name.Get(),name))
        {
          inf->name.Set(name);
          addField("name",name);
        }
        const int use_color = color && (*color&0x1000000) ? *color : RGB(0xa0,0xa0,0xa0);
        if (inf->color != use_color)
        {
          inf->color = use_color;
          addField("color", (int) (GetRValue(use_color) | (GetGValue(use_color)<<8) | (GetBValue(use_color)<<16)));
        }
        const int *nc = (const int *)GetSetMediaTrackInfo((INT_PTR)tr,"I_NCHAN",NULL);
        const int use_nc = nc ? wdl_clamp(*nc,2,REAPER_MAX_CHANNELS) : 2;
        if (use_nc != inf->chans) addField("channels", inf->chans = use_nc);

        const float maxv = g_slider_maxvol ? (float)*g_slider_maxvol : 12.0f;
        if (maxv != inf->maxvol)
        {
          inf->maxvol = maxv;
          addField("maxVolumeValue", (double)maxv);
          addField("maxSendValue", (double)maxv);
        }

        // these aren't usually necessary, but when re-initializing control surfaces, the delayed init
        // of the negotiation can cause them to be missed
        double volume=1.0, pan=0.0;
        if (GetTrackUIVolPan(tr,&volume,&pan))
        {
          float fvol = (float) volume;
          float fpan = (float) pan;
          if (inf->vol != fvol) addFieldVolume("volume", inf->vol = fvol);
          if (inf->pan != fpan) addField("pan", 0.5+(inf->pan = fpan)*0.5);
        }
        bool mute=false;
        if (GetTrackUIMute(tr,&mute) && mute != inf->mute) addFieldBool("mute", inf->mute = mute);

        bool solo = false;
        if (x == nt)
        {
          solo = !!(GetMasterMuteSoloFlags()&2);
        }
        else
        {
          int *soloint = (int *)GetSetMediaTrackInfo((INT_PTR)tr,"I_SOLO",NULL);
          if (soloint) solo = *soloint > 0 && *soloint < 5;
        }
        if (solo != inf->solo) addFieldBool("solo",inf->solo = solo);

        const bool newsendmode = GetTrackNumSends(tr,0x10000001)>=0x10000000;
        const int sendflag = newsendmode ? 0x10000000 : 0;
        for (int x = 0; x < MAX_SENDS; x ++)
        {
          double sendv = 0.0;
          bool ok = GetTrackSendUIVolPan(tr,x | sendflag, &sendv,NULL);
          const float sendvf = (float) sendv;

          char field1[16];
          snprintf(field1,sizeof(field1),"send%d",x+1);
          if (sendvf != inf->sendstates[x])
          {
            inf->sendstates[x] = sendvf;
            addFieldVolume(field1,sendv);
          }

          if (ok != inf->sendok[x])
          {
            inf->sendok[x] = ok;
            lstrcatn(field1,"On",sizeof(field1));
            addFieldBool(field1,ok);
          }
        }

        if (m_tmp.GetLength() != origlen)
          SendCommand(m_tmp.Get());
      }
    }
  }

  void SetTrackListChange()
  {
    RescanTracks();
  }

  void SetSurfaceVolume(MediaTrack *trackid, double volume)
  {
    trackinf *inf;
    const float fvol = (float)volume;
    if (beginForTrack(trackid,&inf) && inf->vol != fvol)
    {
      inf->vol = fvol;
      addFieldVolume("volume",volume);
      SendCommand(m_tmp.Get());
    }
  }

  void SetSurfacePan(MediaTrack *trackid, double pan)
  {
    trackinf *inf;
    const float fpan = (float) pan;
    if (beginForTrack(trackid,&inf) && inf->pan != fpan)
    {
      inf->pan = fpan;
      addField("pan", 0.5+pan*0.5);
      SendCommand(m_tmp.Get());
    }
  }

  void SetSurfaceMute(MediaTrack *trackid, bool mute)
  {
    trackinf *inf;
    if (beginForTrack(trackid,&inf) && inf->mute != mute)
    {
      addFieldBool("mute", inf->mute = mute);
      SendCommand(m_tmp.Get());
    }
  }

  void SetSurfaceSelected(MediaTrack *trackid, bool selected)
  {
    // only send "selected" for first selected track
    if (!selected || trackid != GetSelectedTrack2(NULL,0)) return;
    if (selected && trackid == m_last_sel) return;

    trackinf *inf;
    if (beginForTrack(trackid, &inf))
    {
      if (selected) m_last_sel = trackid;
      else if (m_last_sel == trackid) m_last_sel = NULL;
      addFieldBool("selected",selected);
      SendCommand(m_tmp.Get());
    }
  }

  void SetSurfaceSolo(MediaTrack *trackid, bool solo)
  {
    if (GetMasterTrack(NULL) == trackid) return; // master solo handled separately
    trackinf *inf;
    if (beginForTrack(trackid,&inf))
    {
      addFieldBool("solo", inf->solo = solo);
      SendCommand(m_tmp.Get());
    }
  }

  void SetSurfaceRecArm(MediaTrack *trackid, bool recarm)
  {

  }

  void SetPlayState(bool play, bool pause, bool rec)
  {

  }

  virtual void SetRepeatState(bool rep)
  {

  }

  void SetTrackTitle(MediaTrack *trackid, const char *title)
  {
    if (GetMasterTrack(NULL) == trackid) return;
    trackinf *inf;
    const char *name = (const char*)GetSetMediaTrackInfo((INT_PTR)trackid, "P_NAME", NULL);
    if (name) title = name; // override default name processing ("3" instead of "")

    if (beginForTrack(trackid,&inf) && strcmp(inf->name.Get(),title))
    {
      inf->name.Set(title);
      addField("name", title);
      SendCommand(m_tmp.Get());
    }
  }

  bool GetTouchState(MediaTrack *trackid, int isPan)
  {
    GUID *guid=(GUID*)GetSetMediaTrackInfo((INT_PTR)trackid, "GUID", NULL);
    if (WDL_NORMALLY(guid))
    {
      trackinf *inf = m_tracks.Get(*guid);
      if (inf) return isPan==1 ? inf->pan_touch : isPan==0 ? inf->vol_touch : false;
    }
    return false;
  }

  void SetAutoMode(int mode)
  {
  }

  void ResetCachedVolPanStates()
  {
  }

  void OnTrackSelection(MediaTrack *trackid)
  {
    if (trackid != GetSelectedTrack2(NULL,0)) return;
    if (trackid == m_last_sel) return;
    trackinf *inf;
    if (beginForTrack(trackid,&inf))
    {
      m_last_sel = trackid;
      addFieldBool("selected",true);
      SendCommand(m_tmp.Get());
    }
  }

  bool IsKeyDown(int key)
  {
    return false; // VK_CONTROL, VK_MENU, VK_SHIFT, etc, whatever makes sense for your surface
  }

  int Extended(int call, void *parm1, void *parm2, void *parm3)
  {
    return 0; // return 0 if unsupported
  }
};


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
    break;

    case WM_USER+1024:
    break;
  }
  return 0;
}


static HWND configFunc(const char *typeString, HWND parent, const char *initConfigString)
{
  return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_CONSOLE1), parent,
      dlgProc, (LPARAM)initConfigString);
}

static IReaperControlSurface *createFunc(const char *typeString, const char *configString, int *errStats)
{
  if (s_inst) return NULL;
  return new CSurf_Console1(errStats);
}

reaper_csurf_reg_t csurf_console1_reg =
{
  "CONSOLE1",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "Console1 Mk III",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
