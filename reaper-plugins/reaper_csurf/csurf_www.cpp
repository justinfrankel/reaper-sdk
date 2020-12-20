#include <ctype.h>
#include "csurf.h"

#include "../../WDL/ptrlist.h"
#include "../../WDL/wdlcstring.h"
#include "../../WDL/lineparse.h"
#include "../../WDL/dirscan.h"
#include "../../WDL/assocarray.h"

#include "../../WDL/jnetlib/jnetlib.h"

#define JNETLIB_WEBSERVER_WANT_UTILS
#include "../../WDL/jnetlib/webserver.h"

int g_config_cache_expire = 3600;

static double flexi_atof(const char *p)
{
  if (!p || !*p) return 0.0;
  char buf[512];
  lstrcpyn(buf,p,sizeof(buf));
  char *n=buf;
  int state=0;
  while (*n)
  {
    char c = *n;
    if (!state)
    {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return 0.0; // invalid string (or -inf etc)
      if (c>='0' && c<='9') state++;
    }
    if (c == ',') *n='.';
    n++;
  }
  return state?atof(buf):0.0;
}

static void typedResToString(void *res, const char *p, char *fmt, int fmt_size)
{
  fmt[0]=0;
  if (res) switch (*p)
  {
    case 'P':
      if (!strcmp(p,"P_NAME")) lstrcpyn(fmt,p,fmt_size);
    break;
    case 'B': snprintf(fmt,fmt_size,"%d",*(bool *)res ? 1 : 0); break; 
    case 'C': snprintf(fmt,fmt_size,"%d",*(char *)res); break; 
    case 'D': snprintf(fmt,fmt_size,"%f",*(double *)res); break; 
    case 'F': snprintf(fmt,fmt_size,"%f",*(float *)res); break; 
    case 'I': 
      {
        int v = *(int *)res;
#ifdef _WIN32
        if (!strncmp(p,"I_CUSTOMCOLOR",13))
          // swap byte ordering to be 0xrrggbb
          v = (v&0xff00ff00) | ((v<<16)&0xff0000) | ((v>>16)&0xff);
#endif
        snprintf(fmt,fmt_size,"%d",v);
      }
    break; 
    case 'G':
      if (!strcmp(p,"GUID") || !strcmp(p,"G_GUID")) guidToString((GUID*)res,fmt);
    break;
  }
}
static void *stringToTypedRes(const char *p, char *src, char *workbuf)
{
  switch (*p)
  {
    case 'P':
      if (!strcmp(p,"P_NAME")) return src;
    break;
    case 'B': 
      *(bool *)workbuf = !!atoi(src);
    return workbuf;  
    case 'C': 
      *(char *)workbuf = (char)atoi(src);
    return workbuf;  
    case 'F': 
      *(float *)workbuf = (float)flexi_atof(src);
    return workbuf;  
    case 'D': 
      *(double *)workbuf = (double)flexi_atof(src);
    return workbuf;  
    case 'I': 
    {
      int v = atoi(src);
#ifdef _WIN32
      if (!strncmp(p,"I_CUSTOMCOLOR",13))
        // swap byte ordering from 0xrrggbb
        v = (v&0xff00ff00) | ((v<<16)&0xff0000) | ((v>>16)&0xff);
#endif
      *(int *)workbuf = v;
    }
    return workbuf;  
  }
  return NULL;
}

static const char *getUserWebRoot()
{
  static WDL_FastString s;
  if (!s.GetLength())
  {
    s.Set(GetResourcePath());
    s.Append(WDL_DIRCHAR_STR "reaper_www_root");
    CreateDirectory(s.Get(),NULL);
  }
  return s.Get();
}

static const char *getDefaultWebRoot()
{
  static WDL_FastString s;
  if (!s.GetLength()) 
  {
    char buf[1024];
    GetModuleFileName(g_hInst,buf,sizeof(buf));
    WDL_remove_filepart(buf);
    lstrcatn(buf,WDL_DIRCHAR_STR "reaper_www_root",sizeof(buf));
    s.Set(buf);
  }
  return s.Get();
}

static void simpleEscapeString(const char *p, WDL_FastString *o, char ign=0)
{
  for (;;)
  {
    char c = *p++;
    if (!c) break;
    if (c == ign) o->Append(&c,1);
    else if (c == '\\') o->Append("\\\\");
    else if (c == '\n') o->Append("\\n");
    else if (c == '\t') o->Append("\\t");
    else o->Append(&c,1);
  }
}


static void sendTrackLyrics(int tridx, WDL_FastString *o)
{
  char buf[16384]; 
  buf[0]=0;
  int len=sizeof(buf);
  o->AppendFormatted(64,"LYRICS\t%d\t",tridx);
  MediaTrack *tr = CSurf_TrackFromID(tridx,false);
  if (tr && GetTrackMIDILyrics(tr, 2, buf, &len) && len) simpleEscapeString(buf,o,'\t'); 
  o->Append("\n");
}

static void getProjectExtState(const char *sec, const char *nm, WDL_FastString *o)
{
  char tmp[16384];
  o->AppendFormatted(1024,"PROJEXTSTATE\t%s\t%s\t",sec,nm);
  tmp[0]=0;
  if (GetProjExtState(NULL,sec,nm,tmp,sizeof(tmp)))
  {
    simpleEscapeString(tmp,o);
  }
  o->Append("\n");
}

