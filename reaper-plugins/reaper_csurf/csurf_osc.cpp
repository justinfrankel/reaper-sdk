#include <ctype.h>
#include "csurf.h"
#include "osc.h"
extern void (*update_disk_counters)(int read, int write);
#include "../../WDL/ptrlist.h"
#include "../../WDL/assocarray.h"
#include "../../WDL/projectcontext.cpp"
#include "../../WDL/dirscan.h"

#ifdef _DEBUG
#include <assert.h>
#endif


#define OSC_EXT ".ReaperOSC"

#define MAX_OSC_WC 16
#define MAX_OSC_RPTCNT 16

#define CSURF_EXT_IMPL_ADD 0x00010000
#define CSURF_EXT_SETPAN_EX_IMPL (CSURF_EXT_SETPAN_EX+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETINPUTMONITOR_IMPL (CSURF_EXT_SETINPUTMONITOR+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETSENDVOLUME_IMPL (CSURF_EXT_SETSENDVOLUME+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETSENDPAN_IMPL (CSURF_EXT_SETSENDPAN+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETRECVVOLUME_IMPL (CSURF_EXT_SETRECVVOLUME+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETRECVPAN_IMPL (CSURF_EXT_SETRECVPAN+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETFXENABLED_IMPL (CSURF_EXT_SETFXENABLED+CSURF_EXT_IMPL_ADD)
#define CSURF_EXT_SETFXOPEN_IMPL (CSURF_EXT_SETFXOPEN+CSURF_EXT_IMPL_ADD)


#define MAX_LASTTOUCHED_TRACK 2048
#define ROTARY_STEP (1.0/1024.0)

#define FX_IDX_MODE_REC 0x1000000
#define FX_IDX_MODE(x) ((x) & 0xFF000000)
#define FX_IDX_REMOVE_MODE(x) (((x)&0x800000)?((x)|0xff000000):((x)&0xffffff))
#define FX_IDX_SET_MODE(x,v) (((x)&0xFFFFFF)|(v))

enum 
{
  PAN_MODE_CLASSIC=0,
  PAN_MODE_NEW_BALANCE=3,
  PAN_MODE_STEREO_PAN=5,
  PAN_MODE_DUAL_PAN=6
};


template <class T> void _swap(T& a, T& b) 
{
  T t=a;
  a=b;
  b=t; 
}

template <class T> void _reverse(T* vec, int len)
{
  int i;
  for (i=0; i < len/2; ++i)
  {
    _swap(vec[i], vec[len-i-1]);
  }
}

bool strstarts(const char* p, const char* q)
{
  return !strncmp(p, q, strlen(q));
}

bool strends(const char* p, const char* q)
{
  const char* s=strstr(p, q);
  return (s && strlen(s) == strlen(q));
}

bool isint(const char* p)
{
  if (!*p) return false;
  while (*p)
  {
    if (!isdigit_safe(*p)) return false;
    ++p;
  }
  return true;
}

bool isfloat(const char* p)
{
  if (!*p) return false;
  if (*p == '-') ++p;
  bool hasdec=false;
  while (*p)
  {
    if (*p == '.')
    {   
      if (hasdec) return false;
      hasdec=true;
    }
    else if (!isdigit_safe(*p))
    {
      return false;
    }
    ++p;
  }
  return true;
}


ProjectStateContext *getDefaultOSCContext()
{
  char tmp[1024];
  GetModuleFileName(g_hInst,tmp,sizeof(tmp));
  WDL_remove_filepart(tmp);
  lstrcatn(tmp,PREF_DIRSTR "Default.ReaperOSC",sizeof(tmp));
  ProjectStateContext *ctx = ProjectCreateFileRead(tmp);
  if (ctx) return ctx;

  // fallback to user-directory Default.ReaperOSC
  lstrcpyn_safe(tmp,GetOscCfgDir(),sizeof(tmp));
  lstrcatn(tmp,"Default.ReaperOSC",sizeof(tmp));
  ctx = ProjectCreateFileRead(tmp);

  return ctx;
}

bool LoadCfgContext(ProjectStateContext *ctx, WDL_PtrList<char>* cfg, char* errbuf, int errbuf_sz);

bool LoadCfgFile(const char* fn, WDL_PtrList<char>* cfg, char* errbuf, int errbuf_sz)
{
  if (errbuf) errbuf[0]=0;
  if (!fn || !fn[0]) return false;

  WDL_FastString fnbuf;
  fnbuf.Set(GetOscCfgDir());
  fnbuf.Append(WDL_get_filepart(fn));
  if (stricmp(fnbuf.get_fileext(), OSC_EXT))
  {
    fnbuf.Append(OSC_EXT);
  }
  ProjectStateContext* ctx=ProjectCreateFileRead(fnbuf.Get());

  bool rv = ctx && LoadCfgContext(ctx,cfg,errbuf,errbuf_sz);

  delete ctx;

  return rv;
}

bool LoadCfgContext(ProjectStateContext *ctx, WDL_PtrList<char>* cfg, char* errbuf, int errbuf_sz)
{
  char buf[4096];

  static WDL_StringKeyedArray<char> keys;
  if (!keys.GetSize())
  {
    // load up the valid desc key strings
    ProjectStateContext *rc = getDefaultOSCContext();
    if (rc)
    {
      while (!rc->GetLine(buf,sizeof(buf)))
      {
        LineParserInt lp;
        if (lp.parseDestroyBuffer(buf) || lp.getnumtokens() < 1) continue;    
      
        const char* key=lp.gettoken_str(0);
        if (key[0] == '#') continue;      
        
        keys.AddUnsorted(key, 1);    
      }
      delete rc;
    }
    keys.Resort();
  }

  bool haderr=false;
  int line=0;
  while (!ctx->GetLine(buf, sizeof(buf)))
  {
    ++line;
    LineParser lp;
    if (lp.parse(buf) || lp.getnumtokens() < 2) continue;

    const char* key=lp.gettoken_str(0);
    if (key[0] == '#') continue;

    if (!keys.Exists(key))
    {
      if (!errbuf) continue; // errbuf is non-NULL only when loading from UI, silently ignore unknown actions otherwise

      haderr = true;
      snprintf(errbuf, errbuf_sz, __LOCALIZE_VERFMT("Unknown action \"%s\" on line %d","csurf_osc"), key, line);
      break;
    }

    int i;
    for (i=1; i < lp.getnumtokens(); ++i)
    {
      const char* pattern=lp.gettoken_str(i);

      if (i == 1)
      {
        if (strstarts(key, "DEVICE_") || strstarts(key, "REAPER_"))
        {
          if (strends(key, "_COUNT") && isint(pattern)) 
          {
            continue;
          }
          if (strends(key, "_ROTARY_CENTER") && isfloat(pattern))
          {
            continue;
          }
          if (strends(key, "_FOLLOWS") && 
            (!strcmp(pattern, "REAPER") ||
             !strcmp(pattern, "DEVICE") ||
             !strcmp(pattern, "LAST_TOUCHED") ||
             !strcmp(pattern, "FOCUSED") ||
             !strcmp(pattern, "MIXER")))
          {
            continue;
          }
          if (strends(key, "_EQ") && !strcmp(pattern, "INSERT"))
          {
            continue;
          }
        }
      }
 
      if (!strchr("nfbtrsi", pattern[0]))
      {
        if (errbuf) 
          snprintf(errbuf, errbuf_sz, 
              __LOCALIZE_VERFMT("Pattern \"%s\" starts with unknown flag '%c' on line %d (allowed: [nfbtrsi])","csurf_osc"), 
            pattern, pattern[0], line);
        break;
      }
      if (pattern[1] != '/')
      {
        if (errbuf)
          snprintf(errbuf, errbuf_sz, __LOCALIZE_VERFMT("Pattern \"%s\" does not start with '/' on line %d","csurf_osc"), pattern, line);
        break;
      }
      if (strlen(pattern) > 512)
      {
        if (errbuf)
          snprintf(errbuf, errbuf_sz, __LOCALIZE_VERFMT("Pattern is too long on line %d","csurf_osc"), line);
        break;
      }
    }
    if (i < lp.getnumtokens()) 
    {
      haderr = true;
      break;
    }

    if (cfg) cfg->Add(strdup(buf));
  }

  if (haderr && cfg) cfg->Empty(true, free);

  return !haderr;
}

static int parse_number_or_command_id(const char *b)
{
  // this will be wasteful if the user sends a string beginning with _ to match an integer field for 
  // a command other than ACTION. why they would do that, I don't know!
  // (and if it happens to match an action ID, it might change the behavior, but that seems very unlikely)
  if (*b != '_')
    return atoi(b);
  char buf[1024];
  const char *p = b+1;
  while (*p == '_' || isalnum_safe(*p)) p++;
  if (p == b+1) return 0;
  lstrcpyn_safe(buf,b,wdl_min(sizeof(buf), p+1 - b));
  return NamedCommandLookup(buf);
}
static const char *skip_number_or_command_id(const char *b)
{
  if (*b == '_')
  {
    b++;
    while (*b == '_' || isalnum_safe(*b)) b++;
  }
  else
  {
    while (isdigit_safe(*b)) ++b;
  }
  return b;
}



// '*' matches anything
// '?' matches any character
// '@' matches any integer and matches are returned in wcmatches
// a pattern like /track/1/fx/2,3/fxparam/5,7 has 5 wildcards in 3 slots
// in processing, this will be expanded to track/1/fx/2/fxparam/5 and track/1/fx/3/fxparam/7
// numwc=5, numslots=3, slotcnt=[1,2,2], rptcnt=2

static int OscPatternMatch(const char* a, const char* b, 
                           int* wcmatches, int* numwc, 
                           int* numslots, int* slotcnt, int* rptcnt,
                           char* flag)
{
  if (!a) return -1; 
  if (!b) return 1;

  if (*a != '/')
  {
    if (flag) *flag=*a;
    ++a;
  }
  if (*b != '/')
  {
    if (flag) *flag=*b;
    ++b;
  }
    
  int wantwc = (wcmatches && numwc && numslots && slotcnt && rptcnt);

  while (*a && *b)
  {
    if (*a == *b || *a == '?' || *b == '?')
    {
      ++a;
      ++b;
    }
    else if (*a == '*' || *b == '*')
    {
      bool wca;

      if (*a == '*')
      {
        wca = true;
        if (!*++a) return 0; // wildcard is at end of specifier, match complete
      }
      else 
      {
        wca = false;
        if (!*++b) return 0; // wildcard is at end of specifier, match complete
      }

      int ai=0, bi=0;
      while (a[ai] && a[ai] != '/') ++ai;
      while (b[bi] && b[bi] != '/') ++bi;

      if (wca) 
      {
        if (ai>0) 
        {
          const char *cmp_ptr = b + bi - ai;
          if (cmp_ptr < b) cmp_ptr=b; 
          const int cmp=strncmp(a, cmp_ptr, ai);
          if (cmp) return cmp;
        }
      }
      else 
      {
        if (bi > 0) 
        {
          const char *cmp_ptr = a + ai - bi;
          if (cmp_ptr < a) cmp_ptr=a; 
          const int cmp=strncmp(b, cmp_ptr, bi);
          if (cmp) return cmp;
        }
      }
      a += ai;
      b += bi;
    }
    else if (*a == '@' || *b == '@')
    {
      bool wca = (*a == '@');
      if (!wca) _swap(a, b);

      ++a;
      while (*a == '@') // multiple @ in a row, match one digit to all but the last
      {
        if (isdigit_safe(*b))
        {
          if (wantwc && *numwc < MAX_OSC_WC*MAX_OSC_RPTCNT) 
          {
            wcmatches[(*numwc)++]=*b-'0';
            slotcnt[(*numslots)++]=1;
          }
          ++b;
        }
        ++a;
      }

      if (wantwc && *numwc < MAX_OSC_WC*MAX_OSC_RPTCNT) 
      {
        wcmatches[(*numwc)++]=parse_number_or_command_id(b);
        slotcnt[*numslots]=1;
      }
      b=skip_number_or_command_id(b);

      while (b[0] == ',' && (b[1] == '_' || isdigit_safe(b[1])))
      {
        ++b;
        if (wantwc && *numwc < MAX_OSC_WC*MAX_OSC_RPTCNT) 
        {
          wcmatches[(*numwc)++]=parse_number_or_command_id(b);
          int sc=++slotcnt[*numslots];
          if (sc > *rptcnt) *rptcnt=sc;
        }
        b=skip_number_or_command_id(b);
      }
      if (wantwc) (*numslots)++;

      if (!wca) _swap(a, b);
    }
    else
    {
      return *a-*b;
    }
  }

  return *a-*b;
}

static int CountWildcards(const char* msg)
{
  int cnt=0;
  while (*msg)
  {
    if (*msg == '@') ++cnt;
    ++msg;
  }
  return cnt;
}

static void PackCfg(WDL_String* str,
                    const char* name, int flags,
                    int recvport, const char* sendip, int sendport,
                    int maxpacketsz, int sendsleep,
                    const char* cfgfn)
{
  if (!name) name="";
  if (!sendip) sendip="";
  if (!cfgfn) cfgfn="";
  str->SetFormatted(1024, "\"%s\" %d %d \"%s\" %d %d %d \"%s\"", 
    name, flags, recvport, sendip, sendport, maxpacketsz, sendsleep, cfgfn);
}

static void ParseCfg(const char* cfgstr, 
                     char* name, int namelen, int* flags,
                     int* recvport, char* sendip, int* sendport,
                     int* maxpacketsz, int* sendsleep, 
                     char* cfgfn, int cfgfnlen)
{
  name[0]=0;
  *flags=0;
  *recvport=0;
  sendip[0]=0;
  *sendport=0;
  *maxpacketsz=DEF_MAXPACKETSZ;
  *sendsleep=DEF_SENDSLEEP;
  cfgfn[0]=0;

  if (strstr(cfgstr, "@@@")) return;  // reject config str from pre13

  LineParser lp;
  if (!lp.parse(cfgstr))
  {
    if (lp.getnumtokens() > 0) lstrcpyn(name, lp.gettoken_str(0), namelen);
    if (lp.getnumtokens() > 1) *flags=lp.gettoken_int(1);
    if (lp.getnumtokens() > 2) *recvport=lp.gettoken_int(2);
    if (lp.getnumtokens() > 3) lstrcpyn(sendip, lp.gettoken_str(3), 64);
    if (lp.getnumtokens() > 4) *sendport=lp.gettoken_int(4);
    if (lp.getnumtokens() > 5) *maxpacketsz=lp.gettoken_int(5);
    if (lp.getnumtokens() > 6) *sendsleep=lp.gettoken_int(6);
    if (lp.getnumtokens() > 7) lstrcpyn(cfgfn, lp.gettoken_str(7), cfgfnlen);
  }
}


struct OscVal 
{
  void Clear() 
  {
    memset(&val, 0x80, sizeof(val));
    *(char *)&val = 0;
  }
  bool UpdateFloat(float f)
  {
    if (val.fs.magic == 'f' && val.fs.v == f) return false;
    val.fs.magic = 'f';
    val.fs.v = f;
    return true;
  }
  bool UpdateInt(int f)
  {
    if (val.is.magic == 'i' && val.is.v == f) return false;
    val.is.magic = 'i';
    val.is.v = f;
    return true;
  }
  bool UpdateString(const char *str)
  {
    const size_t slen = strlen(str);
    WDL_UINT64 a = 0;
    if (slen <= 8) memcpy(&a, str, slen);
    else a = hash((unsigned char *)str);
    if (val.ss == a) return false;
    val.ss = a;
    return true;
  }

  union
  {
    struct floatState { 
      float v;
      int magic; // 'f'
    } fs;
    struct intState { 
      int v;
      int magic; // 'i'
    } is;
    WDL_UINT64 ss; // string state
  } val;

  static WDL_UINT64 hash(const unsigned char *p)
  {
    WDL_UINT64 h=WDL_UINT64_CONST(0xCBF29CE484222325);
    while (*p)
    {
      h *= WDL_UINT64_CONST(0x00000100000001B3);
      h ^= *p++;
    }
    return h;
  }
};

static int _osccmp_p(const char * const *a, const char * const *b)
{
  return OscPatternMatch(*a, *b, 0, 0, 0, 0, 0, 0);
}

static const char * const SPECIAL_STRING_CLEARCACHE = "//clearcache"; // never actually dereferenced, just a unique pointer
#define SETSURFNORM(pattern,nval) SetSurfaceVal(pattern,0,0,0,0,&nval,0)
#define SETSURFNORMWC(pattern,wc,numwc,nval) SetSurfaceVal(pattern,wc,numwc,0,0,&nval,0)
#define SETSURFFLOAT(pattern,fval) SetSurfaceVal(pattern,NULL,0,NULL,&fval,NULL,NULL)
#define SETSURFFLOATWC(pattern,wc,numwc,fval) SetSurfaceVal(pattern,wc,numwc,NULL,&fval,NULL,NULL)
#define SETSURFSTR(pattern,str) SetSurfaceVal(pattern,0,0,0,0,0,str)
#define SETSURFSTRWC(pattern,wc,numwc,str) SetSurfaceVal(pattern,wc,numwc,0,0,0,str)
#define SETSURFBOOL(pattern,bval) { double v=(double)bval; SetSurfaceVal(pattern,0,0,0,0,&v,0); }
#define SETSURFBOOLWC(pattern,wc,numwc,bval) { double v=(double)bval; SetSurfaceVal(pattern,wc,numwc,0,0,&v,0); }

#define SETSURFTRACKNORM(pattern,tidx,nval) SetSurfaceTrackVal(pattern,tidx,0,0,&nval,0)
#define SETSURFTRACKSTR(pattern,tidx,str) SetSurfaceTrackVal(pattern,tidx,0,0,0,str)
#define SETSURFTRACKBOOL(pattern,tidx,bval) { double v=(double)bval; SetSurfaceTrackVal(pattern,tidx,0,0,&v,0); }


typedef bool (*OscLocalCallbackFunc)(void* obj, const char* msg, int msglen);

struct OscLocalHandler
{
  void* m_obj;
  OscLocalCallbackFunc m_callback;
};


// copied from kbd.cpp
static void encode_relmode1_extended(double d, int *val, int *valhw)
{
  // float_value[-64.0..+63.0] = val7bit + (val7bit < 0 ? z/256 : val7bit > 0 ? -z/256 : 0)
  // valhw is -1-z where z is 0..255
  int val7bit = 0, z = 0;

  if (d < 0.0)
  {
    const double w = floor(d);
    if (w >= -64.0)
    {
      val7bit = (int) w;
      z = (int) ((d - w) * 256.0 + 0.5);
      WDL_ASSERT(val7bit >= -64 && val7bit < 0);
    }
    else
      val7bit = -64;
  }
  else if (d>0.0)
  {
    const double w = ceil(d);
    if (w <= 63.0)
    {
      val7bit = (int)w;
      z = (int) ((w - d) * 256.0 + 0.5);
      WDL_ASSERT(val7bit > 0 && val7bit < 0x40);
    }
    else
      val7bit = 63;
  }
  *val = val7bit & 0x7f;
  *valhw = -1-wdl_min(z,255);
}

class CSurf_Osc : public IReaperControlSurface
{
public:

  OscHandler* m_osc; // network I/O
  OscLocalHandler* m_osc_local; // local I/O

  WDL_String m_desc;
  WDL_String m_cfg;

  int m_curtrack;  // 0=master, 1=track 0, etc
  int m_curbankstart; // 0=track 0, etc
  int m_curfx;     // 0=first fx on m_curtrack, etc. TODO: handle m_curfx&(1<<24) for input FX (unsure how this will be handled now)
  int m_curfxparmbankstart;
  int m_curfxinstparmbankstart;
  int m_curmarkerbankstart;
  int m_curregionbankstart;

  int m_trackbanksize;
  int m_sendbanksize;
  int m_recvbanksize;
  int m_fxbanksize;
  int m_fxparmbanksize;
  int m_fxinstparmbanksize;
  int m_markerbanksize;
  int m_regionbanksize;

  double m_rotarylo;
  double m_rotarycenter;
  double m_rotaryhi;

  int m_followflag;  // &1=track follows last touched, &2=FX follows last touched, &4=FX follows focused FX, &8=track bank follows mixer, &16=reaper track sel follows device, &32=insert reaeq on any FX_EQ message
  int m_flags; // &1=enable receive, &2=enable send, &4=bind to actions/fxlearn

  int m_nav_active; // if m_altnav != 1, -1 or 1 while rewind/forward button is held down
  int m_altnav; // makes rewind/forward buttons 1=navigate markers, 2=edit loop pts

  int m_scrollx;
  int m_scrolly;
  int m_zoomx;
  int m_zoomy;