static void ProcessCommand(WDL_FastString *o, const char *in, int inlen)
{
  char want[1024];
  char tmp[1024];
  lstrcpyn(want,in,min(sizeof(want),inlen+1));
  int cmd = want[0] == '_' ? NamedCommandLookup(want) : atoi(want);
  if (cmd) 
  {
    if (cmd == 1013) CSurf_OnRecord();
    else SendMessage(GetMainHwnd(),WM_COMMAND,cmd,0);
  }
  else if (!strncmp(want,"SET/",4))
  {
    if (!strncmp(want+4,"UNDO_BEGIN",10))
    {
      Undo_BeginBlock();
    }
    else if (!strncmp(want+4,"UNDO/",5) || !strncmp(want+4,"UNDO_END/",8))
    {
      CSurf_FlushUndo(true);
      if (want[4+4] == '/')
      {
        WebServerBaseClass::url_decode(want+9,want,sizeof(want));
        Undo_OnStateChangeEx(want,UNDO_STATE_TRACKCFG|UNDO_STATE_MISCCFG,-1);
      }
      else
      {
        WebServerBaseClass::url_decode(want+13,want,sizeof(want));
        Undo_EndBlock(want,UNDO_STATE_TRACKCFG|UNDO_STATE_MISCCFG);
      }
    }
    else if (!strncmp(want+4,"REPEAT/",7))
    {
      int a = atoi(want+11);
      if (a<0) a=2;
      else if (a) a=1;
      GetSetRepeat(a);
    }
    else if (!strncmp(want+4,"TRACK/",6))
    {
      char *p = want+10;
      int tridx = atoi(p);
      MediaTrack *tr = CSurf_TrackFromID(tridx,false);

      if (tr)
      {
        while (*p && *p != '/') p++;
        while (*p == '/') p++;
        const char *nm = p;
        while (*p && *p != '/') p++;
        if (*p)
        {
          *p++=0;
          while (*p == '/') p++;

          if (!strcmp(nm,"VOL")) 
          {
            CSurf_SetSurfaceVolume(tr, CSurf_OnVolumeChangeEx(tr,flexi_atof(p),p[0]=='-'||p[0]=='+',p[strlen(p)-1]!='g'), NULL);
          }
          else if (!strcmp(nm,"PAN")) 
          {
            bool rel=false;
            if (p[0] == '+') { p++; rel=true; }
            CSurf_SetSurfacePan(tr, CSurf_OnPanChangeEx(tr,flexi_atof(p),rel,p[strlen(p)-1]!='g'), NULL);
          }
          else if (!strcmp(nm,"WIDTH")) 
          {
            bool rel=false;
            if (p[0] == '+') { p++; rel=true; }
            // does not exist yet:    CSurf_SetSurfaceWidth(tr, , NULL);
            CSurf_OnWidthChangeEx(tr,flexi_atof(p),rel,p[strlen(p)-1]!='g');
          }
          else if (!strcmp(nm,"MUTE")) CSurf_SetSurfaceMute(tr,CSurf_OnMuteChange(tr,atoi(p)),NULL);
          else if (!strcmp(nm,"SOLO")) CSurf_SetSurfaceSolo(tr,CSurf_OnSoloChange(tr,atoi(p)),NULL);
          else if (!strcmp(nm,"FX")) CSurf_OnFXChange(tr,atoi(p));
          else if (!strcmp(nm,"RECARM")) CSurf_SetSurfaceRecArm(tr,CSurf_OnRecArmChange(tr,atoi(p)),NULL);
          else if (!strcmp(nm,"RECMON")) 
          {
            int v = atoi(p);
            if (v<0)
            {
              int flag=0;
              GetTrackInfo(tridx-1,&flag);
              if (flag & 128) v=2;
              else if (flag&256) v=0;
              else v=1;
            }
            CSurf_OnInputMonitorChange(tr,v);
          }
          else if (!strcmp(nm,"SEL")) CSurf_SetSurfaceSelected(tr,CSurf_OnSelectedChange(tr,atoi(p)),NULL);
          else if (!strcmp(nm,"SEND"))
          {
            const int sendidx = atoi(p);

            while (*p && *p != '/') p++;
            while (*p == '/') p++;
            nm = p;
            while (*p && *p != '/') p++;
            if (*p)
            {
              *p++=0;
              while (*p == '/') p++;

              if (!strcmp(nm,"MUTE"))
              {
                int a = atoi(p);
                if (a>=0) 
                {
                  bool mute=false;
                  if (sendidx<0) GetTrackReceiveUIMute(tr,-1-sendidx,&mute);
                  else GetTrackSendUIMute(tr,sendidx,&mute);
                  if ((!!a) != mute) a = -1;
                }
                if (a<0) ToggleTrackSendUIMute(tr,sendidx);
              }
              else if (!strcmp(nm,"VOL") && *p)
              {
                const char *lp = p+strlen(p)-1;
                SetTrackSendUIVol(tr,sendidx,flexi_atof(p),*lp == 'e' ? 1 : *lp == 'E' ? -1 : 0);
              }
              else if (!strcmp(nm,"PAN") && *p)
              {
                const char *lp = p+strlen(p)-1;
                SetTrackSendUIPan(tr,sendidx,flexi_atof(p),*lp == 'e' ? 1 : *lp == 'E' ? -1 : 0);
              }
            }
          }
          else
          {
            if (p) WebServerBaseClass::url_decode(p,p,strlen(p) + 1); // decode in place

            void *setptr = stringToTypedRes(nm,p,tmp);
            if (setptr) GetSetMediaTrackInfo((INT_PTR)tr,nm,setptr);
          }
        }
      }
    }
    else if (!strncmp(want+4,"POS/",4))
    {
      SetEditCurPos(flexi_atof(want+8),true,true);
    }
    else if (!strncmp(want+4,"POS_STR/",8))
    {
      WebServerBaseClass::url_decode(want+12,want,sizeof(want));

      if (want[0] == 'm' || want[0] == 'M') GoToMarker(NULL,atoi(want+1),want[0] == 'M');
      else if (want[0] == 'r' || want[0] == 'R') GoToRegion(NULL,atoi(want+1),want[0] == 'R');
      else
        SetEditCurPos(parse_timestr_pos(want,-1),true,true);
    }
    else if (!strncmp(want+4,"EXTSTATE/",9) ||
             !strncmp(want+4,"EXTSTATEPERSIST/",16)||
             !strncmp(want+4,"PROJEXTSTATE/",13))
    {
      const int wh = want[4+8] == '/' ? (4+9) : want[4+8] == 'P' ? (4+16) : (4+13);
      char *p = want+wh;
      while (*p && *p != '/') p++;
      if (*p)
      {
        *p++=0;
        char *ident = p;
        while (*p && *p != '/') p++;
        if (*p) 
        {
          *p++=0;
          WebServerBaseClass::url_decode(want+wh,want,sizeof(want));
          WebServerBaseClass::url_decode(ident,ident,strlen(ident)+1);
          WebServerBaseClass::url_decode(p,p,strlen(p)+1);

          if (wh == (4+13)) SetProjExtState(NULL,want,ident,p);
          else SetExtState(want,ident,p,wh==(4+16));
        }
      }
    }
  }
  else if (!strncmp(want,"GET/",4))
  {
    if (!strncmp(want+4,"TRACK/",6))
    {
      const char *p = want+10;
      const int tridx=atoi(p);
      MediaTrack *tr = CSurf_TrackFromID(tridx,false);

      if (tr)
      {
        while (*p && *p != '/') p++;
        while (*p == '/') p++;

        if (!strncmp(p,"SEND/",5))
        {
          const int sendidx = atoi(p+5);
          const int num_hwouts = GetTrackNumSends(tr,1);

          int dest_idx=-1;
          bool mute=false;
          double vol=0.0,pan=0.0;
          if (sendidx<0)
          {
            MediaTrack *src = (MediaTrack*)GetSetTrackSendInfo(tr,-1,-1-sendidx,"P_SRCTRACK",NULL);
            if (src) dest_idx=CSurf_TrackToID(src,false);
            GetTrackReceiveUIMute(tr,-1-sendidx,&mute);
            GetTrackReceiveUIVolPan(tr,-1-sendidx,&vol,&pan);
          }
          else
          {
            if (sendidx>=num_hwouts)
            {
              MediaTrack *dest = (MediaTrack*)GetSetTrackSendInfo(tr,0,sendidx-num_hwouts,"P_DESTTRACK",NULL);
              if (dest) dest_idx=CSurf_TrackToID(dest,false);
            }
  
            GetTrackSendUIMute(tr,sendidx,&mute);
            GetTrackSendUIVolPan(tr,sendidx,&vol,&pan);
          }

          o->AppendFormatted(256,"SEND\t%d\t%d\t%d\t%f\t%f\t%d\n",tridx,sendidx,mute?8:0, vol, pan, dest_idx);
        }
        else
        {
          void *res= *p ? GetSetMediaTrackInfo((INT_PTR)tr,p,NULL) : NULL;
        
          o->Append(want);
          char fmt[1024];

          typedResToString(res,p,fmt,sizeof(fmt));

          o->Append("\t");
          o->Append(fmt);
          o->Append("\n");
        }
      }
    }
    else if (!strcmp(want+4,"REPEAT"))
    {
      o->AppendFormatted(64,"GET/REPEAT\t%d\n",!!GetSetRepeat(-1));
    }
    else if (!strncmp(want+4,"EXTSTATE/",9))
    {
      char *p = want+9+4;
      while (*p && *p != '/') p++;
      if (*p)
      {
        *p++=0;
        o->AppendFormatted(512,"EXTSTATE\t%s\t%s\t",want+9+4,p);
        const char *v = GetExtState(want+9+4,p);
        if (v && *v) simpleEscapeString(v,o);
        o->Append("\n");
      }
    }
    else if (!strncmp(want+4,"PROJEXTSTATE/",13))
    {
      char *p = want+13+4;
      while (*p && *p != '/') p++;
      if (*p)
      {
        *p++=0;
        getProjectExtState(want+13+4,p,o);
      }
    }
    else 
    {
      int tmp;
      if (want[4]=='_') tmp = NamedCommandLookup(want+4);
      else tmp = atoi(want+4);

      if (tmp > 0)
      {
        o->AppendFormatted(512,"CMDSTATE\t%s\t%d\n",want+4,GetToggleCommandState(tmp));
      }
    }
  }
  else if (!strcmp(want,"BEATPOS"))
  {
    int ps = GetPlayState();
    double ppos=(ps&1) ? GetPlayPosition() : GetCursorPosition();
    int mcnt=0,cml=0,cdenom=0;
    double fullbeats=0.0;
    double beats = TimeMap2_timeToBeats(NULL,ppos,&mcnt,&cml,&fullbeats,&cdenom);
    o->AppendFormatted(512,"BEATPOS\t%d\t%.15f\t%.15f\t%d\t%.15f\t%d\t%d\n", ps,ppos,fullbeats,mcnt,beats,cml,cdenom);
  }
  else if (!strcmp(want,"TRANSPORT"))
  {
    int ps = GetPlayState();
    double ppos=(ps&1) ? GetPlayPosition() : GetCursorPosition();
    char tsbuf[128], tsbuf2[128];
    int *tmodeptr = (int*)projectconfig_var_addr(NULL,__g_projectconfig_timemode2);
    format_timestr_pos(ppos,tsbuf,sizeof(tsbuf),tmodeptr ? *tmodeptr : -1);
    const char *bts=tsbuf;
    if (!tmodeptr || *tmodeptr != 2)
    {
      format_timestr_pos(ppos, tsbuf2, sizeof(tsbuf2), 2);
      bts=tsbuf2;
    }
    o->AppendFormatted(1024,"TRANSPORT\t%d\t%f\t%d\t%s\t%s\n",ps,ppos,GetSetRepeat(-1),tsbuf,bts);
  }
  else if (!strcmp(want,"NTRACK"))
  {
    o->AppendFormatted(64,"NTRACK\t%d\n",GetNumTracks());
  }
  else if (!strcmp(want,"TRACK") || !strncmp(want,"TRACK/",6))
  {
    int nt=GetNumTracks()+1;
    int minv=0;
    int maxv = nt;
    if (want[5]=='/')
    {
      char *p=want+6;
      minv = atoi(p);
      while (*p && *p != '-') p++;
      while (*p == '-') p++;
      maxv=(*p ? atoi(p) : minv)+1;
    }
    if (minv<0) minv=0;
    if (maxv>nt) maxv=nt;
    for (;minv<maxv;minv++)
    {
      int flag=0;
      MediaTrack *tr = CSurf_TrackFromID(minv,false);
      const char *nm=GetTrackInfo(minv-1,&flag);
      if (nm && tr)
      {
        char tname[512];
        lstrcpyn(tname,minv ? nm : "MASTER",sizeof(tname));
        char *p=tname;
        while (*p)
        {
          if (*p == '\t' || *p == '\n') *p=' ';
          p++;
        }
        double vol=1.0,pan=0.0,wid=0.0;
        int panmode=0;
        GetTrackUIVolPan(tr,&vol,NULL);
        GetTrackUIPan(tr,&pan,&wid,&panmode);
        
        int *peakhold = (int*)GetSetMediaTrackInfo((INT_PTR)tr,"I_PEAKINFO_H",NULL);
        int *peakpos = (int*)GetSetMediaTrackInfo((INT_PTR)tr,"I_PEAKINFO",NULL);

        int color=0;
        const int *pcol = (const int*)GetSetMediaTrackInfo((INT_PTR)tr,"I_CUSTOMCOLOR",NULL);
        if (pcol)
        {
          color = *pcol;
          if (!(color&0x1000000)) color=0;
#ifdef _WIN32
          // swap byte ordering from 0xrrggbb
          else color = (color&0xff00ff00) | ((color<<16)&0xff0000) | ((color>>16)&0xff);
#endif
        }

        o->AppendFormatted(256,"TRACK\t%d\t%s\t%d\t%f\t%f\t%d\t%d\t%f\t%d\t%d\t%d\t%d\t%d\n",
            minv,tname,flag,vol,pan,peakhold?*peakhold:-120000,peakpos?*peakpos:-120000,wid,panmode,
            GetTrackNumSends(tr,0),
            GetTrackNumSends(tr,-1),
            GetTrackNumSends(tr,1),
            color
            );
      }
    }
  }
  else if (!strncmp(want, "LYRICS/", 7))
  {
    sendTrackLyrics(atoi(want+7),o);
  }
  else if (!strncmp(want,"OSC/",4))
  {
    WebServerBaseClass::url_decode(want+3,want,sizeof(want)); // include leading /
    char *p = strstr(want,":");
    if (p) *p++=0;
    double v=p?flexi_atof(p) : 0.0;
    float v2=(float)v;
    OscLocalMessageToHost(want,p?&v:NULL); // csurf processing
    CSurf_OnOscControlMessage(want,p?&v2:NULL); // learn bindings
  }
  else if (!strcmp(want,"MARKER") || !strcmp(want,"REGION"))
  {
    int i=0;
    bool rgn;
    double pos, rgnend;
    const char *name;
    int idx, col;
    o->AppendFormatted(64,"%s_LIST\n",want);
    while (0 != (i=EnumProjectMarkers3(NULL,i,&rgn,&pos,&rgnend,&name,&idx,&col)))
    {
      if (rgn != (want[0] == 'R')) continue;
      o->Append(want);
      o->Append("\t");
      simpleEscapeString(name,o);
      o->AppendFormatted(128,"\t%d\t%.15f",idx, pos);
      if (rgn) o->AppendFormatted(512,"\t%.15f",rgnend);

      if (!(col&0x1000000)) col=0;
#ifdef _WIN32
      // swap byte ordering to 0xrrggbb
      else col = (col & 0xff00ff00) | ((col<<16)&0xff0000) | ((col>>16)&0xff);
#endif
      o->AppendFormatted(512,"\t%d",col);

      o->Append("\n");
    }
    o->AppendFormatted(64,"%s_LIST_END\n",want);


  }
}




class wwwServer : public WebServerBaseClass
{
public:
  wwwServer() { tmpgen=NULL; userpass[0]=0; strcpy(def_file,"/index.html"); } 
  ~wwwServer() { delete tmpgen; }
  JNL_StringPageGenerator *tmpgen;

  virtual IPageGenerator *onConnection(JNL_HTTPServ *serv, int port)
  {
    char buf[2048];
    serv->set_reply_header("Server:reaper_csurf_www/0.1");

    if (userpass[0])
    {
      const char *auth = serv->getheader("Authorization");
      if (auth)
      {
        if (!strnicmp(auth,"Basic",5))
        {
          auth += 5;
          while (*auth == ' ' || *auth == '\t') auth++;
          buf[0]=0;
          base64decode(auth,buf,sizeof(buf));
          if (strcmp(buf,userpass)) auth=NULL;
        }
        else auth=NULL;
      } 
      if (!auth)
      {
        serv->set_reply_header("WWW-Authenticate: Basic realm=\"reaper_www\"");
        serv->set_reply_string("HTTP/1.1 401 Unauthorized");
        serv->set_reply_size(0);
        serv->send_reply();
        return 0;
      }
    }   
    const char *req = serv->get_request_file();
    if (!strncmp(req,"/_/",3))
    {
      req+=3;
      if (!tmpgen) tmpgen = new JNL_StringPageGenerator;
      while (*req == ';') req++;
      while (*req)
      {
        const char *np = req;
        while (*np && *np != ';') np++;
      
        ProcessCommand(&tmpgen->str,req,np-req);

        req = np;
        while (*req == ';') req++;
      }

      JNL_StringPageGenerator *res=NULL;
      if (tmpgen->str.Get()[0])
      {
        res=tmpgen;
        tmpgen=0;
      }

      serv->set_reply_header("Content-Type: text/plain");
      serv->set_reply_header("Cache-Control: no-cache, must-revalidate"); // HTTP/1.1
      serv->set_reply_header("Expires: Sat, 26 Jul 1997 05:00:00 GMT"); // Date in the past
      serv->set_reply_string("HTTP/1.1 200 OK");
      serv->set_reply_size(res ? strlen(res->str.Get()) : 0);
      serv->send_reply();
      return res;
    }
    else if (req[0] == '/' && !strstr(req,".."))
    {
      if (!req[1]) req=def_file;
      WDL_FileRead *fr=NULL;
      const char *userroot = getUserWebRoot();

      const char *extra = WDL_get_fileext(req)[0] ? "\0" : "\0.html\0";
      for (;;)
      {
        if (userroot && *userroot)
        {
          snprintf(buf,sizeof(buf),"%s%s%s",userroot,req,extra);
          fr = new WDL_FileRead(buf,0,32768,2);
          if (!fr->IsOpen()) { delete fr; fr=NULL; }
        }

        if (!fr) // fall back to program root path
        {
          snprintf(buf,sizeof(buf),"%s%s%s",getDefaultWebRoot(),req,extra);
          fr = new WDL_FileRead(buf,0,32768,2);
          if (!fr->IsOpen()) { delete fr; fr=NULL; }
        }
#ifndef _WIN32
        struct stat sb;
        if (fr && !stat(buf,&sb) && !S_ISREG(sb.st_mode))
        {
          // WDL_FileRead() can open directories on posix, oops
          delete fr;
          fr = NULL;
        }
#endif
        if (fr) break;
        while (*extra) extra++;
        extra++;
        if (!*extra) break;
      }

      if (fr)
      {
        char hdr[512];
        strcpy(hdr,"Content-Type: ");
        JNL_get_mime_type_for_file(buf,hdr+strlen(hdr),256);
        serv->set_reply_header(hdr);
        
        const char *ext = WDL_get_fileext(buf);
        // do not cache html/js/css files, but cache everything else
        if (g_config_cache_expire && stricmp(ext,".html") && stricmp(ext,".js") && stricmp(ext,".css"))
        {
          strcpy(hdr,"Expires: ");
          char *p = hdr+strlen(hdr);
          JNL_Format_RFC1123(time(NULL)+g_config_cache_expire,p);
          if (p[0]) serv->set_reply_header(hdr);
        }
        else
        {
          serv->set_reply_header("Cache-Control: no-cache, must-revalidate"); // HTTP/1.1
          serv->set_reply_header("Expires: Sat, 26 Jul 1997 05:00:00 GMT"); // Date in the past
        }

        serv->set_reply_string("HTTP/1.1 200 OK");
        serv->set_reply_size((int)fr->GetSize());
        serv->send_reply();
        return new JNL_FilePageGenerator(fr);
      }
    }

    serv->set_reply_string("HTTP/1.1 404 NOT FOUND");
    serv->set_reply_size(0);
    serv->send_reply();
    return 0; // no data
  }