  // pattern table is simplest as a list, 
  // because we need to look up by both key and value
  WDL_PtrList<char> m_msgtab;

  WDL_StringKeyedArray<int> m_msgkeyidx; // [key] => index of key
  WDL_AssocArray<const char*, int> m_msgvalidx; // [value] => index of value

  // project state .. keep this to a minimum
  DWORD m_lastupd;
  double m_lastpos;  
  bool m_anysolo; 
  bool m_surfinit;
  WDL_StringKeyedArray2<OscVal> m_lastvals;

  int m_wantfx;   // &1=want fx parm feedback, &2=want last touched fx feedback, &4=want fx inst feedback, &8=want fx parm feedback for inactive tracks, &16=want fx inst feedback for inactive tracks, &32=want fxeq feedback, &64=want fxeq feedback for inactive tracks
  int m_wantpos;  // &1=time, &2=beats, &4=samples, &8=frames
  int m_wantvu; // &1=master, &2=other tracks, &4=stereo
  int m_wantmarker; // &1=markers, &2=regions, &4=last marker, &8=current region, &16=loop region

  const char* m_curedit; // the message that is currently being edited, to avoid feedback
  char m_curflag;

  char m_supports_touch;  // &1=vol, &2=pan, &4=width
  char m_hastouch[MAX_LASTTOUCHED_TRACK]; // &1=vol, &2=pan, &4=width
  
  DWORD m_vol_lasttouch[MAX_LASTTOUCHED_TRACK]; // only used if !(m_supports_touch&1)
  DWORD m_pan_lasttouch[MAX_LASTTOUCHED_TRACK]; // only used if !(m_supports_touch&2)
  DWORD m_pan2_lasttouch[MAX_LASTTOUCHED_TRACK]; // only used if !(m_supports_touch&4)

  CSurf_Osc(const char* name, int flags, 
            int recvport, const char* sendip, int sendport,
            int maxpacketsz, int sendsleep, 
            OscLocalHandler* osc_local,
            const char* cfgfn)
  : m_msgkeyidx(true, NULL, false),  m_msgvalidx(_osccmp_p), m_lastvals(true)
  {
    m_osc=0;  
    m_osc_local=osc_local;

    if (!name) name="";
    if (!sendip) sendip="";

    m_flags=flags;
    bool rcven=!!(flags&1);
    bool senden=!!(flags&2);

    m_desc.Set("OSC");
    if (name[0]) 
    {
      m_desc.AppendFormatted(1024, ": %s", name);
    }
    else
    {
      if (rcven || senden) m_desc.Append(" (");
      if (rcven) 
      {
        if (!recvport && senden)
          m_desc.Append(__LOCALIZE("recv-from-destination","csurf_osc"));
        else
          m_desc.AppendFormatted(512, __LOCALIZE_VERFMT("recv %d","csurf_osc"), recvport);
      }
      if (rcven && senden) m_desc.Append(", ");
      if (senden) 
      {
        if (!sendport && rcven)
          m_desc.Append(__LOCALIZE("send-to-last-source","csurf_osc"));
        else
          m_desc.AppendFormatted(512, __LOCALIZE_VERFMT("send %s:%d","csurf_osc"), sendip, sendport);
      }
      if (rcven || senden) m_desc.Append(")");
    }

    PackCfg(&m_cfg, name, flags, recvport, sendip, sendport, maxpacketsz, sendsleep, cfgfn);

    m_curtrack=0;
    m_curbankstart=0;
    m_curfx=0;
    m_curfxparmbankstart=0;
    m_curfxinstparmbankstart=0;
    m_curmarkerbankstart=0;
    m_curregionbankstart=0;

    m_nav_active=0;
    m_altnav=0;
    m_scrollx=0;
    m_scrolly=0;
    m_zoomx=0;
    m_zoomy=0;

    m_surfinit=false;
    m_lastupd=0;
    m_lastpos=0.0;
    m_anysolo=false;

    m_curedit=0;
    m_curflag=0;

    m_trackbanksize=8;
    m_sendbanksize=4;
    m_recvbanksize=4;
    m_fxbanksize=8;
    m_fxparmbanksize=16;
    m_fxinstparmbanksize=16;
    m_markerbanksize=0;
    m_regionbanksize=0;

    m_rotarylo=-1.0;
    m_rotarycenter=0.0;
    m_rotaryhi=1.0;

    m_followflag=0;
    m_flags=flags;

    m_wantfx=0;
    m_wantpos=0;
    m_wantvu=0;

    m_supports_touch=0;
    memset(m_hastouch, 0, sizeof(m_hastouch));
    memset(m_vol_lasttouch, 0, sizeof(m_vol_lasttouch));
    memset(m_pan_lasttouch, 0, sizeof(m_pan_lasttouch));
    memset(m_pan2_lasttouch, 0, sizeof(m_pan2_lasttouch));

    WDL_PtrList<char> customcfg;
    LoadCfgFile(cfgfn, &customcfg, NULL, 0);

    if (!customcfg.GetSize())
    {
      ProjectStateContext *ctx = getDefaultOSCContext();
      if (ctx)
      {
        LoadCfgContext(ctx,&customcfg,NULL,0);
        delete ctx;
      }
    }

    int i;
    for (i=0; i < customcfg.GetSize(); ++i)
    {
      LineParserInt lp;
      if (lp.parseDestroyBuffer(customcfg.Get(i)) || lp.getnumtokens() < 2) continue;

      const char* key=lp.gettoken_str(0);
      if (key[0] == '#') continue;

      int fx_feedback_wc=0;
      bool skipdef=false;
      const char* pattern=lp.gettoken_str(1);

      if (strstarts(key, "DEVICE_") || strstarts(key, "REAPER_"))
      {   
        if (strends(key, "_COUNT"))
        {
          if (isint(pattern)) 
          {
            int a=atoi(pattern);
            if (a > 0)
            {
              if (strends(key, "_TRACK_COUNT")) m_trackbanksize=a;
              else if (strends(key, "_SEND_COUNT")) m_sendbanksize=a;
              else if (strends(key, "_RECEIVE_COUNT")) m_recvbanksize=a;
              else if (strends(key, "_FX_COUNT")) m_fxbanksize=a;
              else if (strends(key, "_FX_PARAM_COUNT")) m_fxparmbanksize=a;
              else if (strends(key, "_FX_INST_PARAM_COUNT")) m_fxinstparmbanksize=a;
              else if (strends(key, "_MARKER_COUNT")) m_markerbanksize=a;
              else if (strends(key, "_REGION_COUNT")) m_regionbanksize=a;
            }      
            skipdef=true;
          }
        }
        else if (strends(key, "_ROTARY_CENTER"))
        {
          if (isfloat(pattern))
          {
            double center=atof(pattern);
            if (center == 0.0f)
            {
              m_rotarylo=-1.0;
              m_rotarycenter=0.0;
              m_rotaryhi=1.0;
            }
            else if (center > 0.0f)
            {
              m_rotarylo=0.0;
              m_rotarycenter=center;
              m_rotaryhi=2.0*center;
            }
            skipdef=true;
          }
        }
        else if (strends(key, "_FOLLOWS"))
        {      
          if (!strcmp(pattern, "DEVICE") ||
              !strcmp(pattern, "REAPER") ||
              !strcmp(pattern, "MIXER") ||
              !strcmp(pattern, "LAST_TOUCHED") ||
              !strcmp(pattern, "FOCUSED"))
          {
            if (!strcmp(key, "REAPER_TRACK_FOLLOWS"))
            {
              if (!strcmp(pattern, "DEVICE")) m_followflag |= 16;
            }
            else if (!strcmp(key, "DEVICE_TRACK_FOLLOWS"))
            {
              if (!strcmp(pattern, "LAST_TOUCHED")) m_followflag |= 1;
            }
            else if (!strcmp(key, "DEVICE_FX_FOLLOWS"))
            {
              if (!strcmp(pattern, "LAST_TOUCHED")) m_followflag |= 2;
              else if (!strcmp(pattern, "FOCUSED")) m_followflag |= 4;
            }
            else if (!strcmp(key, "DEVICE_TRACK_BANK_FOLLOWS"))
            {
              if (!strcmp(pattern, "MIXER")) m_followflag |= 8;
            }
            skipdef=true;
          }
        }
        else if (strends(key, "_EQ"))
        {
          if (!strcmp(pattern, "INSERT")) m_followflag |= 32;
          skipdef=true;
        }
      }
      else
      {
        if (!strcmp(key, "TIME")) m_wantpos |= 1;      
        else if (!strcmp(key, "BEAT")) m_wantpos |= 2;      
        else if (!strcmp(key, "SAMPLES")) m_wantpos |= 4;
        else if (!strcmp(key, "FRAMES")) m_wantpos |= 8;
        else if (!strcmp(key, "MASTER_VU")) m_wantvu |= 1;
        else if (!strcmp(key, "MASTER_VU_L") || !strcmp(key, "MASTER_VU_R")) m_wantvu |= (1|4);
        else if (!strcmp(key, "TRACK_VU")) m_wantvu |= 2;
        else if (!strcmp(key, "TRACK_VU_L") || !strcmp(key, "TRACK_VU_R")) m_wantvu |= (2|4);
        else if (strstarts(key, "FX_PARAM_VALUE") || !strcmp(key, "FX_WETDRY")) m_wantfx |= 1;
        else if (strstarts(key, "LAST_TOUCHED_FX_")) m_wantfx |= 2;
        else if (strstarts(key, "FX_INST_")) m_wantfx |= 4;
        else if (strstarts(key, "FX_EQ_")) m_wantfx |= 32;
        else if (!strncmp(key, "MARKER_",7)) m_wantmarker |= 1;
        else if (!strncmp(key, "REGION_",7)) m_wantmarker |= 2;
        else if (!strncmp(key, "LAST_MARKER_",12)) m_wantmarker |= 4;
        else if (!strncmp(key, "LAST_REGION_",12)) m_wantmarker |= 8;
        else if (!strcmp(key, "LOOP_START_TIME") || !strcmp(key, "LOOP_END_TIME")) m_wantmarker |= 16;
        else if (!strcmp(key, "TRACK_VOLUME_TOUCH")) m_supports_touch |= 1;
        else if (!strcmp(key, "TRACK_PAN_TOUCH")) m_supports_touch |= 2;
        else if (!strcmp(key, "TRACK_PAN2_TOUCH")) m_supports_touch |= 4;

        // want inactive track FX feedback only if we have this many wildcards
        if (strstarts(key, "FX_PARAM_VALUE")) fx_feedback_wc=3;
        else if (!strcmp(key, "FX_WETDRY")) fx_feedback_wc=2;
        else if (strstarts(key, "FX_INST_")) fx_feedback_wc=2;
        else if (strstarts(key, "FX_EQ_")) fx_feedback_wc=2;
      }
      
      if (skipdef && lp.getnumtokens() < 3) continue;

      // check if we have already seen this key
      int pos;
      for (pos=0; pos < m_msgtab.GetSize(); ++pos)
      {
        if ((!pos || !m_msgtab.Get(pos-1)) && !strcmp(key, m_msgtab.Get(pos))) break;
      }
      if (pos < m_msgtab.GetSize()) ++pos;
      else m_msgtab.Insert(pos++, strdup(key));    
      
      int j;
      for (j = (skipdef ? 2 : 1); j < lp.getnumtokens(); ++j)
      {
        pattern=lp.gettoken_str(j);
        
#ifdef _DEBUG
        assert(strchr("nfbtrsi", pattern[0]));
        assert(pattern[1] == '/');
#endif

        m_msgtab.Insert(pos++, strdup(pattern));

        if (fx_feedback_wc && CountWildcards(pattern) >= fx_feedback_wc)
        {
          if (strstarts(key, "FX_INST_PARAM_VALUE")) m_wantfx |= 16;
          else if (strstarts(key, "FX_EQ_")) m_wantfx |= 32;
          else m_wantfx |= 8;
        }
      }

      if (pos >= m_msgtab.GetSize()) m_msgtab.Add(0);
    }

    for (i=0; i < m_msgtab.GetSize(); ++i)
    {
      const char* p=m_msgtab.Get(i);
      if (!p) continue;
      if (!i || !m_msgtab.Get(i-1)) m_msgkeyidx.Insert(p, i);
      else m_msgvalidx.Insert(p, i);
    }

    customcfg.Empty(true, free);
    
    if (!senden)
    {
      m_wantfx=m_wantpos=m_wantvu=m_wantmarker=0;
    }

    if (rcven || senden)
    {
      m_osc=new OscHandler;

      m_osc->m_recv_enable=rcven;
      if (m_flags&4) m_osc->m_recv_enable |= 2;
      m_osc->m_recvsock=INVALID_SOCKET;
      if (rcven)
      {
        m_osc->m_recvaddr.sin_family=AF_INET;
        m_osc->m_recvaddr.sin_addr.s_addr=htonl(INADDR_ANY);
        m_osc->m_recvaddr.sin_port=htons(recvport);
      }

      m_osc->m_send_enable=senden;
      m_osc->m_sendsock=INVALID_SOCKET;
      if (senden)
      {
        m_osc->m_sendaddr.sin_family=AF_INET;
        m_osc->m_sendaddr.sin_addr.s_addr=inet_addr(sendip);
        m_osc->m_sendaddr.sin_port=htons(sendport);
      }

      m_osc->m_maxpacketsz=maxpacketsz;
      m_osc->m_sendsleep=sendsleep;

      m_osc->m_obj=this;
      m_osc->m_handler=ProcessOscMessage;

      OscInit(m_osc);
    }
  }

  ~CSurf_Osc()
  {
    if (m_osc)
    {
      OscQuit(m_osc);
      delete m_osc;
      m_osc=0;
    }
    if (m_osc_local)
    {
      delete m_osc_local;
      m_osc_local=0;
    }
    m_msgtab.Empty(true, free);
  }

  const char* GetTypeString()
  {
    return "OSC";
  }

  const char* GetDescString()
  {
    return m_desc.Get();
  }

  const char* GetConfigString()
  {
    return m_cfg.Get();
  }

  void Run()
  {
    if (m_osc) OscGetInput(m_osc);

    if (m_nav_active)
    {
      if (!m_altnav)
      {
        if (m_nav_active < 0) CSurf_OnRewFwd(1,-1);
        else if (m_nav_active > 0) CSurf_OnRewFwd(1,1);
      }
      else if (m_altnav == 2)
      {
        Loop_OnArrow(0, m_nav_active);
      }
      else
      {
        m_nav_active=0;
      }
    }

    if (m_scrollx || m_scrolly)
    {
      CSurf_OnScroll(m_scrollx, m_scrolly);
    }
    if (m_zoomx || m_zoomy)
    {
      CSurf_OnZoom(m_zoomx, m_zoomy);
    }

    if (osc_send_inactive()) return;

    bool wantmarker=!!(m_wantmarker&4);
    bool wantregion=!!(m_wantmarker&8);
    const bool wantloopregion = (m_wantmarker&16)==16;

    if (!m_wantpos && !m_wantvu && !wantmarker && !wantregion && !wantloopregion) return;

    DWORD now=timeGetTime();
    if ((now-m_lastupd) >= (1000/max((*g_config_csurf_rate),1)))
    {
      m_lastupd=now;      

      double pos=0.0;
      bool poschg=false;

      if (m_wantpos || wantmarker || wantregion)
      {
        int ps=GetPlayState();
        pos=((ps&1) ? GetPlayPosition() : GetCursorPosition());
        if (pos != m_lastpos)
        { 
          m_lastpos=pos;
          poschg=true;
        }
      }

      if (m_wantpos && poschg)
      {
        char buf[512];
        if (m_wantpos&1)
        {
          format_timestr_pos(pos, buf, sizeof(buf), 0);
          SetSurfaceVal("TIME", 0, 0, 0, &pos, 0, buf);
        }
        if (m_wantpos&2)
        {
          format_timestr_pos(pos, buf, sizeof(buf), 1);
          SETSURFSTR("BEAT", buf);
        }
        if (m_wantpos&4)
        {
          format_timestr_pos(pos, buf, sizeof(buf), 4);
          double spos=atof(buf);
          SetSurfaceVal("SAMPLES", 0, 0, 0, &spos, 0, buf);
        }
        if (m_wantpos&8)
        {
          format_timestr_pos(pos, buf, sizeof(buf), 5);
          SETSURFSTR("FRAMES", buf);
        }
      }

      if (m_wantvu)
      { 
        double minvu=(double)*g_vu_minvol;
        double maxvu=(double)*g_vu_maxvol;      

        int starti = ((m_wantvu&1) ? -1 : 0);
        int endi = ((m_wantvu&2) ? m_trackbanksize : -1);
        int i;
        for (i=starti; i <= endi; ++i)
        {
          int tidx;
          if (i == -1) tidx=0; // master
          else if (!i) tidx=m_curtrack;
          else tidx=m_curbankstart+i;
          MediaTrack* tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));

          double vu=0.0;
          double vuL=0.0;
          double vuR=0.0;
          if (tr) 
          {
            double vL=Track_GetPeakInfo(tr,0);
            double vR=Track_GetPeakInfo(tr,1);

            vu=VAL2DB((vL+vR)*0.5);
            if (vu < minvu) vu=0.0;
            else if (vu > maxvu) vu=1.0;
            else vu=(vu-minvu)/(maxvu-minvu);

            if (m_wantvu&4)
            {
              vuL=VAL2DB(vL);
              if (vuL < minvu) vuL=0.0;
              else if (vuL > maxvu) vuL=1.0;
              else vuL=(vuL-minvu)/(maxvu-minvu);
  
              vuR=VAL2DB(vR);
              if (vuR < minvu) vuR=0.0;
              else if (vuR > maxvu) vuR=1.0;
              else vuR=(vuR-minvu)/(maxvu-minvu);
            }
          }
          
          if (i < 0) 
          {
            SETSURFNORM("MASTER_VU", vu);
            if (m_wantvu&4)
            {
              SETSURFNORM("MASTER_VU_L", vuL);
              SETSURFNORM("MASTER_VU_R", vuR);
            }
          }
          else if (!i)
          {
            SETSURFNORM("TRACK_VU", vu);
            if (m_wantvu&4)
            {
              SETSURFNORM("TRACK_VU_L", vuL);
              SETSURFNORM("TRACK_VU_R", vuR);
            }
          }
          else
          {
            int wc[1] = { i };
            SETSURFNORMWC("TRACK_VU", wc, 1, vu);
            if (m_wantvu&4)
            {
              SETSURFNORMWC("TRACK_VU_L", wc, 1, vuL);
              SETSURFNORMWC("TRACK_VU_R", wc, 1, vuR);
            }
          }
        }
      }

      if (wantloopregion)
      {
        double st=0, end=0;
        GetSet_LoopTimeRange2(NULL, false, true, &st,&end,false);
        SETSURFFLOAT("LOOP_START_TIME", st);
        SETSURFFLOAT("LOOP_END_TIME", end);
      }
      if (wantmarker || wantregion)
      {
        int mi=-1;
        int ri=-1;
        GetLastMarkerAndCurRegion(NULL, pos, (wantmarker ? &mi : NULL), (wantregion ? &ri : NULL));

        char buf[128];
        if (wantmarker)
        {
          const char* name="";
          buf[0]=0;
          double pos = 0.0;
          if (mi >= 0)
          {
            int idx=0;
            EnumProjectMarkers3(NULL,mi, NULL, &pos, NULL, &name, &idx,NULL);
            if (!name) name="";
            if (idx > 0) sprintf(buf, "%d", idx);
          }
          SETSURFSTR("LAST_MARKER_NAME", name);
          SETSURFSTR("LAST_MARKER_NUMBER", buf);
          SETSURFFLOAT("LAST_MARKER_TIME", pos);
        }
        if (wantregion)
        {
          const char* name="";
          buf[0]=0;
          double pos = 0.0, endpos=0.0;
          if (ri >= 0)
          {            
            int idx=0;
            EnumProjectMarkers3(NULL,ri, NULL,&pos, &endpos, &name, &idx,NULL);
            if (!name) name="";
            if (idx > 0) sprintf(buf, "%d", idx);
            if (endpos>pos) endpos-=pos;
            else endpos=0.0;
          }
          SETSURFSTR("LAST_REGION_NAME", name);
          SETSURFSTR("LAST_REGION_NUMBER", buf);
          SETSURFFLOAT("LAST_REGION_TIME", pos);
          SETSURFFLOAT("LAST_REGION_LENGTH", endpos);
        }
      }
    }          
  }


  static bool ProcessOscMessage(void* _this, OscMessageRead* rmsg)
  {
    return ((CSurf_Osc*)_this)->ProcessMessage(rmsg);
  }

  const char* FindOscMatch(const char* msg, int* wc, int* numwc, int* rptcnt, char* flag)
  {
    int* validx=m_msgvalidx.GetPtr(msg);
    if (!validx) return 0; // message didn't match anything
    
    if (!wc || !numwc || !rptcnt) return 0; // assert

    const char* p=m_msgtab.Get(*validx);

    // call patterncmp function again to fill in wildcards  
    int numslots=0;
    int slotcnt[MAX_OSC_WC] = { 0 }; // for each slot, the count of comma-separated wildcards
    if (OscPatternMatch(msg, p, wc, numwc, &numslots, slotcnt, rptcnt, flag)) return 0; // assert
    
    if (*rptcnt > 1)
    {
      if (*rptcnt > MAX_OSC_RPTCNT) return 0;
      int wc2[MAX_OSC_WC*MAX_OSC_RPTCNT] = { 0 };
      
      int i, j;
      int pos=0;   
      for (i=0; i < numslots; ++i)
      {             
        // every slot must have either 1 or rptcnt wildcards
        if (slotcnt[i] != 1 && slotcnt[i] != *rptcnt) return 0;
        for (j=0; j < *rptcnt; ++j)
        {
          int pos2=i+numslots*j;
          wc2[pos2]=wc[pos];
          if (slotcnt[i] == *rptcnt) ++pos;
        }
        if (slotcnt[i] == 1) ++pos;
      }
      *numwc=*rptcnt*numslots;
      memcpy(wc, wc2, *numwc*sizeof(int));
    }

    if (numwc && *numwc > 1)
    {    
      const char* q=strchr(p, '@');
      if (q && *(q+1) != '@') // nonconsecutive wildcards
      {
        _reverse(wc, *numwc);
      }
    }   

    // back up until we find the key
    int i;
    for (i=*validx-1; i >= 0; --i)
    {
      p=m_msgtab.Get(i-1);
      if (!p) break;
    }
    
    return m_msgtab.Get(i);    
  }

  static double GetFloatArg(OscMessageRead* rmsg, char flag, bool* hasarg=0)
  {  
    if (hasarg) *hasarg=true;
    if (strchr("nfbtr", flag)) 
    {
      const float* f=rmsg->PopFloatArg(false);
      if (f) return (double)*f;   
      const int* i=rmsg->PopIntArg(false);
      if (i) return (double)*i;
    }
    else if (flag == 'i')
    {
      const int* i=rmsg->PopIntArg(false);
      if (i) return (double)*i;
      const float* f=rmsg->PopFloatArg(false);
      if (f) return (double)*f;      
    }    
    else if (flag == 's')
    {
      const char* s=rmsg->PopStringArg(false);
      if (s) return atof(s);
    }
    if (hasarg) *hasarg=false;
    if (flag == 't') return 1.0;  // trigger messages do not need an argument
    return 0.0;
  }

  int TriggerMessage(OscMessageRead* rmsg, char flag)
  {
    if (strchr("ntbr", flag))
    {           
      bool hasarg=false;
      double v=GetFloatArg(rmsg, flag, &hasarg); 
      if (flag == 'b' && hasarg && v == 0.0) return -1;
      if (flag == 'b' && hasarg && v == 1.0) return 1;  
      if (flag == 't' && (!hasarg || v == 1.0)) return 1; 
      if (flag == 'r' && hasarg && v < m_rotarycenter) return -1;
      if (flag == 'r' && hasarg && v > m_rotarycenter) return 1; 
    }
    return 0;
  }

  bool ProcessGlobalAction(OscMessageRead* rmsg, const char* pattern, char flag)
  {
    // trigger messages do not latch for scroll/zoom
    bool scroll=strstarts(pattern, "SCROLL_");
    bool zoom=strstarts(pattern, "ZOOM_");
    if (scroll || zoom)
    {     
      int len=strlen(scroll ? "SCROLL_" : "ZOOM_");
      char axis=pattern[len];
      char dir=pattern[len+1];
      int iv=0;
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        if (flag == 'r') iv=t;
        else if (dir == '-' && t > 0) iv=-1;
        else if (dir == '+' && t > 0) iv=1;
        int dx = (axis == 'X' ? iv : 0);
        int dy = (axis == 'Y' ? iv : 0);
        if (scroll) 
        {
          if (flag == 'b')
          {
            dx = m_scrollx = (m_scrollx ? 0 : dx);
            dy = m_scrolly = (m_scrolly ? 0 : dy);
          }
          if (dx || dy) CSurf_OnScroll(dx, dy);
        }
        else if (zoom)
        {
          if (flag == 'b')
          {
            dx = m_zoomx = (m_zoomx ? 0 : dx);
            dy = m_zoomy = (m_zoomy ? 0 : dy);
          }
          if (dx || dy) CSurf_OnZoom(dx, dy);
        }
      }
      return true;
    }

    // untested
    bool time=!strcmp(pattern, "TIME");
    bool beat=!strcmp(pattern, "BEAT");
    bool samples=!strcmp(pattern, "SAMPLES");
    bool frames=!strcmp(pattern, "FRAMES");
    if (time || beat || samples || frames)
    {
      double v=0.0;
      if (flag == 'f')
      {
        v=GetFloatArg(rmsg, flag);
        if (time) // time in seconds
        {
        }
        else if (beat)
        {
          v=TimeMap2_beatsToTime(0, v, 0);
        }
        else if (samples)
        {
          char buf[128];
          snprintf(buf,sizeof(buf), "%.0f", v);
          v=parse_timestr_pos(buf, 4);
        }
        else // can't parse frames
        {
          return true;
        }
        SetEditCurPos(v, true, true);
      }
      else if (flag == 's')
      {
        const char* s=rmsg->PopStringArg(false);
        if (s)
        {
          int mode;
          if (time) mode=0;
          else if (beat) mode=1;
          else if (samples) mode=4;
          else if (frames) mode=5;
          else return true;
          v=parse_timestr_pos(s, mode);

          SetEditCurPos(v, true, true);
        }        
      }
      return true;
    }

    if (!strcmp(pattern, "METRONOME"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        int click=GetToggleCommandState(40364); // toggle metronome
        if (flag == 't' || click != (t > 0))
        {     
          Main_OnCommand(40364, 0);
          Extended(CSURF_EXT_SETMETRONOME, (void*)(!click), 0, 0);
        }
      }
      return true;
    }

    if (!strcmp(pattern, "REPLACE"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        int repl=GetToggleCommandState(41186); // replace mode
        if (flag == 't' || repl != (t > 0))
        {
          if (repl) Main_OnCommand(41330, 0); // autosplit
          else Main_OnCommand(41186, 0);
        }
      }
      return true;
    }

    if (!strcmp(pattern, "REPEAT"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        int rpt=GetSetRepeat(-1);
        if (flag == 't' || rpt != (t > 0))
        {
          GetSetRepeat(2);
        }
      }
      return true;
    }

    bool rec=!strcmp(pattern, "RECORD");
    bool stop=!strcmp(pattern, "STOP");
    bool play=!strcmp(pattern, "PLAY");
    bool pause=!strcmp(pattern, "PAUSE");
    if (rec || stop || play || pause)
    {
      if (TriggerMessage(rmsg, flag))
      {
        if (rec) CSurf_OnRecord();
        else if (stop) CSurf_OnStop();
        else if (play) CSurf_OnPlay();           
        else if (pause) CSurf_OnPause();
        int ps=GetPlayState();
        SetPlayState(!!(ps&1), !!(ps&2), !!(ps&4));
      }
      return true;
    }

    if (!strcmp(pattern, "AUTO_REC_ARM"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        int recarm=GetToggleCommandState(40740); // auto-recarm all tracks
        if (flag == 't' || recarm != (t > 0))
        {
          Main_OnCommand(40740, 0);
        }
      }
      return true;
    }

    if (!strcmp(pattern, "SOLO_RESET"))
    {
      if (TriggerMessage(rmsg, flag) > 0)
      {
        SoloAllTracks(0);
      }
      return true;
    }

    // maybe remove this
    if (!strcmp(pattern, "ANY_SOLO"))
    {
      if (TriggerMessage(rmsg, flag) > 0)
      {
        SoloAllTracks(0);
      }
      return true;
    }    

    // trigger messages latch for rew/fwd
    bool rew=!strcmp(pattern, "REWIND");
    bool fwd=!strcmp(pattern, "FORWARD");
    if (rew || fwd)
    {
      if (!m_altnav)
      {
        int t=TriggerMessage(rmsg, flag);
        if (t)
        {              
          if (rew)
          {
            if (flag == 't' || (m_nav_active < 0) != (t > 0))
            {
              m_nav_active = (m_nav_active ? 0 : -1);
            }
            if (m_nav_active) CSurf_OnRewFwd(1,-1);
          }
          else if (fwd)
          {
            if (flag == 't' || (m_nav_active > 0) != (t > 0))
            {
              m_nav_active = (m_nav_active ? 0 : 1);
            }
            if (m_nav_active) CSurf_OnRewFwd(1,1); 
          }
        }
        bool en = ((rew && m_nav_active < 0) || (fwd && m_nav_active > 0));
        SETSURFBOOL(pattern, en);
      }
      else if (m_altnav == 1) // nav by marker
      {   
        if (TriggerMessage(rmsg, flag) > 0)
        {
          m_nav_active=0;
          if (rew) Main_OnCommand(40172, 0); // go to previous marker
          else if (fwd) Main_OnCommand(40173, 0); // go to next marker
        }
      }
      else if (m_altnav == 2) // move loop points
      {
        bool snaploop=Loop_OnArrow(0, 0);
        int t=TriggerMessage(rmsg, flag);
        if (t)
        {
          if (snaploop && t > 0)
          {
            if (rew) Loop_OnArrow(0, -1);
            else if (fwd) Loop_OnArrow(0, 1);
          }        
          else if (!snaploop)
          {
            if (rew)
            {
              if (flag == 't' || (m_nav_active < 0) != (t > 0))
              {
                m_nav_active = (m_nav_active ? 0 : -1);
              }       
              if (m_nav_active) Loop_OnArrow(0, -1);          
            }          
            else if (fwd)
            {
              if (flag == 't' || (m_nav_active > 0) != (t > 0))
              {
                m_nav_active = (m_nav_active ? 0 : 1);
              }       
              if (m_nav_active) Loop_OnArrow(0, 1);      
            }
          }
        }
        if (!snaploop)
        {
          bool en = ((rew && m_nav_active < 0) || (fwd && m_nav_active > 0));
          SETSURFBOOL(pattern, en);
        }
      }
      return true;
    }

    bool bymarker=!strcmp(pattern, "REWIND_FORWARD_BYMARKER");
    bool setloop=!strcmp(pattern, "REWIND_FORWARD_SETLOOP");
    if (bymarker || setloop)
    {
      m_nav_active=0;
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        if (bymarker)
        {     
          if (flag == 't' || (m_altnav == 1) != (t > 0))
          {
            m_altnav = (m_altnav == 1 ? 0 : 1);
          }
        }
        else if (setloop)
        {
          if (flag == 't' || (m_altnav == 2) != (t > 0))
          {
            m_altnav = (m_altnav == 2 ? 0 : 2);
          }
        }
        SETSURFBOOL("REWIND_FORWARD_BYMARKER", (m_altnav == 1));
        SETSURFBOOL("REWIND_FORWARD_SETLOOP", (m_altnav == 2));
      }
      return true;
    }

    if (!strcmp(pattern, "SCRUB"))
    {   
      m_nav_active=0;
      if (flag == 'r')
      {
        int t=TriggerMessage(rmsg, flag);
        if (t < 0) CSurf_OnRewFwd(1,-1);
        else if (t > 0) CSurf_OnRewFwd(1,1);
      }
      return true;
    }

    if (!strcmp(pattern, "PLAY_RATE"))
    {
      if (strchr("nfr", flag))
      {
        double pr=GetFloatArg(rmsg, flag); 
        if (flag == 'r')
        {
          double opr=Master_GetPlayRate(0);
          opr=Master_NormalizePlayRate(opr, false);
          pr=opr+ROTARY_STEP*2.0*(pr-m_rotarycenter)/(m_rotaryhi-m_rotarylo);
        }
        if (flag == 'n' || flag == 'r')
        {
          pr=Master_NormalizePlayRate(pr, true);
        }
        CSurf_OnPlayRateChange(pr);
      }
      return true;
    }

    if (!strcmp(pattern, "TEMPO"))
    {
      if (strchr("nfr", flag))
      {
        double bpm=GetFloatArg(rmsg, flag);         
        if (flag == 'r')
        {
          double obpm=Master_GetTempo();
          obpm=Master_NormalizeTempo(obpm, false);
          bpm=obpm+ROTARY_STEP*2.0*(bpm-m_rotarycenter)/(m_rotaryhi-m_rotarylo);
        }
        if (flag == 'n' || flag == 'r')
        {
          bpm=Master_NormalizeTempo(bpm, true);
        }
        CSurf_OnTempoChange(bpm);
      }
      return true;
    }

    if (!strcmp(pattern,"LOOP_START_TIME") || !strcmp(pattern,"LOOP_END_TIME"))
    {
      if (flag == 'f')
      {
        double v=GetFloatArg(rmsg, flag);

        double st=0.0,end=0.0;
        GetSet_LoopTimeRange2(NULL, false, true, &st,&end,false);
        if (!strcmp(pattern,"LOOP_START_TIME")) st = v;
        else end = v;
        GetSet_LoopTimeRange2(NULL, true, true, &st,&end,true);
      }
      return true;
    }

    return false;
  }

  bool ProcessStateAction(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    if (!strstarts(pattern, "DEVICE_") && !strstarts(pattern, "REAPER_")) return false;

    if (strends(pattern, "_COUNT"))
    {
      int ival=-1;
      if (flag == 'i')
      {
        const int* i=rmsg->PopIntArg(false);
        const float* f=rmsg->PopFloatArg(false);
        if (i) ival=*i;
        else if (f) ival=(int)*f;
      }
      else if (numwc > 0 && TriggerMessage(rmsg, flag) > 0)
      {
        ival=wc[0];
      }
      if (ival >= 0)
      {
        if (strends(pattern, "_TRACK_COUNT"))
        {
          m_trackbanksize=ival;
          SetTrackListChange();
        }
        else if (strends(pattern, "_SEND_COUNT"))
        {
          m_sendbanksize=ival;
          SetTrackListChange();
        }
        else if (strends(pattern, "_RECEIVE_COUNT"))
        {
          m_recvbanksize=ival;
          SetTrackListChange();
        }
        else if (strends(pattern, "_FX_COUNT"))
        {
          m_fxbanksize=ival;
          SetTrackListChange();
        }
        else if (strends(pattern, "_FX_PARAM_COUNT"))
        {
          m_fxparmbanksize=ival;
          SetActiveFXChange();
        }
        else if (strends(pattern, "_FX_INST_PARAM_COUNT"))
        {
          m_fxinstparmbanksize=ival;
          SetActiveFXInstChange();
        }
        else if (strends(pattern, "_MARKER_COUNT"))
        {
          const int oldmarker=m_markerbanksize;
          m_markerbanksize=ival;
          SetMarkerBankChange(oldmarker,0);
        }
        else if (strends(pattern, "_REGION_COUNT"))
        {
          const int oldregion=m_regionbanksize;
          m_regionbanksize=ival;
          SetMarkerBankChange(0,oldregion);
        }
      }
      return true;
    }

    if (strstr(pattern, "_FOLLOWS"))
    {
      if (flag == 's' || TriggerMessage(rmsg, flag) > 0)
      {
        if (flag == 's')
        {
          const char* s=rmsg->PopStringArg(false);
          if (s)
          {      
            if (!strcmp(pattern, "REAPER_TRACK_FOLLOWS"))
            {
              if (!strcmp(s, "DEVICE")) pattern="REAPER_TRACK_FOLLOWS_DEVICE";
              else pattern="REAPER_TRACK_FOLLOWS_REAPER";
            }
            else if (!strcmp(pattern, "DEVICE_TRACK_FOLLOWS"))
            {
              if (!strcmp(s, "LAST_TOUCHED")) pattern="DEVICE_TRACK_FOLLOWS_LAST_TOUCHED";
              else pattern="DEVICE_TRACK_FOLLOWS_DEVICE";
            }
            else if (!strcmp(pattern, "DEVICE_TRACK_BANK_FOLLOWS"))
            {
              if (!strcmp(s, "MIXER")) pattern="DEVICE_TRACK_BANK_FOLLOWS_MIXER";
              else pattern="DEVICE_TRACK_BANK_FOLLOWS_DEVICE";
            }
            else if (!strcmp(pattern, "DEVICE_FX_FOLLOWS"))
            {
              if (!strcmp(s, "LAST_TOUCHED")) pattern="DEVICE_FX_FOLLOWS_LAST_TOUCHED";
              else if (!strcmp(s, "FOCUSED")) pattern="DEVICE_FX_FOLLOWS_FOCUSED";
              else pattern="DEVICE_FX_FOLLOWS_DEVICE";
            }
          }
        }
        
        if (strstarts(pattern, "REAPER_TRACK_FOLLOWS_"))
        {
          m_followflag &= ~16;
          if (!strcmp(pattern, "REAPER_TRACK_FOLLOWS_DEVICE"))
          {
            m_followflag |= 16;
            MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
            if (tr) SetOnlyTrackSelected(tr);
          }
        }
        else if (strstarts(pattern, "DEVICE_TRACK_FOLLOWS_"))
        {
          m_followflag &= ~1;
          if (!strcmp(pattern, "DEVICE_TRACK_FOLLOWS_LAST_TOUCHED")) 
          {
            m_followflag |= 1;
            MediaTrack* tr=GetLastTouchedTrack();
            if (tr) Extended(CSURF_EXT_SETLASTTOUCHEDTRACK, tr, 0, 0);
          }
        }

        else if (strstarts(pattern, "DEVICE_TRACK_BANK_FOLLOWS_"))
        {
          m_followflag &= ~8;
          if (!strcmp(pattern, "DEVICE_TRACK_BANK_FOLLOWS_MIXER"))
          {
            m_followflag |= 8;
            MediaTrack* tr=GetMixerScroll();
            if (tr) Extended(CSURF_EXT_SETMIXERSCROLL, tr, 0, (void*)(INT_PTR)1);
          }
        }
        else if (strstarts(pattern, "DEVICE_FX_FOLLOWS_"))
        {
          m_followflag &= ~(2|4);
          int tidx=-1;
          int fxidx=-1;
          if (!strcmp(pattern, "DEVICE_FX_FOLLOWS_LAST_TOUCHED"))
          {
            m_followflag |= 2;
            if (GetLastTouchedFX(&tidx, &fxidx, 0))
            {
              MediaTrack* tr=CSurf_TrackFromID(tidx, false);
              if (tr) Extended(CSURF_EXT_SETLASTTOUCHEDFX, tr, 0, &fxidx);
            }
          }
          else if (!strcmp(pattern, "DEVICE_FX_FOLLOWS_FOCUSED"))
          {
            m_followflag |= 4;
            if (GetFocusedFX(&tidx, 0, &fxidx))
            {
              MediaTrack* tr=CSurf_TrackFromID(tidx, false);
              if (tr) Extended(CSURF_EXT_SETFOCUSEDFX, tr, 0, &fxidx);
            }
          }
        }
      }
      return true;
    }

    if (strends(pattern, "_SELECT"))
    {
      int ival=-1;
      if (flag == 'i')
      {
        const int* i=rmsg->PopIntArg(false);
        if (i) ival=*i;
      }
      else if (numwc > 0 && TriggerMessage(rmsg, flag) > 0)
      {
        ival=wc[0];
      }
      if (ival >= 0)
      {   
        if (strends(pattern, "_TRACK_SELECT"))
        {
          int numtracks=CSurf_NumTracks(!!(m_followflag&8));  
          m_curtrack=m_curbankstart+ival;
          if (m_curtrack < 0) m_curtrack=0;
          else if (m_curtrack > numtracks) m_curtrack=numtracks;
          if (m_followflag&16)
          {
            MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
            if (tr) SetOnlyTrackSelected(tr);
          }
          SetActiveTrackChange();
        }
        else if (strends(pattern, "_TRACK_BANK_SELECT") && m_trackbanksize)
        {
          int numtracks=CSurf_NumTracks(!!(m_followflag&8));
          m_curbankstart=(ival-1)*m_trackbanksize;
          if (m_curbankstart < 0) m_curbankstart=0;
          else if (m_curbankstart >= numtracks) m_curbankstart=max(0,numtracks-1);
          m_curbankstart -= m_curbankstart%m_trackbanksize;
          if (m_followflag&8)
          {
            MediaTrack* tr=CSurf_TrackFromID(m_curbankstart+1, true);
            if (tr) 
            {
              tr=SetMixerScroll(tr);
              Extended(CSURF_EXT_SETMIXERSCROLL, tr, 0, (void*)(INT_PTR)1);
            }
          }
          else
          {
            SetTrackListChange();
          }
        }
        else if (strends(pattern, "_FX_SELECT"))
        {
          int numfx=0;
          MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
          if (tr) numfx=TrackFX_GetCount(tr);
          m_curfx=ival-1;
          if (m_curfx < 0) m_curfx=0;
          else if (m_curfx >= numfx) m_curfx=max(0,numfx);
          SetActiveFXChange();
        }
        else if (strends(pattern, "_FX_PARAM_BANK_SELECT") && m_fxparmbanksize)
        {
          int numparms=0;
          MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
          if (tr) numparms=TrackFX_GetNumParams(tr, m_curfx);
          m_curfxparmbankstart=(ival-1)*m_fxparmbanksize;
          if (m_curfxparmbankstart < 0) m_curfxparmbankstart=0;
          else if (m_curfxparmbankstart >= numparms) m_curfxparmbankstart=max(0,numparms-1);
          m_curfxparmbankstart -= m_curfxparmbankstart%m_fxparmbanksize;
          SetActiveFXChange();
        }
        else if (strends(pattern, "_FX_INST_PARAM_BANK_SELECT") && m_fxinstparmbanksize)
        {
          int numparms=0;
          MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
          int fxidx=-1;
          if (tr) fxidx=TrackFX_GetInstrument(tr);
          if (fxidx >= 0) numparms=TrackFX_GetNumParams(tr, fxidx);
          m_curfxinstparmbankstart=(ival-1)*m_fxinstparmbanksize;
          if (m_curfxinstparmbankstart < 0) m_curfxinstparmbankstart=0;
          else if (m_curfxinstparmbankstart >= numparms) m_curfxinstparmbankstart=max(0,numparms-1);
          m_curfxinstparmbankstart -= m_curfxinstparmbankstart%m_fxinstparmbanksize;
          SetActiveFXInstChange();
        }
        else if (strends(pattern, "_MARKER_BANK_SELECT") && m_markerbanksize)
        {
          int nm=0;
          CountProjectMarkers(NULL, &nm, NULL);
          m_curmarkerbankstart=(ival-1)*m_markerbanksize;
          if (m_curmarkerbankstart < 0) m_curmarkerbankstart=0;
          else if (m_curmarkerbankstart >= nm) m_curmarkerbankstart=max(0, nm-1);
          m_curmarkerbankstart -= m_curmarkerbankstart%m_markerbanksize;
          SetMarkerBankChange();
        }
        else if (strends(pattern, "_REGION_BANK_SELECT") && m_regionbanksize)
        {
          int nr=0;
          CountProjectMarkers(NULL, NULL, &nr);
          m_curregionbankstart=(ival-1)*m_regionbanksize;
          if (m_curregionbankstart < 0) m_curregionbankstart=0;
          else if (m_curregionbankstart >= nr) m_curregionbankstart=max(0, nr-1);
          m_curregionbankstart -= m_curregionbankstart%m_regionbanksize;
          SetMarkerBankChange();
        }
      }
      return true;
    }

    bool prevtr=strends(pattern, "_PREV_TRACK");
    bool nexttr=strends(pattern, "_NEXT_TRACK");
    if (prevtr || nexttr)
    {
      if (TriggerMessage(rmsg, flag) > 0)
      {
        int numtracks=CSurf_NumTracks(!!(m_followflag&8));      
        int dir = (prevtr ? -1 : 1);
        m_curtrack += dir;
        if (m_curtrack < 0) m_curtrack=numtracks;
        else if (m_curtrack > numtracks) m_curtrack=0;
        if (!m_curtrack && numtracks)
        {
          int vis=GetMasterTrackVisibility();
          if (!(vis&1)) m_curtrack=(dir > 0 ? 1 : numtracks);
        }
        if (m_followflag&16)
        {
          MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
          if (tr) SetOnlyTrackSelected(tr);
        }
        SetActiveTrackChange();
      }
      return true;
    }


    bool prevbank=strends(pattern, "_PREV_TRACK_BANK");
    bool nextbank=strends(pattern, "_NEXT_TRACK_BANK");
    if (prevbank || nextbank)
    {
      if (TriggerMessage(rmsg, flag) > 0 && m_trackbanksize)
      {
        int numtracks=CSurf_NumTracks(!!(m_followflag&8));
        if (prevbank) m_curbankstart -= m_trackbanksize;
        else m_curbankstart += m_trackbanksize;
        if (m_curbankstart < 0)
        {
          if (m_followflag&8) m_curbankstart=0;
          else m_curbankstart=max(0,numtracks-1);
        }
        else if (m_curbankstart >= numtracks) 
        {
          if (m_followflag&8) m_curbankstart=max(0,numtracks-1);
          else m_curbankstart=0;
        }
        m_curbankstart -= m_curbankstart%m_trackbanksize;
        if (m_followflag&8)
        {
          MediaTrack* tr=CSurf_TrackFromID(m_curbankstart+1, true);
          if (tr) 
          {
            SetMixerScroll(tr);
            Extended(CSURF_EXT_SETMIXERSCROLL, tr, 0, (void*)(INT_PTR)1);
          }
        }
        else
        {
          SetTrackListChange();
        }
      }
      return true;
    }

    bool prevfx=strends(pattern, "_PREV_FX");
    bool nextfx=strends(pattern, "_NEXT_FX");
    if (prevfx || nextfx)
    {
      if (TriggerMessage(rmsg, flag) > 0)
      {
        int numfx=0;
        MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
        if (tr) numfx=TrackFX_GetCount(tr);
        m_curfx += (prevfx ? -1 : 1);
        if (m_curfx < 0) m_curfx=max(0,numfx-1);
        else if (m_curfx >= numfx) m_curfx=0;
        SetActiveFXChange();
      }
      return true;
    }

    bool prevfxparmbank=strends(pattern, "_PREV_FX_PARAM_BANK");
    bool nextfxparmbank=strends(pattern, "_NEXT_FX_PARAM_BANK");
    if (prevfxparmbank || nextfxparmbank)
    {
      if (TriggerMessage(rmsg, flag) > 0 && m_fxparmbanksize)
      {
        int numparms=0;
        MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
        if (tr) numparms=TrackFX_GetNumParams(tr, m_curfx);
        if (prevfxparmbank) m_curfxparmbankstart -= m_fxparmbanksize;
        else m_curfxparmbankstart += m_fxparmbanksize;     
        if (m_curfxparmbankstart < 0) m_curfxparmbankstart=max(0,numparms-1);
        else if (m_curfxparmbankstart >= numparms) m_curfxparmbankstart=0;
        m_curfxparmbankstart -= m_curfxparmbankstart%m_fxparmbanksize;
        SetActiveFXChange();
      }
      return true;
    }

    bool prevfxinstparmbank=strends(pattern, "_PREV_FX_INST_PARAM_BANK");
    bool nextfxinstparmbank=strends(pattern, "_NEXT_FX_INST_PARAM_BANK");
    if (prevfxinstparmbank || nextfxinstparmbank)
    {
      if (TriggerMessage(rmsg, flag) > 0 && m_fxinstparmbanksize)
      {
        int numparms=0;
        MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
        int fxidx=-1;
        if (tr) fxidx=TrackFX_GetInstrument(tr);
        if (fxidx >= 0) numparms=TrackFX_GetNumParams(tr, fxidx);
        if (prevfxinstparmbank) m_curfxinstparmbankstart -= m_fxinstparmbanksize;
        else m_curfxinstparmbankstart += m_fxinstparmbanksize;     
        if (m_curfxinstparmbankstart < 0) m_curfxinstparmbankstart=max(0,numparms-1);
        else if (m_curfxinstparmbankstart >= numparms) m_curfxinstparmbankstart=0;
        m_curfxinstparmbankstart -= m_curfxinstparmbankstart%m_fxinstparmbanksize;
        SetActiveFXInstChange();
      }
      return true;
    }

    bool prevmarkerbank=strends(pattern, "_PREV_MARKER_BANK");
    bool nextmarkerbank=strends(pattern, "_NEXT_MARKER_BANK");
    if (prevmarkerbank || nextmarkerbank) 
    {
      if (TriggerMessage(rmsg, flag) > 0 && m_markerbanksize)
      {
        int nm=0;
        int nr=0;
        CountProjectMarkers(NULL, &nm, &nr);
        if (prevmarkerbank) m_curmarkerbankstart -= m_markerbanksize;
        else if (nextmarkerbank) m_curmarkerbankstart += m_markerbanksize;
        if (m_curmarkerbankstart < 0) m_curmarkerbankstart=max(0, nm-1);
        else if (m_curmarkerbankstart >= nm) m_curmarkerbankstart=0;
        m_curmarkerbankstart -= m_curmarkerbankstart%m_markerbanksize;
        SetMarkerBankChange();
      }
      return true;
    }

    bool prevregionbank=strends(pattern, "_PREV_REGION_BANK");
    bool nextregionbank=strends(pattern, "_NEXT_REGION_BANK");
    if (prevregionbank || nextregionbank)
    {
      if (TriggerMessage(rmsg, flag) > 0 && m_regionbanksize)
      {
        int nm=0;
        int nr=0;
        CountProjectMarkers(NULL, &nm, &nr);
        if (prevregionbank) m_curregionbankstart -= m_regionbanksize;
        else if (nextregionbank) m_curregionbankstart += m_regionbanksize;
        if (m_curregionbankstart < 0) m_curregionbankstart=max(0, nr-1);
        else if (m_curregionbankstart >= nr) m_curregionbankstart=0;
        m_curregionbankstart -= m_curregionbankstart%m_regionbanksize;      
        SetMarkerBankChange();
      }
      return true;
    }

    return true;
  }

  bool ProcessMarkerRegionAction(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    int idx = -1;
    bool is_region=false;
    if (strstarts(pattern, "LAST_MARKER_") ||
        (is_region=strstarts(pattern, "LAST_REGION_")))
    {
      const int ps=GetPlayState();
      const double pos=((ps&1) ? GetPlayPosition() : GetCursorPosition());

      GetLastMarkerAndCurRegion(NULL, pos, is_region ? NULL : &idx, is_region ? &idx : NULL);
      if (idx<0) return true;
    }
    else if (strstarts(pattern, "MARKER_") ||
             (is_region=strstarts(pattern, "REGION_")))
    {
      if (!numwc) return true;
      idx = wc[0] - 1;
      if (is_region) idx += m_curregionbankstart;
      else idx += m_curmarkerbankstart;

      // convert from OSC-index to combined index
      for (int i = 0;;)
      {
        bool isrgn=false;
        const int nexti=EnumProjectMarkers3(NULL,i, &isrgn, NULL, NULL, NULL, NULL, NULL);
        if (!nexti) return true; // ran out of search items

        if (isrgn == is_region && !idx--)
        {
          idx = i;
          break;
        }
        i = nexti;
      }
    }
    else if (strstarts(pattern, "MARKERID_") ||
             (is_region=strstarts(pattern, "REGIONID_")))
    {
      if (!numwc || wc[0] < 0) return true;

      for (int pass = 0; ; pass ++)
      {
        int tidx = 0;
        bool isrgn = false, found = false;
        idx=0;
        while (EnumProjectMarkers3(NULL,idx, &isrgn, NULL, NULL, NULL, &tidx, NULL))
        {
          if (isrgn == is_region && tidx == wc[0]) { found=true; break; }
          idx++;
        }
        if (found) break;

        if (pass || AddProjectMarker2(NULL,is_region,0.0, 1.0, "", wc[0], 0)<0) return true;
      }
    }
    else
      return false;

    // idx is in combined marker/region coordinates
    bool isrgn=false;
    double pos=0.0,endpos=0.0;
    int ID=0,color=0;
    bool hasarg=false;

    if (strends(pattern, "_NAME"))
    {
      const char *p = rmsg->PopStringArg(false);
      if (p && EnumProjectMarkers3(NULL,idx,&isrgn,&pos,&endpos,NULL,&ID,&color) && isrgn == is_region)
      {
        SetProjectMarkerByIndex2(NULL,idx,is_region,pos,endpos,ID,p,color,!p[0] ? 1 : 0);
        Undo_OnStateChangeEx(
            isrgn ? "Set region name via OSC" :
            "Set marker name via OSC",
            UNDO_STATE_MISCCFG, 0xdefe7000 | 200);
      }
    }
    else if (strends(pattern, "_NUMBER"))
    {
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg && EnumProjectMarkers3(NULL,idx,&isrgn,&pos,&endpos,NULL,&ID,&color) && isrgn == is_region)
      {
        ID = (int) floor(v+0.5);
        SetProjectMarkerByIndex2(NULL,idx,is_region,pos,endpos,ID,NULL,color,0);
        Undo_OnStateChangeEx(
            isrgn ? "Set region number via OSC" :
            "Set marker number via OSC",
            UNDO_STATE_MISCCFG, 0xdefe7000 | 200);
      }
    }
    else if (strends(pattern, "_TIME"))
    {
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg && EnumProjectMarkers3(NULL,idx,&isrgn,&pos,&endpos,NULL,&ID,&color) && isrgn == is_region)
      {
        if (isrgn) endpos -= pos;
        pos = v;
        if (isrgn) endpos += pos;
        SetProjectMarkerByIndex2(NULL,idx,is_region,pos,endpos,ID,NULL,color,0);
        Undo_OnStateChangeEx(
            isrgn ? "Set region start via OSC" :
            "Set marker position via OSC",
            UNDO_STATE_MISCCFG, 0xdefe7000 | 200);
      }
    }
    else if (is_region && strends(pattern, "_LENGTH"))
    {
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg && EnumProjectMarkers3(NULL,idx,&isrgn,&pos,&endpos,NULL,&ID,&color) && isrgn == is_region)
      {
        endpos = pos+v;
        SetProjectMarkerByIndex2(NULL,idx,is_region,pos,endpos,ID,NULL,color,0);
        Undo_OnStateChangeEx( "Set region length via OSC", UNDO_STATE_MISCCFG, 0xdefe7000 | 200);
      }
    }

    return true;
  }

  bool ProcessTrackAction(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    if (!strstarts(pattern, "TRACK_") && !strstarts(pattern, "MASTER_"))
    {
      return false;
    }
    if (strstr(pattern, "_SEND_") || strstr(pattern, "_RECV_")) 
    {
      return false;
    }

    int tidx=m_curtrack;
    if (strstarts(pattern, "MASTER_")) 
    {
      tidx=0;
    }
    else if (numwc) 
    {
      tidx=m_curbankstart+wc[0];
    }
    MediaTrack* tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
    if (!tr) return true;  // badly formed pattern

    if (strends(pattern, "_NAME"))
    {
      const char *p = rmsg->PopStringArg(false);
      if (p)
      {
        GetSetMediaTrackInfo((INT_PTR)tr,"P_NAME",(void*)p);
        Undo_OnStateChangeEx("Set track name via OSC",UNDO_STATE_TRACKCFG, 0xdefe7000 | 200); // undocumented defer-undo syntax
      }
      return true;
    }
    
    if (strends(pattern, "_VOLUME"))
    {
      bool hasarg=false;
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg)
      {
        if (strchr("nbtr", flag)) v=SLIDER2DB(v*1000);
        v=DB2VAL(v);
        CSurf_OnVolumeChangeEx(tr, v, false, true); 
        SetSurfaceVolume(tr, v);

        if (!(m_supports_touch&1) && tidx >= 0 && tidx < MAX_LASTTOUCHED_TRACK)
        {
          m_vol_lasttouch[tidx]=GetTickCount();
        }
      }
      return true;
    }

    bool pan=strends(pattern, "_PAN");
    bool pan2=strends(pattern, "_PAN2");
    if (pan || pan2)
    {
      bool hasarg=false;
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg)
      {
        if (strchr("nbtr", flag)) v=v*2.0-1.0;
        if (pan) CSurf_OnPanChangeEx(tr, v, false, true);
        else if (pan2) CSurf_OnWidthChangeEx(tr, v, false, true);
        int panmode=PAN_MODE_NEW_BALANCE;
        double tpan[2] = { 0.0, 0.0 };
        GetTrackUIPan(tr, &tpan[0], &tpan[1], &panmode);
        Extended(CSURF_EXT_SETPAN_EX_IMPL, &tidx, tpan, &panmode);

        if (!(m_supports_touch&(pan2?4:2)) && tidx >= 0 && tidx < MAX_LASTTOUCHED_TRACK)
        {
          (pan2 ? m_pan2_lasttouch : m_pan_lasttouch)[tidx]=GetTickCount();
        }
      }
      return true;
    }

    if (strends(pattern, "_SELECT"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        bool sel=IsTrackSelected(tr);
        if (flag == 't' || sel != (t > 0))
        {  
          CSurf_OnSelectedChange(tr, !sel);
          SetSurfaceSelected(tr, !sel);
        }
      }
      return true;
    }

    bool mute=strends(pattern, "_MUTE");
    bool solo=strends(pattern, "_SOLO");
    bool recarm=strends(pattern, "_REC_ARM");
    bool monitor=strends(pattern, "_MONITOR");
    if (mute || solo || recarm || monitor)
    {
      if (!tidx && !mute && !solo) return true;

      int t=0;
      bool is_direct_parm=false;
      if (monitor && flag != 't')
      {
        double v=GetFloatArg(rmsg, flag, &is_direct_parm);
        if (is_direct_parm) t = (int)v;
      }
      if (!is_direct_parm)
        t=TriggerMessage(rmsg, flag);

      if (t || is_direct_parm)
      {
        int flags=0;
        GetTrackInfo((INT_PTR)tr, &flags);
     
        int a=-1;
        if (mute && (flag == 't' || !!(flags&8) != (t > 0)))
        {
          a=!(flags&8);
        }
        else if (solo && (flag == 't' || !!(flags&16) != (t > 0)))
        {
          a=!(flags&16);
        }
        else if (recarm && (flag == 't' || !!(flags&64) != (t > 0)))
        {
          a=!(flags&64);
        }
        else if (monitor)
        {
          a=0;
          if (flags&128) a=1;
          else if (flags&256) a=2;

          if (is_direct_parm)
          {
            if (a == t || t < 0 || t > 2) a = -1;
            else a=t;
          }
          else if (flag == 't')
          {
            if (++a > 2) a=0;
          }
          else
          {
            a=-1;
          }
        }

        if (a >= 0)
        {
          if (mute) CSurf_OnMuteChangeEx(tr, a, true);                  
          else if (solo) CSurf_OnSoloChangeEx(tr, a, true);
          else if (recarm) CSurf_OnRecArmChangeEx(tr, a, true); // triggers UpdateAllExternalSurfaces
          else if (monitor) CSurf_OnInputMonitorChangeEx(tr, a, true);      

          bool sel=IsTrackSelected(tr);
          int n=CSurf_NumTracks(!!(m_followflag&8));
          int i;
          for (i=0; i <= n; ++i)
          {
            MediaTrack* ttr=CSurf_TrackFromID(i, !!(m_followflag&8));
            if (tr == ttr || (sel && IsTrackSelected(ttr)))
            {
              if (mute) SetSurfaceMute(ttr, !!a);
              else if (solo) SetSurfaceSolo(ttr, !!a);
              else if (recarm) SetSurfaceRecArm(ttr, !!a);
              else if (monitor) Extended(CSURF_EXT_SETINPUTMONITOR, ttr, &a, 0);
            }
          }
        }
      }
      return true;
    }

    int automode=-1;
    if (!strcmp(pattern, "TRACK_AUTO_TRIM")) automode=AUTO_MODE_TRIM;
    else if (!strcmp(pattern, "TRACK_AUTO_READ")) automode=AUTO_MODE_READ;
    else if (!strcmp(pattern, "TRACK_AUTO_LATCH")) automode=AUTO_MODE_LATCH;
    else if (!strcmp(pattern, "TRACK_AUTO_TOUCH")) automode=AUTO_MODE_TOUCH;
    else if (!strcmp(pattern, "TRACK_AUTO_WRITE")) automode=AUTO_MODE_WRITE;
    if (automode >= 0)
    {
      int t=TriggerMessage(rmsg, flag);
      if (t)
      {
        if (t < 0) automode=AUTO_MODE_TRIM;   
        
        bool sel=IsTrackSelected(tr);

        int i;
        int n=CSurf_NumTracks(!!(m_followflag&8));
        for (i=-1; i < n; ++i)
        {         
          if (i >= 0 && !sel) break;
          if (i == tidx) continue;
          MediaTrack* ttr = (i < 0 ? tr : CSurf_TrackFromID(i, !!(m_followflag&8)));
          if (!ttr || (i >= 0 && !IsTrackSelected(ttr))) continue;
          
          SetTrackAutomationMode(ttr, automode);
          SetSurfaceAutoMode(ttr, automode);
        }
      }
      return true;
    }

    bool voltouch=!strcmp(pattern, "TRACK_VOLUME_TOUCH");
    bool pantouch=!strcmp(pattern, "TRACK_PAN_TOUCH");
    bool pan2touch=!strcmp(pattern, "TRACK_PAN2_TOUCH");
    if (voltouch || pantouch || pan2touch)
    {
      int t=TriggerMessage(rmsg, flag);
      if (t && tidx >= 0 && tidx < MAX_LASTTOUCHED_TRACK)
      {
        char mask = (voltouch ? 1 : pan2touch ? 4 : 2);      
        if (t > 0) m_hastouch[tidx] |= mask;
        else m_hastouch[tidx] &= ~mask;
      }
      return true;
    }
  
    return true;
  }

  bool ProcessFXAction(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    if (!strstarts(pattern, "FX_") && !strstarts(pattern, "LAST_TOUCHED_FX_"))
    {
      return false;
    }

    MediaTrack* tr=0;
    int tidx=m_curtrack;
    int fxidx=m_curfx;

    // continuous actions

    bool fxparm=!strcmp(pattern, "FX_PARAM_VALUE");
    bool fxwet=!strcmp(pattern, "FX_WETDRY");
    bool fxeqwet=!strcmp(pattern, "FX_EQ_WETDRY");
    bool instparm=!strcmp(pattern, "FX_INST_PARAM_VALUE");
    bool lasttouchedparm=!strcmp(pattern, "LAST_TOUCHED_FX_PARAM_VALUE");
    if (fxparm || fxwet || fxeqwet || instparm || lasttouchedparm)
    {      
      const float* fArg = rmsg->PopFloatArg(false);
      const int *intArg = rmsg->PopIntArg(false);
      if (fArg || intArg)
      {
        int parmidx=-1;

        if (fxparm)
        {
          if (!numwc) return true; // need at least parmidx
          parmidx=m_curfxparmbankstart+wc[0]-1;
          if (numwc > 1) fxidx=wc[1]-1;          
          if (numwc > 2) tidx=m_curbankstart+wc[2];
          tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
        }
        else if (fxwet)
        {
          if (numwc) fxidx=wc[0]-1;
          if (numwc > 1) tidx=m_curbankstart+wc[1];
          tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
          parmidx=TrackFX_GetParamFromIdent(tr, fxidx, ":wet");
        }
        else if (fxeqwet)
        {
          if (numwc) tidx=m_curbankstart+wc[0];
          tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
          if (tr) fxidx=TrackFX_GetEQ(tr, !!(m_followflag&32));
          parmidx=TrackFX_GetParamFromIdent(tr, fxidx, ":wet");
        }
        else if (instparm)
        {
          if (!numwc) return true; // need at least parmidx
          parmidx=m_curfxinstparmbankstart+wc[0]-1;
          if (numwc > 1) tidx=m_curbankstart+wc[1];
          tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));         
          if (tr) fxidx=TrackFX_GetInstrument(tr);          
        }
        else if (lasttouchedparm)
        {
          if (!GetLastTouchedFX(&tidx, &fxidx, &parmidx)) return true;
          tr=CSurf_TrackFromID(tidx, false);
        }

        if (tr && fxidx >= 0 && parmidx >= 0)
        {
          if (fArg)
            TrackFX_SetParamNormalized(tr, fxidx, parmidx, *fArg);
          if (intArg && *intArg==-1)
            TrackFX_EndParamEdit(tr,fxidx,parmidx);
        }
      }
      return true;          
    }

    int bandidx=0;
    bool isinst=strstarts(pattern, "FX_INST_");
    bool iseq=strstarts(pattern, "FX_EQ_");
    if (isinst || iseq)
    {
      if (iseq && strstr(pattern, "_BAND_"))
      {
        if (numwc) bandidx=wc[0];
        if (numwc > 0) tidx=m_curbankstart+wc[1];
      }
      else if (numwc) 
      {
        tidx=m_curbankstart+wc[0];
      }
      tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
      if (!tr) return true;
      if (isinst) fxidx=TrackFX_GetInstrument(tr);
      else if (iseq) fxidx=TrackFX_GetEQ(tr, !!(m_followflag&32));
      if (fxidx < 0) return true;    

      if (strends(pattern, "_BYPASS")) pattern="FX_BYPASS";
      else if (strends(pattern, "_OPEN_UI")) pattern="FX_OPEN_UI";
      else if (strends(pattern, "_PREV_PRESET")) pattern="FX_PREV_PRESET";
      else if (strends(pattern, "_NEXT_PRESET")) pattern="FX_NEXT_PRESET";
      else if (strends(pattern, "_PRESET")) pattern="FX_PRESET";
    }
    else
    {
      if (numwc) fxidx=wc[0]-1;
      if (numwc > 1) tidx=m_curbankstart+wc[1];
      tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
    }

    if (!strcmp(pattern, "FX_BYPASS"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (tr && t)
      {
        bool en=TrackFX_GetEnabled(tr, fxidx);
        if (flag == 't' || en != (t > 0))
        {
          TrackFX_SetEnabled(tr, fxidx, !en);
        }
      }
      return true;
    }

    if (!strcmp(pattern, "FX_OPEN_UI"))
    {
      int t=TriggerMessage(rmsg, flag);
      if (tr && t)
      {
        bool open=TrackFX_GetOpen(tr, fxidx);
        if (flag == 't' || open != (t > 0))
        {
          TrackFX_SetOpen(tr, fxidx, !open);
        }
      }
      return true;
    }

    bool prevpreset=!strcmp(pattern, "FX_PREV_PRESET");
    bool nextpreset=!strcmp(pattern, "FX_NEXT_PRESET");
    if (prevpreset || nextpreset)
    {
      if (TriggerMessage(rmsg, flag) > 0 && tr)
      {
        int dir = (prevpreset ? -1 : 1);
        TrackFX_NavigatePresets(tr, fxidx, dir);
        if (isinst) SetActiveFXInstChange();
        else SetActiveFXChange();
      }
      return true;
    }

    if (!strcmp(pattern, "FX_PRESET"))
    {
      const char* s=rmsg->PopStringArg(false);
      if (tr && s && s[0])
      {
        TrackFX_SetPreset(tr, fxidx, s);
      }     
      return true;
    }

    if (strstarts(pattern, "FX_EQ_"))
    {
      // we know tr, fxidx, bandidx are valid

      int bandtype=-2;
      int parmtype=-2;       

      if (strends(pattern, "_MASTER_GAIN"))
      {
        bandtype=parmtype=-1;
      }
      else
      {
        // bandtype: -1=master gain, 0=hipass, 1=loshelf, 2=band, 3=notch, 4=hishelf, 5=lopass
        if (strstr(pattern, "_HIPASS_")) bandtype=0;
        else if (strstr(pattern, "_LOSHELF_")) bandtype=1;
        else if (strstr(pattern, "_BAND_")) bandtype=2;
        else if (strstr(pattern, "_NOTCH_")) bandtype=3;
        else if (strstr(pattern, "_HISHELF_")) bandtype=4;
        else if (strstr(pattern, "_LOPASS_")) bandtype=5;
      
        if (strends(pattern, "_FREQ")) parmtype=0;
        else if (strends(pattern, "_GAIN")) parmtype=1;
        else if (strends(pattern, "_Q")) parmtype=2;
        else if (strends(pattern, "_BYPASS")) parmtype=-1;
      }

      if (bandtype >= -1 && parmtype >= -1)
      {       
        if (bandtype >= 0 && parmtype == -1)
        {
          int t=TriggerMessage(rmsg, flag);
          if (t)
          {
            bool en=TrackFX_GetEQBandEnabled(tr, fxidx, bandtype, bandidx);
            if (flag == 't' || en != (t > 0))
            {
              TrackFX_SetEQBandEnabled(tr, fxidx, bandtype, bandidx, !en);
            }
          }
        }
        else
        {
          bool hasarg=false;
          double v=GetFloatArg(rmsg, flag, &hasarg);
          if (hasarg)
          {
            bool isnorm=!strchr("fi", flag);
            TrackFX_SetEQParam(tr, fxidx, bandtype, bandidx, parmtype, v, isnorm);
          }
        }
      }
      return true;
    }

    return true;
  }

  bool ProcessSendAction(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    if (!strstr(pattern, "_SEND_") && !strstr(pattern, "_RECV_")) return false;

    if (!numwc) return true;  // badly formed pattern
    int sidx=wc[0];
    if (sidx) --sidx;

    int tidx=m_curtrack;
    if (strstarts(pattern, "MASTER_") && strstr(pattern, "_SEND_"))
    {
      tidx=0;
    }
    else if (numwc > 1) 
    {
      tidx=m_curbankstart+wc[1];
    }
    MediaTrack* tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
    if (!tr) return true; // badly formed pattern

    bool sendvol=strends(pattern, "_SEND_VOLUME");
    bool recvvol=strends(pattern, "_RECV_VOLUME");
    if (sendvol || recvvol)
    {
      bool hasarg=false;
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg)
      {
        if (strchr("nbtr", flag)) v=SLIDER2DB(v*1000.0);
        v=DB2VAL(v);
        if (sendvol) 
        {
          CSurf_OnSendVolumeChange(tr, sidx, v, false);
          Extended(CSURF_EXT_SETSENDVOLUME, tr, &sidx, &v);
        }
        else if (recvvol)
        {
          CSurf_OnRecvVolumeChange(tr, sidx, v, false);
          Extended(CSURF_EXT_SETRECVVOLUME, tr, &sidx, &v);
        }      
      }
      return true;
    }

    bool sendpan=strends(pattern, "_SEND_PAN");
    bool recvpan=strends(pattern, "_RECV_PAN");
    if (sendpan || recvpan)
    {
      bool hasarg=false;
      double v=GetFloatArg(rmsg, flag, &hasarg);
      if (hasarg)
      {
        if (strchr("nbtr", flag)) v=v*2.0-1.0;
        if (sendpan) 
        {
          CSurf_OnSendPanChange(tr, sidx, v, false);
          Extended(CSURF_EXT_SETSENDPAN, tr, &sidx, &v);
        }
        else
        {
          CSurf_OnRecvPanChange(tr, sidx, v, false);
          Extended(CSURF_EXT_SETRECVPAN, tr, &sidx, &v);
        }
      }
      return true;
    }

    return true;
  }

  static void DoOSCAction(const char* pattern, const int* cmd, const char* str, const float* val, bool isRelative)
  {
    int cmdid=0;
    if (cmd) cmdid=*cmd;
    else if (str && str[0]) cmdid=NamedCommandLookup(str);
    if (!cmdid) return;

    if (!strcmp(pattern, "ACTION"))
    {
      if (val) 
      {
        if (isRelative)
        {
          int val7bit, valhw;
          encode_relmode1_extended(*val, &val7bit,&valhw);
          KBD_OnMainActionEx(cmdid, val7bit, valhw, 1, GetMainHwnd(), 0);
        }
        else
        {
          int ival=(int)(*val*16383.0);   
          KBD_OnMainActionEx(cmdid, (ival>>7)&0x7F, ival&0x7F, 0, GetMainHwnd(), 0);
        }
      }
      else
      {
        Main_OnCommand(cmdid, 0);
      }
    }
    else if (!strcmp(pattern, "MIDIACTION"))
    {
      if (cmd) MIDIEditor_LastFocused_OnCommand(cmdid, false);
    }
    else if (!strcmp(pattern, "MIDILISTACTION"))
    {
      MIDIEditor_LastFocused_OnCommand(cmdid, true);
    }
  }

  bool ProcessAction(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    if (strstr(pattern, "ACTION"))
    {
      const bool isRelative = !strcmp(pattern, "ACTION_RELATIVE");
      const bool isSoftTakeover = !strcmp(pattern, "ACTION_SOFT");

      if (isSoftTakeover)
      {
         kbd_pushPopSoftTakeover("OSC",rmsg->GetMessage());
      }
      
      if (isRelative || isSoftTakeover)
      {
        pattern = "ACTION";
      }

      if (numwc && (TriggerMessage(rmsg, flag) > 0 || flag == 'f'))
      {
        const float* f=NULL;
        if (flag == 'f') f=rmsg->PopFloatArg(false);
        DoOSCAction(pattern, &wc[0], 0, f,isRelative);
      }
      else
      {
        const int* i=rmsg->PopIntArg(false);        
        const char* s=rmsg->PopStringArg(false);
        while ((i && *i > 0) || (s && s[0]))
        {
          const float* f=rmsg->PopFloatArg(false);
          DoOSCAction(pattern, i, s, f,isRelative);
          i=rmsg->PopIntArg(false);
          s=rmsg->PopStringArg(false);
        }
      }

      if (isSoftTakeover)
      {
        kbd_pushPopSoftTakeover(NULL,NULL);
      }

      return true;
    }

    bool goto_marker=!strcmp(pattern, "GOTO_MARKER");
    bool goto_region=!strcmp(pattern, "GOTO_REGION");
    if (goto_marker || goto_region)
    {
      int idx=-1;
      if (numwc && TriggerMessage(rmsg, flag) > 0) 
      {
        idx=wc[0];
      }
      else
      {
        const int* i=rmsg->PopIntArg(false);
        const float* f=rmsg->PopFloatArg(false);
        if (i) idx=*i;        
        else if (f) idx=(int)*f;
      }
      if (idx >= 0)
      {
        if (goto_marker) GoToMarker(NULL, idx+m_curmarkerbankstart, true);
        else if (goto_region) GoToRegion(NULL, idx+m_curregionbankstart, true);
      }
      return true;
    }

    return false;
  }
  bool ProcessVKBMIDI(OscMessageRead* rmsg, const char* pattern, char flag, int* wc, int numwc)
  {
    if (strncmp(pattern, "VKB_MIDI_",9)) return false;

    pattern += 9;

    const bool isCC=!strcmp(pattern,"CC");
    const bool isCP=!strcmp(pattern,"CHANNELPRESSURE");

    const bool isPA=!strcmp(pattern,"POLYAFTERTOUCH");
    const bool isPC=!strcmp(pattern,"PROGRAM");
    const bool isPitch = !strcmp(pattern,"PITCH");

    if (isPitch || isPC || isCP || isCC || isPA || !strcmp(pattern,"NOTE"))
    {
      int value = 0;
      const float* f=rmsg->PopFloatArg(false);
      const int *i = rmsg->PopIntArg(false);
      if (i) value = *i;
      else if (f) value = (int)*f;

      if (value<0)value=0;        

      int psz=0;
      MIDI_event_t evt={0,3,};
      if (isPitch)
      {
        if (value > 16383) value=16383;
        evt.midi_message[0]=0xE0;
        evt.midi_message[1] = value&127;
        evt.midi_message[2] = value>>7;
      }
      else 
      {
        if (value > 127) value=127;
        if (isPC || isCP)
        {
          evt.midi_message[0] = isPC ? 0xC0 : 0xD0;
          evt.midi_message[1] = value;
        }
        else
        {
          if (numwc<1) return false; // CC/PA/NOTE all require a wildcard to specify the note parameter

          evt.midi_message[0] = isCC ? 0xB0 : isPA ? 0xA0 : (value ? 0x90 : 0x80);
          evt.midi_message[1] = wc[0];
          evt.midi_message[2] = value;
          psz=1;
        }
      }

      if (psz < numwc && wc[psz] >= 0)
      {
        evt.midi_message[0] += min(max(wc[psz],0),15);
        VkbStuffMessage(&evt,false);
      }
      else
      {
        VkbStuffMessage(&evt,true);
      }
      return true;
    }
    return false;
  }
  

  bool ProcessMessage(OscMessageRead* rmsg)
  { 
    char flag='n';
    int wc[MAX_OSC_WC*MAX_OSC_RPTCNT]; 
    int numwc=0;
    int rptcnt=1;
    const char* msg=rmsg->GetMessage();
    const char* pattern=FindOscMatch(msg, wc, &numwc, &rptcnt, &flag);
    if (!pattern) return false;

    OscVal *lastval=m_lastvals.GetPtr(msg);
    if (lastval) lastval->Clear();

    m_curedit=msg;
    m_curflag=flag;
  
    bool ok=false;
    if (rptcnt < 1) rptcnt=1;

    numwc /= rptcnt;
    int i;
    for (i=rptcnt-1; i >= 0; --i)
    {
      int* twc=wc+i*numwc;
      if (!numwc && ProcessGlobalAction(rmsg, pattern, flag)) ok=true;
      else if (ProcessStateAction(rmsg, pattern, flag, twc, numwc)) ok=true;
      else if (ProcessTrackAction(rmsg, pattern, flag, twc, numwc)) ok=true;
      else if (ProcessMarkerRegionAction(rmsg, pattern, flag, twc, numwc)) ok=true;
      else if (ProcessFXAction(rmsg, pattern, flag, twc, numwc)) ok=true;
      else if (ProcessSendAction(rmsg, pattern, flag, twc, numwc)) ok=true;
      else if (ProcessAction(rmsg, pattern, flag, twc, numwc)) ok=true;
      else if (ProcessVKBMIDI(rmsg,pattern,flag,twc,numwc)) ok=true;
    }

    m_curedit=0;
    m_curflag=0;

    return ok;
  }

  // core implementation for sending data to the csurf
  // nval=normalized
  void SetSurfaceVal(const char* pattern, const int* wcval, int numwcval,
    const int* ival, const double* fval, const double* nval, const char* sval)
  {
    if (osc_send_inactive()) return;
    if (!pattern) return;

    int* keyidx=m_msgkeyidx.GetPtr(pattern);
    if (!keyidx) return;  // code error, asking to send an undefined key string

    int i;
    for (i=*keyidx+1; i < m_msgtab.GetSize(); ++i)
    {
      const char* p=m_msgtab.Get(i);
      if (!p) break;

      char flag=*p;
      ++p;

#ifdef _DEBUG
      assert(*p == '/');
#endif

      const int* tival=0;
      const double* tfval=0;
      const char* tsval=0;
      if (flag == 'i') tival=ival;
      else if (flag == 'f') tfval=fval;
      else if (strchr("nbt", flag)) tfval=nval;
      else if (flag == 's') tsval=sval;
      else continue; // don't send 'r' messages to the device
    
      char msg[2048];
      {
        bool rev = false;
        const char *rp = p;
        char *wp = msg;
        int j = 0;

        const int needed_wc = CountWildcards(p);
        if (needed_wc != numwcval) continue; // must have exact match of wildcard counts

        while (*rp && wp < msg + sizeof(msg) - 33)
        {
          const char c = *rp++;
          if (c == '@')
          {
            if (!j && *rp != '@') rev = true;

            if (j < numwcval)
            {
              const int k = (rev ? needed_wc - j - 1 : j);
              snprintf(wp, 30, "%d", wcval[k]);
              while (*wp) wp++;
            }
            j++;
          }
          else
          {
            *wp++ = c;
          }
        }
        *wp = 0;
      }

      if (m_curedit && !strchr("tb", flag) && !strcmp(m_curedit, msg) && m_curflag == flag) 
      {
        continue; // antifeedback
      }

      {
        OscVal *lastval=m_lastvals.GetPtr(msg);
        OscVal f;
        if (!lastval) lastval = &f;

        bool need_update = false;
        if (tival) need_update = lastval->UpdateInt(*tival);
        else if (tfval) need_update = lastval->UpdateFloat((float) *tfval);
        else if (tsval == SPECIAL_STRING_CLEARCACHE) 
        {
          if (lastval != &f) m_lastvals.Delete(msg);
          continue;
        }
        else if (tsval) need_update = lastval->UpdateString(tsval);
        else continue; // unknown type
      
        if (lastval == &f) m_lastvals.Insert(msg, f);
        else if (!need_update) continue;
      }

      OscMessageWrite wmsg;
      wmsg.PushWord(msg);

      if (tival) wmsg.PushIntArg(*tival);
      else if (tfval) wmsg.PushFloatArg((float)*tfval);
      else if (tsval) wmsg.PushStringArg(tsval);

      if (m_osc) OscSendOutput(m_osc, &wmsg);

      if (m_osc_local && m_osc_local->m_callback) 
      {
        int len=0;
        const char* p=wmsg.GetBuffer(&len);
        if (p && len)
        {
          m_osc_local->m_callback(m_osc_local->m_obj, p, len);
        }
      }
    }
  }

  // validate the track, if it could be visible in the csurf, send the message
  void SetSurfaceTrackVal(const char* pattern, int tidx,
    int* ival, double* fval, double* nval, const char* sval)
  {
    if (tidx == m_curtrack)
    {
      SetSurfaceVal(pattern, 0, 0, ival, fval, nval, sval);
    }
    if (tidx > m_curbankstart && tidx <= m_curbankstart+m_trackbanksize)
    {
      int wcval[1] = { tidx-m_curbankstart };
      SetSurfaceVal(pattern, wcval, 1, ival, fval, nval, sval);
    }
  }


  static void GetFXName(MediaTrack* tr, int fxidx, char* buf, int buflen)
  {
    buf[0]=0;
    if (TrackFX_GetFXName(tr, fxidx, buf, buflen) && buf[0])
    {
      const char* prefix[] = { "VST: ", "VSTi: ", "VST3: ", "VST3i: ", "JS: ", "DX: ", "DXi: ", "AU: ", "AUi: " };
      int i;
      for (i=0; i < sizeof(prefix)/sizeof(prefix[0]); ++i)
      {
        if (strstarts(buf, prefix[i]))
        {
          memmove(buf, buf+strlen(prefix[i]), strlen(buf)-strlen(prefix[i])+1);
          return;
        }
      }
    }  
  }

  void UpdateSurfaceTrack(int tidx)
  {
    if (osc_send_inactive()) return;

    MediaTrack* tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
   
    char buf[512];
    char buf2[512];
    char ibuf[128];

    buf[0]=0;           
    sprintf(ibuf, "%d", tidx);
    const char* p=0;
    if (tr) p=GetTrackInfo((INT_PTR)tr, 0);
    if (p) lstrcpyn(buf, p, sizeof(buf));              
    if (!buf[0] || !strcmp(buf, ibuf))
    {
      if (!tidx) strcpy(buf, "MASTER");
      else sprintf(buf, "Track %d", tidx);
    }   
    SetSurfaceTrackVal("TRACK_NAME", tidx, 0, 0, 0, buf);
    SetSurfaceTrackVal("TRACK_NUMBER", tidx, 0, 0, 0, ibuf);

    bool sel=false;
    if (tr) sel=IsTrackSelected(tr);
    SetSurfaceSelectedImpl(tidx, sel);
    
    double vol=0.0;
    if (tr) GetTrackUIVolPan(tr, &vol, 0);
    SetSurfaceVolumeImpl(tidx, vol);

    double pan[2] = { 0.0, 0.0 };
    int panmode=PAN_MODE_NEW_BALANCE;
    if (tr) GetTrackUIPan(tr, &pan[0], &pan[1], &panmode);
    Extended(CSURF_EXT_SETPAN_EX_IMPL, &tidx, pan, &panmode);

    int flags=0;
    if (tr) GetTrackInfo((INT_PTR)tr, &flags);
    SetSurfaceMuteImpl(tidx, !!(flags&8));
    SetSurfaceSoloImpl(tidx, !!(flags&16));
    if (tidx)
    {
      SetSurfaceRecArmImpl(tidx, !!(flags&64));
      int a=0;
      if (flags&128) a=1;
      else if (flags&256) a=2;
      Extended(CSURF_EXT_SETINPUTMONITOR_IMPL, &tidx, &a, 0);
    }

    int mode=0;
    if (tr) mode=GetTrackAutomationMode(tr);
    SetSurfaceAutoModeImpl(tidx, mode);

    bool iscur = (tidx == m_curtrack);
    bool isbank = (tidx > m_curbankstart && tidx <= m_curbankstart+m_trackbanksize);

    int i;
    for (i=0; i < m_sendbanksize; ++i)
    {
      buf[0]=0;
      if (tr) GetTrackSendName(tr, i, buf, sizeof(buf));
      if (!buf[0]) sprintf(buf, (!tidx ? "Aux %d" : "Send %d"), i+1);
      int wc[2] = { i+1, tidx-m_curbankstart };     
      if (!tidx) SETSURFSTRWC("MASTER_SEND_NAME", wc, 1, buf);
      if (iscur) SETSURFSTRWC("TRACK_SEND_NAME", wc, 1, buf);
      if (isbank) SETSURFSTRWC("TRACK_SEND_NAME", wc, 2, buf);
    
      double vol=0.0;
      double pan=0.0;
      if (tr) GetTrackSendUIVolPan(tr, i, &vol, &pan);
      Extended(CSURF_EXT_SETSENDVOLUME_IMPL, &tidx, &i, &vol);
      Extended(CSURF_EXT_SETSENDPAN_IMPL, &tidx, &i, &pan);
    }

    if (tidx)
    {
      for (i=0; i < m_recvbanksize; ++i)
      {
        buf[0]=0;
        if (tr) GetTrackReceiveName(tr, i, buf, sizeof(buf));
        if (!buf[0]) sprintf(buf, "Recv %d", i+1);
        int wc[2] = { i+1, tidx-m_curbankstart };     
        if (iscur) SETSURFSTRWC("TRACK_RECV_NAME", wc, 1, buf);
        if (isbank) SETSURFSTRWC("TRACK_RECV_NAME", wc, 2, buf);

        double vol=0.0;
        double pan=0.0;
        if (tr) GetTrackReceiveUIVolPan(tr, i, &vol, &pan);
        Extended(CSURF_EXT_SETRECVVOLUME_IMPL, &tidx, &i, &vol);
        Extended(CSURF_EXT_SETRECVPAN_IMPL, &tidx, &i, &pan);
      }
    }
 
    for (i=0; i < m_fxbanksize; ++i)
    {
      int numfx=0, numparms=0;
      bool en=false;
      bool open=false;
      buf[0]=buf2[0]=ibuf[0]=0;
      if (tr) 
      {
        numfx=TrackFX_GetCount(tr);
        if (i < numfx)
        {      
          en=TrackFX_GetEnabled(tr, i);
          open=TrackFX_GetOpen(tr, i);
          GetFXName(tr, i, buf, sizeof(buf));
          sprintf(ibuf, "%d", i+1);
          TrackFX_GetPreset(tr, i, buf2, sizeof(buf2));
          numparms=TrackFX_GetNumParams(tr, i);
        }
      }

      bool iscurfx = (iscur && i == m_curfx);
      {
        int wc[2] = { i+1, tidx-m_curbankstart };
        int k;
        for (k=0; k <= 2; ++k)
        {
          if ((!k && iscurfx) || (k == 1 && iscur) || (k == 2 && isbank))
          {
            SETSURFSTRWC("FX_NAME", wc, k, buf);
            SETSURFSTRWC("FX_NUMBER", wc, k, ibuf); 
            SETSURFSTRWC("FX_PRESET", wc, k, buf2);
            SETSURFBOOLWC("FX_BYPASS", wc, k, en);
            SETSURFBOOLWC("FX_OPEN_UI", wc, k, open);

            if (m_wantfx&1)
              SendUpdateFXParms(tr,i,numfx,numparms, wc, k);
          }
        }
      }

      int fxidx=-1;
      if (tr) fxidx=TrackFX_GetInstrument(tr);

      en=false;
      open=false;
      buf[0]=buf2[0]=0;
      if (tr && fxidx >= 0)
      {
        en=TrackFX_GetEnabled(tr, fxidx);
        open=TrackFX_GetOpen(tr, fxidx);
        GetFXName(tr, fxidx, buf, sizeof(buf));
        TrackFX_GetPreset(tr, fxidx, buf2, sizeof(buf2));       
      }
  
      {
        int wc[1] = { tidx-m_curbankstart };
        int k;
        for (k=0; k <= 1; ++k)
        {
          if ((iscur && k == 0) || (isbank && k == 1))
          {
            SETSURFSTRWC("FX_INST_NAME", wc, k, buf);         
            SETSURFSTRWC("FX_INST_PRESET", wc, k, buf2);
            SETSURFBOOLWC("FX_INST_BYPASS", wc, k, en);
            SETSURFBOOLWC("FX_INST_OPEN_UI", wc, k, open);
          }
        }
      }
    }
  }

  // track order or current bank changed
  void SetTrackListChange()
  {
    if (osc_send_inactive()) return;

    if (!m_surfinit)
    {
      m_surfinit=true;

      SETSURFBOOL("REWIND_FORWARD_BYMARKER", (double)(m_altnav == 1));
      SETSURFBOOL("REWIND_FORWARD_SETLOOP", (double)(m_altnav == 2));
      m_nav_active=0;

      m_scrollx=0;
      m_scrolly=0;
      m_zoomx=0;
      m_zoomy=0;
      SETSURFBOOL("SCROLL_X-", false);
      SETSURFBOOL("SCROLL_X+", false);
      SETSURFBOOL("SCROLL_Y-", false);
      SETSURFBOOL("SCROLL_Y+", false);
      SETSURFBOOL("ZOOM_X-", false);
      SETSURFBOOL("ZOOM_X+", false);
      SETSURFBOOL("ZOOM_Y-", false);
      SETSURFBOOL("ZOOM_X+", false);

      int recmode=!!GetToggleCommandState(41186); // rec mode replace (tape)
      Extended(CSURF_EXT_SETRECMODE, &recmode, 0, 0);

      int click=!!GetToggleCommandState(40364); // metronome enable
      Extended(CSURF_EXT_SETMETRONOME, (void*)(INT_PTR)click, 0, 0);

      int rpt=GetSetRepeat(-1);  
      SetRepeatState(!!rpt);

      int ps=GetPlayState();
      SetPlayState(!!(ps&1), !!(ps&2), !!(ps&4));

      double pr=Master_GetPlayRate(0); 
      double bpm=Master_GetTempo();
      Extended(CSURF_EXT_SETBPMANDPLAYRATE, &bpm, &pr, 0);

      int autorecarm=GetToggleCommandState(40740); // auto-recarm all
      Extended(CSURF_EXT_SETAUTORECARM, (void*)(INT_PTR)(!!autorecarm), 0, 0);

      if (m_wantfx&32)
      {
        SETSURFSTR("FX_EQ_HIPASS_NAME", "HPF");
        SETSURFSTR("FX_EQ_LOSHELF_NAME", "Lo Shlf");
        SETSURFSTR("FX_EQ_BAND_NAME", "Band");
        SETSURFSTR("FX_EQ_NOTCH_NAME", "Notch");
        SETSURFSTR("FX_EQ_HISHELF_NAME", "Hi Shlf");
        SETSURFSTR("FX_EQ_LOPASS_NAME", "LPF");
      }

      SetActiveTrackChange();

      SetMarkerBankChange();
    }

    int i;
    for (i=0; i <= m_trackbanksize; ++i)
    {
      int tidx = (!i ? 0 : m_curbankstart+i); // !i == master
      UpdateSurfaceTrack(tidx);
    }
  }

  // m_curtrack changed, not track bank
  void SetActiveTrackChange()
  {
    if (osc_send_inactive()) return;

    int i;
    for (i=1; i <= m_trackbanksize; ++i)
    {
      int tidx=m_curbankstart+i;
      bool iscur = (tidx == m_curtrack);
      int wc[1] = { i }; 
      SETSURFBOOLWC("DEVICE_TRACK_SELECT", wc, 1, iscur);
    }
    
    UpdateSurfaceTrack(m_curtrack);
    SetActiveFXChange();
    SetActiveFXInstChange();
    SetActiveFXEQChange();
  }

  void SetActiveFXEQChange()
  {
    if (osc_send_inactive()) return;

    int fxidx=-1;

    MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
    if (tr) fxidx=TrackFX_GetEQ(tr, false);

    char buf[512];
    buf[0]=0;
    bool en=false;
    bool open=false;
    if (tr && fxidx >= 0)
    {
      en=TrackFX_GetEnabled(tr, fxidx);
      open=TrackFX_GetOpen(tr, fxidx);
      TrackFX_GetPreset(tr, fxidx, buf, sizeof(buf));
    }
    SETSURFSTR("FX_EQ_PRESET", buf);
    SETSURFBOOL("FX_EQ_BYPASS", en);
    SETSURFBOOL("FX_EQ_OPEN_UI", open);

    static const char* bandstr[6] = { "_HIPASS", "_LOSHELF", "_BAND", "_NOTCH", "_HISHELF", "_LOPASS" };
    static const char* parmtypestr[3] = { "_FREQ", "_GAIN", "_Q" };
    static double defval[3] = { 0.0, 0.5, 0.5 };
    static const char* defstr[3] = { "0Hz", "0dB", "1.0" };

    int i, j, k;
    for (i=0; i < 6; ++i)
    {
      bool isband = (i == 2);

      for (j=0; j < 3; ++j)
      {
        char pattern[512];
        strcpy(pattern, "FX_EQ");
        strcat(pattern, bandstr[i]);
        strcat(pattern, parmtypestr[j]);
        for (k=0; k < (isband ? 8 : 1); ++k)
        {
          int wc[1] = { k };
          SetSurfaceVal(pattern, (isband ? wc : 0), (isband ? 1 : 0), 0, 0, &defval[j], defstr[j]);
        }
      }

      for (k=0; k < (isband ? 8 : 1); ++k)
      {
        if (tr && fxidx >= 0)
        {
          bool band_en=TrackFX_GetEQBandEnabled(tr, fxidx, i, k);
          
          char pattern[512];
          strcpy(pattern, "FX_EQ");
          strcat(pattern, bandstr[i]);
          strcat(pattern, "_BYPASS");
          
          int wc[1] = { k };
          if (isband) 
          {
            SETSURFBOOLWC(pattern, wc, 1, band_en);
          }
          else 
          {
            SETSURFBOOL(pattern, band_en);
          }
        }
      }
    }

    if (tr && fxidx >= 0)
    {
      int numparms=TrackFX_GetNumParams(tr, fxidx);           
      int i;
      for (i=0; i < numparms; ++i)
      {
        double val=TrackFX_GetParamNormalized(tr, fxidx, i);
        int f=(fxidx<<16)|i;
        Extended(FX_IDX_MODE(fxidx) == FX_IDX_MODE_REC ? CSURF_EXT_SETFXPARAM_RECFX : CSURF_EXT_SETFXPARAM, tr, &f, &val);
      }
    }
  }

  void SetActiveFXChange()
  {
    if (osc_send_inactive()) return;

    int numfx=0;
    int numparms=0;
    MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
    if (tr) 
    {
      numfx=TrackFX_GetCount(tr);
      numparms=TrackFX_GetNumParams(tr, m_curfx);
    }
    if (m_curfx >= numfx) m_curfx=0;
    if (m_curfxparmbankstart >= numparms) m_curfxparmbankstart=0;

    int i;
    for (i=0; i < m_fxbanksize; ++i)
    {
      bool iscur = (numfx && i == m_curfx);
      int wc[1] = { i+1 }; 
      SETSURFBOOLWC("DEVICE_FX_SELECT", wc, 1, iscur);
    }

    char buf[512];
    char buf2[512];
    char ibuf[128];

    bool en=false;
    bool open=false;
    buf[0]=buf2[0]=ibuf[0]=0;
    if (tr && numfx)
    {
      en=TrackFX_GetEnabled(tr, m_curfx);
      open=TrackFX_GetOpen(tr, m_curfx);
      GetFXName(tr, m_curfx, buf, sizeof(buf));
      sprintf(ibuf, "%d", m_curfx+1);
      TrackFX_GetPreset(tr, m_curfx, buf2, sizeof(buf2));
    }

    SETSURFSTR("FX_NAME", buf);
    SETSURFSTR("FX_NUMBER", ibuf); 
    SETSURFSTR("FX_PRESET", buf2);
    SETSURFBOOL("FX_BYPASS", en);
    SETSURFBOOL("FX_OPEN_UI", open);

    int banksel=0;
    if (m_fxparmbanksize > 0) banksel=m_curfxparmbankstart/m_fxparmbanksize;
    sprintf(buf, "%d", banksel+1);
    SETSURFSTR("DEVICE_FX_PARAM_BANK_SELECT", buf);

    SendUpdateFXParms(tr,m_curfx,numfx,numparms,NULL,0);
  }

  void SendUpdateFXParms(MediaTrack *tr, int curfx, int numfx, int numparms, const int *wc_lead, int wc_lead_size)
  {
    if (osc_send_inactive()) return;
    int i;
    for (i=0; i < m_fxparmbanksize; ++i)
    {
      int parmidx=m_curfxparmbankstart+i;
      char buf[512];
      char buf2[512];
      buf[0]=buf2[0]=0;
      double val=0.0;

      if (tr && curfx < numfx && parmidx < numparms)
      {
        TrackFX_GetParamName(tr, curfx, parmidx, buf, sizeof(buf));
        val=TrackFX_GetParamNormalized(tr, curfx, parmidx);
        TrackFX_GetFormattedParamValue(tr, curfx, parmidx, buf2, sizeof(buf2));
      }

      int wc[32];
      const int wcsz = (wc_lead ? wdl_min(wc_lead_size,31) : 0) +1;

      wc[0] = i+1;
      if (wcsz>1) memcpy(wc+1,wc_lead,(wcsz-1)*sizeof(int));

      SETSURFSTRWC("FX_PARAM_NAME", wc, wcsz, buf);
      SetSurfaceVal("FX_PARAM_VALUE", wc, wcsz, 0, 0, &val, buf2);
      if (parmidx == TrackFX_GetParamFromIdent(tr, curfx, ":wet"))
      {      
        SetSurfaceVal("FX_WETDRY", wc_lead, wc_lead_size, 0, 0, &val, buf2);
      }
    }
  }

  void SetActiveFXInstChange()
  {
    if (osc_send_inactive()) return;

    int fxidx=-1;
    MediaTrack* tr=CSurf_TrackFromID(m_curtrack, !!(m_followflag&8));
    if (tr) fxidx=TrackFX_GetInstrument(tr);    

    int numparms=0;
    if (fxidx >= 0) numparms=TrackFX_GetNumParams(tr, fxidx);
    if (m_curfxinstparmbankstart >= numparms) m_curfxinstparmbankstart=0;

    char buf[512];
    char buf2[512];

    buf[0]=buf2[0]=0;
    if (fxidx >= 0)
    {
      GetFXName(tr, fxidx, buf, sizeof(buf));
      TrackFX_GetPreset(tr, fxidx, buf2, sizeof(buf2));
    }
    SETSURFSTR("FX_INST_NAME", buf);
    SETSURFSTR("FX_INST_PRESET", buf2);

    sprintf(buf, "%d", m_fxinstparmbanksize > 0 ? m_curfxinstparmbankstart/m_fxinstparmbanksize+1 : 1);
    SETSURFSTR("DEVICE_FX_INST_PARAM_BANK_SELECT", buf);

    int i;
    for (i=0; i < m_fxinstparmbanksize; ++i)
    {
      int parmidx=m_curfxinstparmbankstart+i;
      buf[0]=buf2[0]=0;         
      double val=0.0;

      if (tr && fxidx >= 0 && parmidx < numparms)
      {
        TrackFX_GetParamName(tr, fxidx, parmidx, buf, sizeof(buf));
        val=TrackFX_GetParamNormalized(tr, fxidx, parmidx);
        TrackFX_GetFormattedParamValue(tr, fxidx, parmidx, buf2, sizeof(buf2));   
      }

      int wc[1] = { i+1 };
      SETSURFSTRWC("FX_INST_PARAM_NAME", wc, 1, buf);
      SetSurfaceVal("FX_INST_PARAM_VALUE", wc, 1, 0, 0, &val, buf2);
    }
  }

  void SetMarkerBankChange(int old_marker_size=0, int old_region_size=0)
  {
    if (osc_send_inactive()) return;
    const int marker_top = wdl_max(m_markerbanksize,old_marker_size);
    const int region_top = wdl_max(m_regionbanksize,old_region_size);

    const bool domarkers = (marker_top>0 && (m_wantmarker&1));
    const bool doregions = (region_top>0 && (m_wantmarker&2));
    if (!domarkers && !doregions) return;

    int mcnt=-m_curmarkerbankstart;
    int rcnt=-m_curregionbankstart;

    char buf[128];

    bool isrgn;
    const char *name=NULL;
    int idx;
    int i=0;
    double pos, endpos;
    while (0 != (i=EnumProjectMarkers3(NULL,i, &isrgn, &pos, &endpos, &name, &idx, NULL)))
    {
      if (!name) name="";
      if (!isrgn && domarkers && mcnt < m_markerbanksize)
      {
        if (++mcnt > 0)
        { 
          int wc[1] = { mcnt };
          SETSURFSTRWC("MARKER_NAME", wc, 1, name);
          sprintf(buf, "%d", idx);
          SETSURFSTRWC("MARKER_NUMBER", wc, 1, buf);
          SETSURFFLOATWC("MARKER_TIME", wc, 1, pos);
        }
      }
      else if (isrgn && doregions && rcnt < m_regionbanksize)
      {
        if (++rcnt > 0)
        {
          int wc[1] = { rcnt };
          SETSURFSTRWC("REGION_NAME", wc, 1, name);  
          sprintf(buf, "%d", idx);
          SETSURFSTRWC("REGION_NUMBER", wc, 1, buf);
          SETSURFFLOATWC("REGION_TIME", wc, 1, pos);
          if (endpos > pos) endpos-=pos;
          else endpos=0.0;
          SETSURFFLOATWC("REGION_LENGTH", wc, 1, endpos);
        }
      }
      if (mcnt >= m_markerbanksize && rcnt >= m_regionbanksize) break;
    }

    if (domarkers)
    {
      for (i=max(mcnt+1, 1); i <= marker_top; ++i)
      {
        int wc[1] = { i };
        const char *p = i <= m_markerbanksize ? "" : SPECIAL_STRING_CLEARCACHE;
        SETSURFSTRWC("MARKER_NAME", wc, 1, p);
        SETSURFSTRWC("MARKER_NUMBER", wc, 1, p);
        if (p == SPECIAL_STRING_CLEARCACHE) SETSURFSTRWC("MARKER_TIME",wc,1,p);
      }
    }
    if (doregions)
    {
      for (i=max(rcnt+1, 1); i <= region_top; ++i)
      {
        int wc[1] = { i };
        const char *p = i <= m_regionbanksize ? "" : SPECIAL_STRING_CLEARCACHE;
        SETSURFSTRWC("REGION_NAME", wc, 1, p);
        SETSURFSTRWC("REGION_NUMBER", wc, 1, p);
        if (p == SPECIAL_STRING_CLEARCACHE) 
        {
          SETSURFSTRWC("REGION_TIME",wc,1,p);
          SETSURFSTRWC("REGION_LENGTH",wc,1,p);
        }
      }
    }
  }

  int GetSurfaceTrackIdx(MediaTrack* tr) // returns -1 if the track is not visible in the surface
  {
    int tidx=CSurf_TrackToID(tr, !!(m_followflag&8));
    if (tidx == m_curtrack) return tidx;
    if (tidx > m_curbankstart && tidx <= m_curbankstart+m_trackbanksize) return tidx;
    return -1;
  }

  void SetTrackTitle(MediaTrack* tr, const char* title)
  {   
    if (osc_send_inactive()) return;
    char buf[128];
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) 
    {
      char ibuf[128];   
      sprintf(ibuf, "%d", tidx);
      if (!title[0] || !strcmp(ibuf, title))
      {
        sprintf(buf, "Track %d", tidx);
        title=buf;
      }
    }
    SETSURFTRACKSTR("TRACK_NAME", tidx, title);
  }

  void SetSurfaceSelectedImpl(int tidx, bool selected)
  {
    SETSURFTRACKBOOL("TRACK_SELECT", tidx, selected);
  }

  void SetSurfaceSelected(MediaTrack *tr, bool selected)
  {
    if (osc_send_inactive()) return;
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) SetSurfaceSelectedImpl(tidx, selected);
  }

  void SetSurfaceVolumeImpl(int tidx, double volume)
  {
    double db=VAL2DB(volume);
    double v=DB2SLIDER(db)/1000.0;
    char buf[128];
    mkvolstr(buf, volume);
    
    if (!tidx) SetSurfaceVal("MASTER_VOLUME", 0, 0, 0, &db, &v, buf);
    SetSurfaceTrackVal("TRACK_VOLUME", tidx, 0, &db, &v, buf); 
  }

  void SetSurfaceVolume(MediaTrack *tr, double volume)
  {
    if (osc_send_inactive()) return;
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) SetSurfaceVolumeImpl(tidx, volume);
  }

  void SetSurfacePan(MediaTrack* tr, double pan)
  {
    // ignore because we handle CSURF_EXT_SETPAN_EX
  }

  void SetSurfaceMuteImpl(int tidx, bool mute)
  {
    SETSURFTRACKBOOL("TRACK_MUTE", tidx, mute);
  }

  void SetSurfaceMute(MediaTrack* tr, bool mute)
  {
    if (osc_send_inactive()) return;
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) SetSurfaceMuteImpl(tidx, mute);
  }

  void SetSurfaceSoloImpl(int tidx, bool solo)
  {
    SETSURFTRACKBOOL("TRACK_SOLO", tidx, solo);
    if (m_anysolo != AnyTrackSolo(0))
    {
      m_anysolo=!m_anysolo;
      SETSURFBOOL("ANY_SOLO", m_anysolo);
    }
  }

  void SetSurfaceSolo(MediaTrack *tr, bool solo)
  {
    if (osc_send_inactive()) return;
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) SetSurfaceSoloImpl(tidx, solo);
  }

  void SetSurfaceRecArmImpl(int tidx, bool recarm)
  {
    SETSURFTRACKBOOL("TRACK_REC_ARM", tidx, recarm);
  }

  void SetSurfaceRecArm(MediaTrack *tr, bool recarm)
  {
    if (osc_send_inactive()) return;
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) SetSurfaceRecArmImpl(tidx, recarm);
  }

  bool GetTouchState(MediaTrack* tr, int ispan)
  {
    if (ispan != 0 && ispan != 1 && ispan != 2) return false; 

    const int tidx=GetSurfaceTrackIdx(tr);
    if (tidx < 0 || tidx >= MAX_LASTTOUCHED_TRACK) return false;

    if (m_supports_touch&(1<<ispan))
    {
      return !!(m_hastouch[tidx]&(1<<ispan));
    }

    DWORD lastt = (!ispan ? m_vol_lasttouch[tidx] : ispan == 1 ? m_pan_lasttouch[tidx] : m_pan2_lasttouch[tidx]);
    if (!lastt) return false;
    return ((GetTickCount()-lastt) < 3000);
  }

  void SetAutoMode(int mode) // the passed-in mode is ignored
  {
    if (osc_send_inactive()) return;
    int i;
    for (i=0; i <= m_trackbanksize; ++i)
    {
      int tidx;
      if (!i) tidx=m_curtrack;
      else tidx=m_curbankstart+i;
      MediaTrack* tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
      int mode=0;
      if (tr) mode=GetTrackAutomationMode(tr);
      SetSurfaceAutoModeImpl(tidx, mode);
    }
  }

  void SetSurfaceAutoModeImpl(int tidx, int mode)
  {
    SETSURFTRACKBOOL("TRACK_AUTO_TRIM", tidx, (mode == AUTO_MODE_TRIM));
    SETSURFTRACKBOOL("TRACK_AUTO_READ", tidx, (mode == AUTO_MODE_READ));
    SETSURFTRACKBOOL("TRACK_AUTO_LATCH", tidx, (mode == AUTO_MODE_LATCH));
    SETSURFTRACKBOOL("TRACK_AUTO_TOUCH", tidx, (mode == AUTO_MODE_TOUCH));
    SETSURFTRACKBOOL("TRACK_AUTO_WRITE", tidx, (mode == AUTO_MODE_WRITE));
  }

  void SetSurfaceAutoMode(MediaTrack* tr, int mode)
  {
    if (osc_send_inactive()) return;
    int tidx=GetSurfaceTrackIdx(tr);
    if (tidx >= 0) SetSurfaceAutoModeImpl(tidx, mode);
  }

  void SetRepeatState(bool rpt)
  {
    SETSURFBOOL("REPEAT", (double)rpt);
  }

  void SetPlayState(bool play, bool pause, bool rec)
  {
    if (osc_send_inactive()) return;
    SETSURFBOOL("RECORD", (play && rec));
    SETSURFBOOL("STOP", !play);
    SETSURFBOOL("PAUSE",  pause);
    SETSURFBOOL("PLAY", play);
  }

  int Extended(int call, void* parm1, void* parm2, void* parm3)
  {
    int tidx=-1;

    if (call == CSURF_EXT_SETPAN_EX ||
        call == CSURF_EXT_SETINPUTMONITOR ||
        call == CSURF_EXT_SETSENDVOLUME ||
        call == CSURF_EXT_SETSENDPAN ||
        call == CSURF_EXT_SETRECVVOLUME ||
        call == CSURF_EXT_SETRECVPAN ||
        call == CSURF_EXT_SETFXENABLED ||
        call == CSURF_EXT_SETFXOPEN)
    {
      if (osc_send_inactive()) return 1;
      if (parm1) tidx=CSurf_TrackToID((MediaTrack*)parm1, !!(m_followflag&8)); 
      if (tidx < 0) return 1;
      parm1=&tidx;
      call += CSURF_EXT_IMPL_ADD;
    }

    if (call == CSURF_EXT_SETPAN_EX_IMPL)
    {
      if (parm1 && parm2 && parm3)
      {
        int tidx=*(int*)parm1;
        double* pan=(double*)parm2;
        int mode=*(int*)parm3;

        char panstr[256];
        mkpanstr(panstr, *pan);
        if (!strcmp(panstr, "center")) strcpy(panstr, "C");
        double tpan=0.5*(*pan+1.0);

        if (!tidx) 
        {
          SetSurfaceVal("MASTER_PAN", 0,0, 0, 0, &tpan, panstr);
        }
        else
        {     
          const char* panmode="";    
          char panstr2[256];
          panstr2[0]=0;
          double tpan2=0.5*(pan[1]+1.0);
          if (mode == PAN_MODE_DUAL_PAN)
          {
            panmode="Dual pan";    
            mkpanstr(panstr2, pan[1]);
            if (!strcmp(panstr2, "center")) strcpy(panstr2, "C");
          }
          else
          {
            snprintf(panstr2, sizeof(panstr2),"%.0fW", 100.0*pan[1]);
            if (mode == PAN_MODE_CLASSIC)
            {
              panmode="Balance (classic)";
            }
            else if (mode == PAN_MODE_NEW_BALANCE)
            {
              panmode="Balance";
            }
            else if (mode == PAN_MODE_STEREO_PAN)
            {
              panmode="Stereo pan";
            }
          }
        
          SETSURFTRACKSTR("TRACK_PAN_MODE", tidx, panmode);
          SetSurfaceTrackVal("TRACK_PAN2", tidx, 0, 0, &tpan2, panstr2);
        }
          
        SetSurfaceTrackVal("TRACK_PAN", tidx, 0, 0, &tpan, panstr);
      }
    }

    if (call == CSURF_EXT_SETSENDVOLUME_IMPL)
    {
      if (parm1 && parm2 && parm3)
      {
        int tidx=*(int*)parm1;
        int sidx=*(int*)parm2;
        double volume=*(double*)parm3;

        double v=DB2SLIDER(VAL2DB(volume))/1000.0;
        char buf[128];
        mkvolstr(buf, volume);

        int wc[2] = { sidx+1, tidx-m_curbankstart };
        if (tidx == m_curtrack)
        {         
          SetSurfaceVal("TRACK_SEND_VOLUME", wc, 1, 0, 0, &v, buf);
        }
        if (wc[1] > 0 && wc[1] <= m_trackbanksize)
        {
          SetSurfaceVal("TRACK_SEND_VOLUME", wc, 2, 0, 0, &v, buf);
        }
        if (!tidx)
        {
          SetSurfaceVal("MASTER_SEND_VOLUME", wc, 1, 0, 0, &v, buf);
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETRECVVOLUME_IMPL)
    {
      if (parm1 && parm2 && parm3)
      {
        int tidx=*(int*)parm1;
        int sidx=*(int*)parm2;
        double volume=*(double*)parm3;

        double v=DB2SLIDER(VAL2DB(volume))/1000.0;
        char buf[128];
        mkvolstr(buf, volume);

        int wc[2] = { sidx+1, tidx-m_curbankstart };
        if (tidx == m_curtrack)
        {         
          SetSurfaceVal("TRACK_RECV_VOLUME", wc, 1, 0, 0, &v, buf);
        }
        if (wc[1] > 0 && wc[1] <= m_trackbanksize)
        {
          SetSurfaceVal("TRACK_RECV_VOLUME", wc, 2, 0, 0, &v, buf);
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETSENDPAN_IMPL)
    {
      if (parm1 && parm2 && parm3) // parm1 can be index 0
      {
        int tidx=*(int*)parm1;
        int sidx=*(int*)parm2;
        double pan=*(double*)parm3;
        double p=0.5*(pan+1.0);
        char buf[128];
        mkpanstr(buf, pan);

        int wc[2] = { sidx+1, tidx-m_curbankstart };
        if (tidx == m_curtrack)
        {
          SetSurfaceVal("TRACK_SEND_PAN", wc, 1, 0, 0, &p, buf);
        }
        if (tidx >= 0 && wc[1] > 0 && wc[1] <= m_trackbanksize)
        {
          SetSurfaceVal("TRACK_SEND_PAN", wc, 2, 0, 0, &p, buf);
        }
        if (!tidx) 
        {      
          SetSurfaceVal("MASTER_SEND_PAN", wc, 1, 0, 0, &p, buf);
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETRECVPAN_IMPL)
    {
      if (parm1 && parm2 && parm3) // parm1 can be index 0
      {
        int tidx=*(int*)parm1;
        int sidx=*(int*)parm2;
        double pan=*(double*)parm3;
        double p=0.5*(pan+1.0);
        char buf[128];
        mkpanstr(buf, pan);

        int wc[2] = { sidx+1, tidx-m_curbankstart };
        if (tidx == m_curtrack)
        {
          SetSurfaceVal("TRACK_RECV_PAN", wc, 1, 0, 0, &p, buf);
        }
        if (tidx >= 0 && wc[1] > 0 && wc[1] <= m_trackbanksize)
        {
          SetSurfaceVal("TRACK_RECV_PAN", wc, 2, 0, 0, &p, buf);
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETINPUTMONITOR_IMPL)
    {
      if (parm1 && parm2)
      {
        int tidx=*(int*)parm1;
        double mon=(*(int*)parm2);
        SETSURFTRACKNORM("TRACK_MONITOR", tidx, mon);
      }
      return 1;
    }

    if (call == CSURF_EXT_SETMETRONOME)
    {       
      SETSURFBOOL("METRONOME", !!parm1);
      return 1;
    }

    if (call == CSURF_EXT_SETAUTORECARM)
    {
      SETSURFBOOL("AUTO_REC_ARM", !!parm1);
      return 1;
    }

    if (call == CSURF_EXT_SETRECMODE)
    {
      if (parm1)
      {
        SETSURFBOOL("REPLACE", (*(int*)parm1 == 1));
      }
      return 1;
    }

    if (call == CSURF_EXT_SETLASTTOUCHEDTRACK)
    {
      if (parm1)
      {
        if (m_followflag&1)
        {
          MediaTrack* tr=(MediaTrack*)parm1;
          int tidx=CSurf_TrackToID(tr, !!(m_followflag&8));
          if (tidx >= 0 && tidx != m_curtrack)
          {
            m_curtrack=tidx;
            SetActiveTrackChange();         
          }
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETFXENABLED_IMPL ||
        call == CSURF_EXT_SETFXOPEN_IMPL)
    {
      if (parm1 && parm2) // parm3 can be index 0
      {
        int tidx=*(int*)parm1;
        int fxidx=*(int*)parm2;
        bool en=!!parm3;

        bool isinst=false;
        bool iseq=false;
        if (m_wantfx&(4|32))
        {
          MediaTrack* tr=CSurf_TrackFromID(tidx, !!(m_followflag&8));
          isinst = ((m_wantfx&4) && fxidx == TrackFX_GetInstrument(tr));
          iseq = ((m_wantfx&32) && fxidx == TrackFX_GetEQ(tr, false));
        }

        int wc[2] = { fxidx+1, tidx };
        if (tidx == m_curtrack)
        {
          if (call == CSURF_EXT_SETFXENABLED_IMPL) 
          {
            if (fxidx==m_curfx) SETSURFBOOLWC("FX_BYPASS", wc, 0, en);
            SETSURFBOOLWC("FX_BYPASS", wc, 1, en);
            if (isinst) SETSURFBOOL("FX_INST_BYPASS", en);
            if (iseq) SETSURFBOOL("FX_EQ_BYPASS", en);
          }
          else if (call == CSURF_EXT_SETFXOPEN_IMPL)
          {
            if (fxidx==m_curfx) SETSURFBOOLWC("FX_OPEN_UI", wc, 0, en);
            SETSURFBOOLWC("FX_OPEN_UI", wc, 1, en);
            if (isinst) SETSURFBOOL("FX_INST_OPEN_UI", en);
            if (iseq) SETSURFBOOL("FX_EQ_OPEN_UI", en);
          }
        }
        if (wc[1] > 0 && wc[1] <= m_trackbanksize) 
        {
          if (call == CSURF_EXT_SETFXENABLED_IMPL)
          {
            SETSURFBOOLWC("FX_BYPASS", wc, 2, en);
            if (isinst) SETSURFBOOLWC("FX_INST_BYPASS", wc+1, 1, en);          
            if (iseq) SETSURFBOOLWC("FX_EQ_BYPASS", wc+1, 1, en);
          }
          else if (call == CSURF_EXT_SETFXOPEN_IMPL)
          {
            SETSURFBOOLWC("FX_OPEN_UI", wc, 2, en);
            if (isinst) SETSURFBOOLWC("FX_INST_OPEN_UI", wc+1, 1, en);
            if (iseq) SETSURFBOOLWC("FX_EQ_OPEN_UI", wc+1, 1, en);
          }
        }        
      }
      return 1;
    }

    if (call == CSURF_EXT_SETFXPARAM)
    {
      if (osc_send_inactive()) return 1;
      if (parm1 && parm2 && parm3) 
      {
        MediaTrack* tr=(MediaTrack*)parm1;
        int f=*(int*)parm2;
        int fxidx=(f>>16)&0xFFFF;
        int parmidx=f&0xFFFF;
        double val=*(double*)parm3;

        int tidx=CSurf_TrackToID(tr, !!(m_followflag&8));

        bool isinst=false;
        bool iseq=false;

        bool wet_only = false;
        bool iscurfx=false;
        bool iscurinst=false;
        bool iscureq=false;
        bool islasttouched=false;
        bool iscurtrackfx=false;
        bool isbanktrackfx=false;
        bool isbanktrackfxinst=false;
        bool isbanktrackeq=false;
        
        if ((m_wantfx&4) && fxidx == TrackFX_GetInstrument(tr)) isinst=true;
        else if ((m_wantfx&32) && fxidx == TrackFX_GetEQ(tr, false)) iseq=true;

        if (tidx == m_curtrack)
        {
          bool is_wet = (parmidx == TrackFX_GetParamFromIdent(tr, fxidx, ":wet"));

          if ((m_wantfx&1) && fxidx == m_curfx)
          {
            if (parmidx >= m_curfxparmbankstart && parmidx < m_curfxparmbankstart+m_fxparmbanksize)
              iscurfx = true;
            else if (is_wet)
              iscurfx = wet_only = true;
          }
  
          if (isinst)
          {
            if (parmidx >= m_curfxinstparmbankstart && parmidx < m_curfxinstparmbankstart+m_fxinstparmbanksize)
              iscurinst=true;
            else if (is_wet)
              iscurinst = wet_only = true;
          }

          if (iseq)
          {
            iscureq=true;
          }
        }

        if (tidx > m_curbankstart && tidx <= m_curbankstart+m_trackbanksize)
        {
          bool is_wet = (parmidx == TrackFX_GetParamFromIdent(tr, fxidx, ":wet"));

          if ((m_wantfx&8))
          {
            if (parmidx >= m_curfxparmbankstart && parmidx < m_curfxparmbankstart+m_fxparmbanksize)
              isbanktrackfx=true;
            else if (is_wet)
              isbanktrackfx = wet_only = true;
          }

          if (isinst && (m_wantfx&16))
          {
            if (parmidx >= m_curfxinstparmbankstart && parmidx < m_curfxinstparmbankstart+m_fxinstparmbanksize)
              isbanktrackfxinst=true;
            else if (is_wet)
              isbanktrackfxinst = wet_only = true;
          }

          if (iseq && (m_wantfx&64))
          {
            isbanktrackeq=true;
          }
        }

        if (m_wantfx&2)
        {
          int ltidx=-1;
          int lfxidx=-1;
          int lparmidx=-1;
          if (GetLastTouchedFX(&ltidx, &lfxidx, &lparmidx) &&
              tidx == ltidx && fxidx == lfxidx && parmidx == lparmidx)
          {
            islasttouched=true;
          }
        }

        if (iscurfx || iscurinst || iscureq || islasttouched || 
            iscurtrackfx || isbanktrackfx || isbanktrackfxinst || isbanktrackeq)
        {
          char buf[512];
          buf[0]=0;
          TrackFX_GetFormattedParamValue(tr, fxidx, parmidx, buf, sizeof(buf));
          if (!buf[0]) snprintf(buf, sizeof(buf),"%.3f", val);

          if (iscureq || isbanktrackeq)
          {
            int bandtype=-1;
            int bandidx=0;
            int parmtype=-1;
            if (TrackFX_GetEQParam(tr, fxidx, parmidx, &bandtype, &bandidx, &parmtype, 0))
            {
              char pattern[512];
              strcpy(pattern, "FX_EQ");

              if (bandtype < 0 || bandtype >= 6) bandtype=2;
              static const char* bandstr[6] = { "_HIPASS", "_LOSHELF", "_BAND", "_NOTCH", "_HISHELF", "_LOPASS" };
              strcat(pattern, bandstr[bandtype]);
              bool isband = (bandtype == 2);

              if (parmtype < 0 || parmtype >= 3) parmtype=0;
              static const char* parmtypestr[3] = { "_FREQ", "_GAIN", "_Q" };
              strcat(pattern, parmtypestr[parmtype]);

              if (parmtype == 0) strcat(buf, "Hz");
              else if (parmtype == 1) strcat(buf, "dB");

              int wc[2] = { bandidx, tidx-m_curbankstart };              
              if (iscureq) SetSurfaceVal(pattern, (isband ? wc : 0), (isband ? 1 : 0), 0, 0, &val, buf);
              if (isbanktrackeq) SetSurfaceVal(pattern, (isband ? wc : wc+1), (isband ? 2 : 1), 0, 0, &val, buf);
            }
          }

          if (iscurfx || isbanktrackfx)
          {
            bool iswet = (parmidx == TrackFX_GetParamFromIdent(tr, fxidx, ":wet"));

            int wc[3] = { parmidx-m_curfxparmbankstart+1, fxidx+1, tidx-m_curbankstart };
            int k;
            for (k=1; k <= 3; ++k)
            {
              if ((iscurfx && k == 1) || (iscurtrackfx && k == 2) || (isbanktrackfx && k == 3))
              {
                if (!wet_only)
                {
                  SetSurfaceVal("FX_PARAM_VALUE", wc, k, 0, 0, &val, buf);
                }
                if (iswet)
                {
                  SetSurfaceVal("FX_WETDRY", wc+1, k-1, 0, 0, &val, buf);
                }
              }
            }
          } 
    
          if (!wet_only && (iscurinst || isbanktrackfxinst))
          {
            int wc[2] = { parmidx-m_curfxinstparmbankstart+1, tidx-m_curbankstart };
            if (iscurinst) SetSurfaceVal("FX_INST_PARAM_VALUE", wc, 1, 0, 0, &val, buf);
            if (isbanktrackfxinst) SetSurfaceVal("FX_INST_PARAM_VALUE", wc, 2, 0, 0, &val, buf);
          }

          if (islasttouched)
          {
            SetSurfaceVal("LAST_TOUCHED_FX_PARAM_VALUE", 0, 0, 0, 0, &val, buf);

            buf[0]=0;
            char ibuf[128];    
            sprintf(ibuf, "%d", tidx);
            const char* p=GetTrackInfo((INT_PTR)tr, 0);
            if (p) lstrcpyn(buf, p, sizeof(buf));              
            if (!buf[0] || !strcmp(buf, ibuf))
            {
              if (!tidx) strcpy(buf, "MASTER");
              else sprintf(buf, "Track %d", tidx);
            }         
            SetSurfaceVal("LAST_TOUCHED_FX_TRACK_NAME", 0, 0, 0, 0, 0, buf);
            SetSurfaceVal("LAST_TOUCHED_FX_TRACK_NUMBER", 0, 0, 0, 0, 0, ibuf);

            buf[0]=0;
            if (tr) GetFXName(tr, fxidx, buf, sizeof(buf));
            SetSurfaceVal("LAST_TOUCHED_FX_NAME", 0, 0, 0, 0, 0, buf);
            buf[0]=0;
            if (tr) sprintf(buf, "%d", fxidx+1);
            SetSurfaceVal("LAST_TOUCHED_FX_NUMBER", 0, 0, 0, 0, 0, buf);         
            buf[0]=0;
            if (tr) TrackFX_GetParamName(tr, fxidx, parmidx, buf, sizeof(buf));
            SetSurfaceVal("LAST_TOUCHED_FX_PARAM_NAME", 0, 0, 0, 0, 0, buf);
          }
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETFOCUSEDFX ||
        call == CSURF_EXT_SETLASTTOUCHEDFX)
    {
      if (parm1 && !parm2 && parm3)
      {
        if (((m_followflag&2) && call == CSURF_EXT_SETLASTTOUCHEDFX) ||
            ((m_followflag&4) && call == CSURF_EXT_SETFOCUSEDFX))
        {
          MediaTrack* tr=(MediaTrack*)parm1;
          int tidx=CSurf_TrackToID(tr, !!(m_followflag&8));
          int fxidx=*(int*)parm3;
          if (tidx >= 0 && fxidx >= 0 && fxidx < TrackFX_GetCount(tr))
          {
            if (tidx != m_curtrack) 
            {
              m_curtrack=tidx;
              m_curfx=fxidx;
              SetActiveTrackChange();
            }
            else if (fxidx != m_curfx)
            {
              m_curfx=fxidx;
              SetActiveFXChange();
            }
          }
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETBPMANDPLAYRATE)
    {
      if (osc_send_inactive()) return 1;
      char buf[512];
      buf[0]=0;
      if (parm1)
      {
        double bpm=*(double*)parm1;
        double nbpm=Master_NormalizeTempo(bpm, false);
        snprintf(buf, sizeof(buf), "%g", bpm);
        SetSurfaceVal("TEMPO", 0, 0, 0, &bpm, &nbpm, buf);
      }
      if (parm2)
      {
        double pr=*(double*)parm2;
        double npr=Master_NormalizePlayRate(pr, false);
        snprintf(buf, sizeof(buf), "%g", pr);
        SetSurfaceVal("PLAY_RATE", 0, 0, 0, &pr, &npr, buf);
      }
      return 1;
    }

    if (call == CSURF_EXT_SETMIXERSCROLL)
    {
      if (parm1 && (m_followflag&8))
      {
        MediaTrack* tr=(MediaTrack*)parm1;
        int tidx=CSurf_TrackToID(tr, true);
        tidx=max(0, tidx-1);             
        if (m_curbankstart != tidx || parm3)
        {
          m_curbankstart=tidx;
          SetTrackListChange();
        }
      }
      return 1;
    }

    if (call == CSURF_EXT_SETFXCHANGE)
    {
      SetTrackListChange();
      return 1;
    }

    if (call == CSURF_EXT_SETPROJECTMARKERCHANGE)
    {
      SetMarkerBankChange();
      return 1;
    }

    if (call == CSURF_EXT_RESET)
    {
      m_lastvals.DeleteAll();
      m_surfinit=false;
      m_lastupd=0;
      m_lastpos=0.0;
      m_anysolo=false;
      SetTrackListChange();  
      SetActiveTrackChange();
      return 1;
    }

    if (call == CSURF_EXT_SUPPORTS_EXTENDED_TOUCH) return 1;

    return 0;
  }

  bool osc_send_inactive() const { return (!m_osc || !m_osc->m_send_enable) && !m_osc_local; }

};


// simple 1-way message sending (like from reascript)
void OscLocalMessageToHost(const char* msg, const double* value)
{
  OscMessageWrite wmsg;
  wmsg.PushWord(msg);
  if (value) wmsg.PushFloatArg((float)*value);

  int len=0;
  char* p=(char*)wmsg.GetBuffer(&len); // OK to write on this
  OscMessageRead rmsg(p, len);

  static CSurf_Osc *tmposc;
  if (!tmposc)
  {
    tmposc = new CSurf_Osc(0, 0, 0, 0, 0, 0, 0, 0, 0);
    Plugin_Register("csurf_inst",(void*)tmposc); // install as a hidden surface so GetTouchState() is called, etc
  }
  tmposc->ProcessMessage(&rmsg);
}


static IReaperControlSurface *createFunc(const char* typestr, const char* cfgstr, int* err)
{
  char name[512];
  int flags=0;
  int recvport=0; 
  char sendip[128];
  int sendport=0;
  int maxpacketsz=DEF_MAXPACKETSZ;
  int sendsleep=DEF_SENDSLEEP;
  char cfgfn[2048];
  ParseCfg(cfgstr, name, sizeof(name), &flags, &recvport, sendip, &sendport, &maxpacketsz, &sendsleep, cfgfn, sizeof(cfgfn));
  return new CSurf_Osc(name, flags, recvport, sendip, sendport, maxpacketsz, sendsleep, 0, cfgfn);
}


void* CreateLocalOscHandler(void* obj, OscLocalCallbackFunc callback)
{
  OscLocalHandler* osc_local=new OscLocalHandler;
  osc_local->m_obj=obj;
  osc_local->m_callback=callback;
  return new CSurf_Osc(0, 0, 0, 0, 0, 0, 0, osc_local, 0);
}

void SendLocalOscMessage(void* csurf_osc, const char* msg, int msglen)
{
  if (!csurf_osc || !msg || !msg[0] || msglen > MAX_OSC_MSG_LEN) return;

  char buf[MAX_OSC_MSG_LEN];
  memcpy(buf, msg, msglen);
  OscMessageRead rmsg(buf, msglen);
  CSurf_Osc::ProcessOscMessage(csurf_osc, &rmsg);
}

void DestroyLocalOscHandler(void* csurf_osc)
{
  delete (CSurf_Osc*)csurf_osc;
}


bool OscListener(void* obj, OscMessageRead* rmsg)
{
  char dump[MAX_OSC_MSG_LEN*2];
  dump[0]=0;
  rmsg->DebugDump(0, dump, sizeof(dump));
  if (dump[0])
  {
    strcat(dump, "\r\n");    
    SendMessage((HWND)obj, WM_USER+100, 0, (LPARAM)dump);
  }
  return true;
}

static WDL_DLGRET OscListenProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static OscHandler* s_osc;
  static WDL_FastString s_state;
  static bool s_state_chg;

  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      s_osc=0;
      s_state.Set("");
      if (lParam)
      {
        char buf[512];
        snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("OSC: listening to port %d","csurf_osc"), (int)lParam);
        SetWindowText(hwndDlg, buf);
        
        s_osc=OscAddLocalListener(OscListener, hwndDlg, (int)lParam);
        if (s_osc) SetTimer(hwndDlg, 1, 50, 0);
        else SetDlgItemText(hwndDlg, IDC_EDIT1, __LOCALIZE("Error: can't create OSC listener","csurf_osc"));
        SetTimer(hwndDlg,2,100,0);
      }
    }
    return 0;

    case WM_TIMER:
      if (wParam == 1)
      {
        OscGetInput(s_osc); // will call back to OscListener
      }
      else if (wParam == 2)
      {
        if (s_state_chg)
        {
          SetDlgItemText(hwndDlg, IDC_EDIT1, s_state.Get());
          SendDlgItemMessage(hwndDlg, IDC_EDIT1, EM_SCROLL, SB_BOTTOM, 0);
          s_state_chg = false;
        }
      }
    return 0;

    case WM_USER+100:
    {
      if (lParam)
      {
        const char* msg=(const char*)lParam;
        if (msg[0])
        {                 
          s_state.Append(msg);
          int skip = 0;
          while (skip < s_state.GetLength()-2048 && s_state.Get()[skip])
          {
            while (s_state.Get()[skip] && s_state.Get()[skip] != '\r' && s_state.Get()[skip] != '\n') skip++;
            while (s_state.Get()[skip] == '\r' || s_state.Get()[skip] == '\n') skip++;
          }
          if (skip>0) s_state.DeleteSub(0,skip);
          s_state_chg=true;
        }
      }
    }
    return 0;

    case WM_DESTROY:
      s_state.Set("");
      OscRemoveLocalListener(s_osc);
      delete s_osc;
      s_osc=0;
    return 0;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hwndDlg, 0);
      }
    return 0;
  }

  return 0;
}


static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static char s_cfgfn[2048];
  static WDL_PtrList<char> s_patterncfgs;

  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      char name[512];
      name[0]=0;
      int flags=0;
      int recvport=0;    
      char sendip[128];
      sendip[0]=0;
      int sendport=0;
      int maxpacketsz=DEF_MAXPACKETSZ;
      int sendsleep=DEF_SENDSLEEP;
      s_cfgfn[0]=0;

      if (lParam) 
      {
        ParseCfg((const char*)lParam, name, sizeof(name), &flags, &recvport, sendip, &sendport, &maxpacketsz, &sendsleep, s_cfgfn, sizeof(s_cfgfn));
      }
      if (!sendip[0]) strcpy(sendip, "0.0.0.0");
      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO1));
      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO4));

      SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM) __LOCALIZE("Device IP/port","csurf_osc"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM) __LOCALIZE("Device IP/port [send only]","csurf_osc"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM) __LOCALIZE("Local port","csurf_osc"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM) __LOCALIZE("Local port [receive only]","csurf_osc"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM) __LOCALIZE("Configure device IP+local port","csurf_osc"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM) __LOCALIZE("Disabled","csurf_osc"));

      int cbidx = 5;
      switch (flags&3)
      {
        case 1: cbidx = 3; break;
        case 2: cbidx = 1; break;
        case 3: cbidx = recvport == 0 ? 0 : sendport == 0 ? 2 : 4; break;
      }
      SendDlgItemMessage(hwndDlg, IDC_COMBO4, CB_SETCURSEL, cbidx, 0);

      SetDlgItemText(hwndDlg, IDC_EDIT4, name);

      char buf[256];
      sprintf(buf, "%d", recvport?recvport:DEF_RECVPORT);
      SetDlgItemText(hwndDlg, IDC_EDIT1, buf);

      SetDlgItemText(hwndDlg, IDC_EDIT2, sendip);
      sprintf(buf, "%d", sendport?sendport:DEF_SENDPORT);
      SetDlgItemText(hwndDlg, IDC_EDIT3, buf);
 
      GetLocalIP(buf, sizeof(buf));
      if (!buf[0]) strcpy(buf, "0.0.0.0");
      SetDlgItemText(hwndDlg, IDC_EDIT6, buf);

      if (flags&4) CheckDlgButton(hwndDlg, IDC_CHECK3, BST_CHECKED);

      sprintf(buf, "%d", maxpacketsz);
      SetDlgItemText(hwndDlg, IDC_EDIT7, buf);

      sprintf(buf, "%d", sendsleep);
      SetDlgItemText(hwndDlg, IDC_EDIT8, buf);

      SendMessage(hwndDlg, WM_USER+100, 0, 0);
    }
    // fall through

    case WM_COMMAND:
      if (uMsg == WM_INITDIALOG ||
          (LOWORD(wParam) == IDC_COMBO4 && HIWORD(wParam) == CBN_SELCHANGE))
      {
        const int sel = (int)SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_GETCURSEL,0,0);

        int show = (sel == 2 || sel == 3 || sel == 4) ? SW_SHOWNA : SW_HIDE;
        ShowWindow(GetDlgItem(hwndDlg, IDC_LISTEN_LBL), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_LISTEN_LBL2), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_EDIT1), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_EDIT6), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), show);

        show = (sel == 0 || sel == 1 || sel == 4) ? SW_SHOWNA : SW_HIDE;
        ShowWindow(GetDlgItem(hwndDlg, IDC_DEVICE_LBL), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_DEVICE_LBL2), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_EDIT2), show);
        ShowWindow(GetDlgItem(hwndDlg, IDC_EDIT3), show);
      }  
      else if (LOWORD(wParam) == IDC_BUTTON2)
      {
        char buf[128];
        GetDlgItemText(hwndDlg, IDC_EDIT1, buf, sizeof(buf));
        int recvport=atoi(buf);
        if (recvport)
        {
          DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_OSC_LISTEN), hwndDlg, OscListenProc, (LPARAM)recvport);
        }
      }
      else if (LOWORD(wParam) == IDC_COMBO1 && HIWORD(wParam) == CBN_SELCHANGE)
      {
        int a=SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_GETCURSEL, 0, 0);
        if (!a)
        {
          s_cfgfn[0]=0;
        }
        else if (a < s_patterncfgs.GetSize()+1) 
        {
          lstrcpyn(s_cfgfn, s_patterncfgs.Get(a-1), sizeof(s_cfgfn));
          
          // validate the cfg file
          char errbuf[512];
          if (s_cfgfn[0] && !LoadCfgFile(s_cfgfn, 0, errbuf,sizeof(errbuf)))
          {
            if (!errbuf[0]) snprintf(errbuf, sizeof(errbuf), __LOCALIZE_VERFMT("Warning: possible error parsing config file \"%s\"","csurf_osc"), s_cfgfn);
            MessageBox(hwndDlg, errbuf, __LOCALIZE("OSC Config File Warning","csurf_osc"), MB_OK);
          }
        }
        else if (a == s_patterncfgs.GetSize()+1)
        {
          SendMessage(hwndDlg, WM_USER+100, 0, 0);
        }
        else if (a == s_patterncfgs.GetSize()+2)
        {
          const char* dir=GetOscCfgDir();
          ShellExecute(hwndDlg, "open", dir, "", dir, SW_SHOW);
          SendMessage(hwndDlg, WM_USER+100, 0, 0);
        }
      }
    return 0;

    case WM_DESTROY:
      s_patterncfgs.Empty(true, free);
    return 0;

    case WM_USER+100: // refresh pattern config file list
    {
      s_patterncfgs.Empty(true, free);
      
      const char* dir=GetOscCfgDir();
      WDL_DirScan ds;
      if (!ds.First(dir))
      {
        do
        {
          const char* fn=ds.GetCurrentFN();
          int len=strlen(fn);
          int extlen=strlen(OSC_EXT);
          if (len > extlen && !stricmp(fn+len-extlen, OSC_EXT) && strnicmp(fn, "default.", strlen("default.")))
          {
            char* p=s_patterncfgs.Add(strdup(fn));
            p += len-extlen;
            *p=0;
          }
        } while (!ds.Next());
      }

      SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
      SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("Default","csurf_osc"));

      int cursel=0;
      WDL_String tmp;
      int i;
      for (i=0; i < s_patterncfgs.GetSize(); ++i)
      {
        const char* p=s_patterncfgs.Get(i);
        int a=SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)p);
        if (!stricmp(s_cfgfn, p)) cursel=a;
      }

      SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("(refresh list)","csurf_osc"));
      SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("(open config directory)","csurf_osc"));
      SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_SETCURSEL, cursel, 0);
    }
    return 0;

    case WM_USER+1024:
      if (wParam > 1 && lParam)
      {
        char name[512];
        GetDlgItemText(hwndDlg, IDC_EDIT4, name, sizeof(name));

        const int sel = (int)SendDlgItemMessage(hwndDlg,IDC_COMBO4,CB_GETCURSEL,0,0);

        int flags=0;

        if (!(sel&1)) flags|=3;
        else if (sel == 1) flags |= 2;
        else if (sel == 3) flags |= 1;

        if (IsDlgButtonChecked(hwndDlg, IDC_CHECK3)) flags |= 4;

        char buf[512];
        GetDlgItemText(hwndDlg, IDC_EDIT1, buf, sizeof(buf));
        int recvport=sel == 0 ? 0 : atoi(buf);
        
        char sendip[512];
        GetDlgItemText(hwndDlg, IDC_EDIT2, sendip, sizeof(sendip));
        GetDlgItemText(hwndDlg, IDC_EDIT3, buf, sizeof(buf));
        int sendport=sel == 2 ? 0 : atoi(buf);

        GetDlgItemText(hwndDlg, IDC_EDIT7, buf, sizeof(buf));
        int maxpacketsz=atoi(buf);
        GetDlgItemText(hwndDlg, IDC_EDIT8, buf, sizeof(buf));
        int sendsleep=atoi(buf);

        WDL_String str;
        PackCfg(&str, name, flags, recvport, sendip, sendport, maxpacketsz, sendsleep, s_cfgfn);
        lstrcpyn((char*)lParam, str.Get(), wParam);
      }
    return 0;
  }

  return 0;
}

static HWND configFunc(const char* typestr, HWND parent, const char* cfgstr)
{
  return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_OSC), parent, dlgProc,(LPARAM)cfgstr);
}

reaper_csurf_reg_t csurf_osc_reg = 
{
  "OSC",
  // !WANT_LOCALIZE_STRINGS_BEGIN:csurf_type
  "OSC (Open Sound Control)",
  // !WANT_LOCALIZE_STRINGS_END
  createFunc,
  configFunc,
};