  char userpass[256],def_file[128];
};

void GetLocalIP(char* buf, int buflen);

class CSurf_WWW : public IReaperControlSurface
{
public:

  CSurf_WWW(const char *cfg, int *err) 
  {
    m_istmp=false;
    s_list.Add(this);
    m_rc_tok[0]=0;
    m_rc_response[0]=0;
    m_rc_chki=0;
    m_rc_nexttime=0;
    m_rc_con=NULL;
    m_rc_lastlocalip[0]=0;
    m_rc_lastlocalip_validcnt = 0; 

    if (cfg) m_cfg.Set(cfg);

    update_cfg(err);
  }
  void update_cfg(int *err)
  {
    LineParser lp;
    lp.parse(m_cfg.Get());
    m_enabled = !lp.gettoken_int(0);
    lstrcpyn_safe(serv.userpass,lp.gettoken_str(2),sizeof(serv.userpass));

    if (lp.gettoken_str(3)[0])
      snprintf(serv.def_file,sizeof(serv.def_file),"/%s",lp.gettoken_str(3));

    serv.removeListenIdx(0);

    if (m_enabled)
    {
#ifdef _WIN32
      static bool socketlib_init;
      if (!socketlib_init)
      {
        socketlib_init=true;
        JNL::open_socketlib();
      }
#endif

      int port = lp.gettoken_int(1);
      if (!port) port=8080;
      if (serv.addListenPort(port)<0 && err) err[0]|=4;
    }

    if (m_enabled && lp.gettoken_int(4))
    {
        // could reduce an extra check on apply by doing
        //if (strncmp(m_rc_tok,lp.gettoken_str(5),sizeof(m_rc_tok)-1)) || port_changed || ip_changed
      {
        lstrcpyn_safe(m_rc_tok,lp.gettoken_str(5),sizeof(m_rc_tok));
        m_rc_nexttime = 0;
        m_rc_lastlocalip_validcnt = 0;
        m_rc_chki=0;
        m_rc_response[0]=0;
        delete m_rc_con;
        m_rc_con=NULL;
      }
    }
    else
    {
      m_rc_tok[0]=0;
    }

  }

  ~CSurf_WWW()
  {
    delete m_rc_con;
    m_rc_con=NULL;
    s_list.DeletePtr(this);
  }
  virtual const char *GetTypeString() { return "HTTP"; }
  virtual const char *GetDescString() 
  { 
    char buf[128];
    GetLocalIP(buf, sizeof(buf));
    if (!buf[0]) strcpy(buf, "localhost");

    static WDL_FastString fs;
    fs.SetFormatted(512,__LOCALIZE_VERFMT("Web browser interface: %s%shttp://%s:%d","csurf_www"),
      m_rc_response,m_rc_response[0]? " -- ":"",
      buf,serv.getListenPort(0));
    return fs.Get();
  }
  virtual const char *GetConfigString() { return m_cfg.Get(); }

  virtual void Run()
  {
    static bool reent;
    if (!reent && m_enabled)
    {
      reent=true;
      serv.run();
      reent=false;
    }
    if (m_rc_con)
    {
      m_rc_con->run();
      char buf[512];
      for (;;)
      {
        int l = m_rc_con->recv_get_linelen();
        if (l<1) break;
        if (l > sizeof(buf)-1) l = sizeof(buf)-1;
        m_rc_con->recv_bytes(buf,l);
        buf[l]=0;
        while (l > 0 && (buf[l-1] == '\r' || buf[l-1] == '\n')) buf[--l]=0;
        switch (m_rc_chki)
        {
          case 0:
            // HTTP reply
            {
              int code=0;
              if (!strncmp(buf,"HTTP/",5))
              {
                const char *p=strstr(buf," ");
                if (p) code=atoi(p);
              }

              if (code < 200 || code >= 400)
              {
                m_rc_chki = 100;
                lstrcpyn(m_rc_response,buf,sizeof(buf));
              }
              else 
                m_rc_chki++;
            }
          break;
          case 1:
            // HTTP headers
            if (!buf[0]) m_rc_chki++;
          break;
          case 2:
            if (!strncmp(buf,"OK\t",3))
              lstrcpyn_safe(m_rc_response,buf+3,sizeof(m_rc_response));
            else
              lstrcpyn_safe(m_rc_response,buf,sizeof(m_rc_response));

            m_rc_chki++;
          break;
        }
      }

      if (m_rc_chki > 2 || 
          m_rc_con->get_state() == JNL_Connection::STATE_ERROR ||
          m_rc_con->get_state() == JNL_Connection::STATE_CLOSED ||
          time(NULL) > m_rc_nexttime)
      {
        m_rc_nexttime = time(NULL) + 60; // check for IP changes in 60s
        m_rc_lastlocalip_validcnt = 0;

        if (m_rc_chki == 3 && !strnicmp(m_rc_response,"http://",7)) 
        {
          m_rc_lastlocalip_validcnt = 30;  // only re-send every 30 minutes unless IP changed
        }

//        printf("done, next %d\n",(int)(m_rc_nexttime-time(NULL)));

        delete m_rc_con;
        m_rc_con=NULL;
        m_rc_chki = 100;
      }
    }
    else if (m_enabled && m_rc_tok[0] && --m_rc_chki<0)
    {
      m_rc_chki=100;
      if (!m_rc_nexttime || m_rc_nexttime < time(NULL))
      {
        int port,lerr=0;
        if ((port=serv.getListenPort(0,&lerr))>0 && !lerr)
        {
          char buf[1024], tmp[32];
          tmp[0]=0;
          GetLocalIP(tmp,sizeof(tmp));
          if (m_rc_lastlocalip_validcnt>0) m_rc_lastlocalip_validcnt--;

          if (m_rc_lastlocalip_validcnt>0 && (!tmp[0] || !strcmp(tmp,m_rc_lastlocalip))) 
          {
            // IP invalid or didn't change, skip
            m_rc_nexttime = time(NULL)+60;
          }
          else if (tmp[0])
          {
            lstrcpyn_safe(m_rc_lastlocalip,tmp,sizeof(m_rc_lastlocalip));

            static JNL_AsyncDNS *s_dns;
            if (!s_dns) s_dns = new JNL_AsyncDNS;
            m_rc_con = new JNL_Connection(s_dns);
            m_rc_con->connect("rc.reaper.fm",80);
            snprintf(buf,sizeof(buf),"GET /_/%s/%s/%d HTTP/1.1\r\n"
                            "Host: rc.reaper.fm\r\n"
                            "User-Agent: reaper_csurf_www/0.1\r\n"
                            "Connection: close\r\n"
                            "\r\n",m_rc_tok,tmp,port);
                
            m_rc_con->send(buf,strlen(buf));
            m_rc_nexttime = time(NULL)+8;
            lstrcpyn_safe(m_rc_response,__LOCALIZE("requesting...","csurf_www"),sizeof(m_rc_response));
            //printf("requesting...\n");
            m_rc_chki = 0;
          }
          else
          {
            m_rc_nexttime = time(NULL)+60;
            lstrcpyn_safe(m_rc_response,__LOCALIZE("error getting local IP","csurf_www"),sizeof(m_rc_response));
          }
        }
        else
        {
          m_rc_nexttime = time(NULL)+60;
          lstrcpyn_safe(m_rc_response,__LOCALIZE("error getting local port","csurf_www"),sizeof(m_rc_response));
        }
      }
    }
  }
  wwwServer serv;
  bool m_enabled;
  WDL_FastString m_cfg;
  bool m_istmp; // set if created by the dialog

  JNL_Connection *m_rc_con;
  int m_rc_chki; // check interval if !m_rc_con, otherwise state for m_rc_con
  int m_rc_lastlocalip_validcnt; 

  char m_rc_response[128];
  char m_rc_tok[64];
  char m_rc_lastlocalip[32];
  time_t m_rc_nexttime;

  static WDL_PtrList<CSurf_WWW> s_list;
};

WDL_PtrList<CSurf_WWW> CSurf_WWW::s_list;


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static const char *need_update_msg;
  if (!need_update_msg) need_update_msg = __LOCALIZE_NOCACHE("Click 'Apply settings' for URL","csurf_www");
  char buf[1024];
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO1));

      LineParser lp;
      if (lParam) 
      {
        const char *p = (const char *)lParam;
        if (*p) 
        {
          int x;
          for (x=0;x<CSurf_WWW::s_list.GetSize();x++)
          {
            CSurf_WWW *cs = CSurf_WWW::s_list.Get(x);
            if (cs && !cs->m_istmp && !strcmp(cs->m_cfg.Get(),p))
            {
              SetWindowLongPtr(hwndDlg,GWLP_USERDATA,(LPARAM)cs);
            }
          }
          lp.parse(p);
        }
        HWND apply = GetDlgItem(GetParent(hwndDlg),1144); // IDC_APPLY
        if (apply) ShowWindow(apply,SW_SHOWNA);
      }

      if (!lp.gettoken_int(0)) CheckDlgButton(hwndDlg,IDC_CHECK1,BST_CHECKED);
      SetDlgItemInt(hwndDlg,IDC_EDIT1,lp.gettoken_int(1) ? lp.gettoken_int(1) : 8080, FALSE);
      SetDlgItemText(hwndDlg,IDC_EDIT2,lp.gettoken_str(2));
      if (lp.gettoken_int(4)) CheckDlgButton(hwndDlg,IDC_CHECK2,BST_CHECKED);
      SetDlgItemText(hwndDlg,IDC_EDIT3,lp.gettoken_str(5));

      SendMessage(hwndDlg,WM_USER+100,0,(LPARAM)lp.gettoken_str(3));

      SendMessage(hwndDlg,WM_USER+101,0,0);
      SetTimer(hwndDlg,1,30,NULL);
    }
    return 0;
    case WM_DESTROY:
      {
        KillTimer(hwndDlg,1);
        CSurf_WWW *cs = (CSurf_WWW*)GetWindowLongPtr(hwndDlg,GWLP_USERDATA);
        if (cs)
        {
          SetWindowLongPtr(hwndDlg,GWLP_USERDATA,0);
          if (CSurf_WWW::s_list.Find(cs)>=0 && cs->m_istmp) delete cs;
        }
      }
    return 0;
    case WM_TIMER:
      if (wParam == 1)
      {
        CSurf_WWW *cs = (CSurf_WWW*)GetWindowLongPtr(hwndDlg,GWLP_USERDATA);
        if (cs && CSurf_WWW::s_list.Find(cs)>=0)
        {
          if (cs->m_istmp) cs->Run();
          if (cs->m_rc_tok[0] && cs->m_rc_response[0]) 
          {
            GetDlgItemText(hwndDlg,IDC_EDIT6,buf,sizeof(buf));
            if (strcmp(buf,need_update_msg) &&
                strncmp(buf,cs->m_rc_response,sizeof(cs->m_rc_response)-1))
              SetDlgItemText(hwndDlg,IDC_EDIT6,cs->m_rc_response);
          }
        }
      }
    return 0;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDC_CHECK1 || 
          LOWORD(wParam) == IDC_CHECK2 ||
          ((LOWORD(wParam) == IDC_EDIT1||LOWORD(wParam)==IDC_EDIT3) && HIWORD(wParam) == EN_CHANGE))
      {
        SetDlgItemText(hwndDlg,IDC_EDIT6,need_update_msg);
      }
      else if (LOWORD(wParam) == IDC_BUTTON1 || LOWORD(wParam) == IDC_BUTTON2)
      {
        const char *dir=LOWORD(wParam) == IDC_BUTTON1 ? getUserWebRoot() : getDefaultWebRoot();
        ShellExecute(hwndDlg, "open", dir, "", dir, SW_SHOW);
      }
      else if (LOWORD(wParam) == 1144/*IDC_APPLY parent*/)
      {
        buf[0]=0;
        SendMessage(hwndDlg,WM_USER+1024,sizeof(buf),(LPARAM)buf);

        int err=0;
        CSurf_WWW *cs = (CSurf_WWW*)GetWindowLongPtr(hwndDlg,GWLP_USERDATA);
        if (!cs)
        {
          cs = new CSurf_WWW(buf,&err);
          cs->m_istmp = true;
          SetWindowLongPtr(hwndDlg,GWLP_USERDATA,(LPARAM)cs);
        }
        else if (cs && CSurf_WWW::s_list.Find(cs)>=0)
        {
          if (buf[0])
          {
            cs->m_cfg.Set(buf);
            cs->update_cfg(&err);
          }
        }
        SendMessage(hwndDlg,WM_USER+101,0,0);
        if (err)
        {
          MessageBox(hwndDlg,__LOCALIZE("Error listening on port, maybe the port is in use?","csurf_www"),
               __LOCALIZE("REAPER Web Interface Error","csurf_www"),MB_OK);
        }
      }
    return 0;
    case WM_USER+101:
      {
        if (IsDlgButtonChecked(hwndDlg,IDC_CHECK1))
        {
          CSurf_WWW *cs = (CSurf_WWW*)GetWindowLongPtr(hwndDlg,GWLP_USERDATA);
          if (cs && CSurf_WWW::s_list.Find(cs)>=0 && cs->m_rc_tok[0])
          {
            lstrcpyn_safe(buf,cs->m_rc_response[0] ? cs->m_rc_response : __LOCALIZE("updating...","csurf_www"),sizeof(buf));
          }
          else
          {
            strcpy(buf,"http://");
            GetLocalIP(buf+7, sizeof(buf)-7);
            if (!buf[7]) strcpy(buf+7, "localhost");
            BOOL t;
            int port = GetDlgItemInt(hwndDlg,IDC_EDIT1,&t,FALSE);
            snprintf_append(buf,sizeof(buf),":%d",port?port:8080);
          }
        }
        else
        {
          lstrcpyn_safe(buf,__LOCALIZE("disabled","csurf_www"),sizeof(buf));
        }

        SetDlgItemText(hwndDlg,IDC_EDIT6,buf);
      }
    return 0;

    case WM_USER+100: // refresh pattern config file list
    {
      WDL_StringKeyedArray<bool> list(false);
      WDL_DirScan ds;
      if (!ds.First(getUserWebRoot())) do
      {
        const char* fn=ds.GetCurrentFN();
        if (!stricmp(WDL_get_fileext(fn), ".html")) list.Insert(fn,true);
      }
      while (!ds.Next());

      if (!ds.First(getDefaultWebRoot())) do
      {
        const char* fn=ds.GetCurrentFN();
        if (!stricmp(WDL_get_fileext(fn), ".html")) list.Insert(fn,true);
      }
      while (!ds.Next());

      if (lParam) lstrcpyn_safe(buf,(const char *)lParam,sizeof(buf));
      else GetDlgItemText(hwndDlg,IDC_COMBO1,buf,sizeof(buf));

      if (!buf[0]) lstrcpyn_safe(buf,"index.html",sizeof(buf));

      int didsel=0;

      SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
      for (int x = 0; x < list.GetSize(); x++)
      {
        const char *fn;
        list.Enumerate(x,&fn);
        int a = (int)SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)fn);
        if (!stricmp(fn,buf))
        {
          didsel=1;
          SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_SETCURSEL, a,0);
        }
      }

      if (!didsel)
      {
        int a = (int)SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)buf);
        SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_SETCURSEL, a,0);
      }
    }
    return 0;

    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        BOOL t;
        int port=GetDlgItemInt(hwndDlg,IDC_EDIT1,&t,FALSE);
        if (port < 1) port = 8080;

        GetDlgItemText(hwndDlg,IDC_EDIT2,buf,sizeof(buf));
        snprintf((char*)lParam, (int)wParam, "%d %d '%s'", 
          IsDlgButtonChecked(hwndDlg,IDC_CHECK1)?0:1,port,buf);

        GetDlgItemText(hwndDlg,IDC_COMBO1,buf,sizeof(buf));
        snprintf_append((char*)lParam, (int)wParam, " '%s' %d",buf,IsDlgButtonChecked(hwndDlg,IDC_CHECK2)?1:0);

        GetDlgItemText(hwndDlg,IDC_EDIT3,buf,64);
        const char *rd=buf;
        char *wr=buf;
        while (*rd)
        {
          char c = *rd++;
          if ((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c=='.' || c == '_') *wr++ = c;
        }
        *wr=0;
        snprintf_append((char*)lParam, (int)wParam, " '%s'",buf);
      }
    return 0;
  }

  return 0;
}


static HWND configFunc(const char* typestr, HWND parent, const char* cfgstr)
{
  return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_WWW), parent, dlgProc,(LPARAM)cfgstr);
}


static IReaperControlSurface *createFunc(const char* typestr, const char* cfgstr, int* err)
{
  if (!strcmp(typestr,"HTTP")) return new CSurf_WWW(cfgstr,err);
  return NULL;
}


reaper_csurf_reg_t csurf_www_reg = 
{
  "HTTP",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "Web browser interface",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
