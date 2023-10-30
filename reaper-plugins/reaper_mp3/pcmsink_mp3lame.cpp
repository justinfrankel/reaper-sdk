#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#endif
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#include "../../WDL/wdlcstring.h"

extern void (*gOnMallocFailPtr)(int);
#ifndef WDL_HEAPBUF_ONMALLOCFAIL
#define WDL_HEAPBUF_ONMALLOCFAIL(x) if (gOnMallocFailPtr) gOnMallocFailPtr(x);
#endif

#include "../../WDL/lameencdec.h"

#include "resource.h"

#include "../reaper_plugin.h"
#include "../localize.h"

#include "../../WDL/win32_utf8.c"

#include "../../WDL/lineparse.h"
#include "../../WDL/wdlstring.h"
extern void (*update_disk_counters)(int read, int write);
#ifndef WDL_FILEWRITE_ON_ERROR
#error WDL_FILEWRITE_ON_ERROR not defined
#endif
#include "../../WDL/filewrite.h"


extern REAPER_PeakBuild_Interface *(*PeakBuild_CreateEx)(PCM_source *src, const char *fn, int srate, int nch, int flags);
extern const char *(*get_ini_file)();
extern const char *(*GetResourcePath)();
extern const char *(*GetExePath)();
extern const char *(*EnumCurrentSinkMetadata)(int cnt, const char **id);
extern REAPER_Resample_Interface *(*Resampler_Create)();
extern void (*resolve_fn)(const char *in, char *out, int outlen);

extern HWND g_main_hwnd;

struct ID3RawTag;
int PackID3Chunk(WDL_HeapBuf *hb, WDL_StringKeyedArray<char*> *metadata,
  bool want_embed_otherschemes, int *ixml_lenwritten, int ixml_padtolen,
  WDL_PtrList<ID3RawTag> *rawtags=NULL);
int ArrayToMetadata(const char **metadata_arr, WDL_StringKeyedArray<char*> *metadata);


typedef enum 
{
	LQP_NOPRESET=-1,

	// QUALITY PRESETS
	LQP_NORMAL_QUALITY		= 0,
	LQP_LOW_QUALITY			= 1,
	LQP_HIGH_QUALITY		= 2,
	LQP_VOICE_QUALITY		= 3,
	LQP_R3MIX				= 4,
	LQP_VERYHIGH_QUALITY	= 5,
	LQP_STANDARD			= 6,
	LQP_FAST_STANDARD		= 7,
	LQP_EXTREME				= 8,
	LQP_FAST_EXTREME		= 9,
	LQP_INSANE				= 10,
	LQP_ABR					= 11,
	LQP_CBR					= 12,
	LQP_MEDIUM				= 13,
	LQP_FAST_MEDIUM			= 14, 

	// NEW PRESET VALUES
	LQP_PHONE	=1000,
	LQP_SW		=2000,
	LQP_AM		=3000,
	LQP_FM		=4000,
	LQP_VOICE	=5000,
	LQP_RADIO	=6000,
	LQP_TAPE	=7000,
	LQP_HIFI	=8000,
	LQP_CD		=9000,
	LQP_STUDIO	=10000

} LAME_QUALTIY_PRESET;

extern HINSTANCE g_hInst;
#define WIN32_FILE_IO


static void FixSampleRate(int &sr)
{
  if (sr <= 8000) sr=8000;
  else if (sr <= 11025) sr=11025;
  else if (sr <= 12000) sr=12000;
  else if (sr <= 16000) sr=16000;
  else if (sr <= 22050) sr=22050;
  else if (sr <= 24000) sr=24000;
  else if (sr <= 32000) sr=32000;
  else if (sr <= 44100 || sr == 88200 || sr == 176400 || sr == 352800) sr=44100;
  else sr=48000;
}

#define DISABLE_JOINT_STEREO 1000
#define STEREO_JSMASK 0xffff
#define ENABLE_REPLAY_GAIN 0x40000
#define STEREO_REPLAY_GAIN_MASK (0xf0000)

class PCM_sink_mp3lame : public PCM_sink
{
  public:
    static HWND showConfig(void *cfgdata, int cfgdata_l, HWND parent);

    PCM_sink_mp3lame(const char *fn, void *cfgdata, int cfgdata_l, int nch, int srate, bool buildpeaks)
    {
        m_peakbuild=0;
        m_bitrate = 128;
        m_stereomode = 0;
        m_quality = 2;
        m_vbrmethod = -1; //no vbr
        m_vbrq = 2;
        m_abr = 128;
        m_vbrmax = 320;

        if (cfgdata_l >= 32 && *((int *)cfgdata) == REAPER_FOURCC('m','p','3','l'))
        {
          m_bitrate=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[0]);
          m_stereomode=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[1]);
          m_quality=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[2]);
          m_vbrmethod=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[3]);
          m_vbrq=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[4]);
          m_vbrmax=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[5]);
          m_abr=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[6]);
        }
        if (m_quality >= 10) m_quality=0;
        else if (m_quality<0) m_quality=2;

        m_resampler_srate_in=0;
        m_resampler=NULL;

        int srate_in=srate;
        FixSampleRate(srate);
        if (srate != srate_in)
        {
          m_resampler_srate_in=srate_in;
          m_resampler=Resampler_Create();
          if (WDL_NORMALLY(m_resampler))
          {
            m_resampler->SetRates(m_resampler_srate_in, srate);
            m_resampler->Extended(RESAMPLE_EXT_SETFEEDMODE, (void*)(INT_PTR)1, 0, 0);
          }
        }

        m_nch=nch>1?2:1;
        const int rpgain = (m_stereomode&STEREO_REPLAY_GAIN_MASK) == ENABLE_REPLAY_GAIN;

        if (m_nch == 1) m_stereomode = 3; // BE_MP3_MODE_MONO
        else if ((m_stereomode&STEREO_JSMASK) == DISABLE_JOINT_STEREO) m_stereomode = 0; // force stereo
        else m_stereomode = 1; // BE_MP3_MODE_JSTEREO

        m_srate=srate;
        m_lensamples=0;
        m_filesize=0;
        m_fn.Set(fn);
        m_enc=0;

        m_fh=new WDL_FileWrite(fn,0);
        if (m_fh && !m_fh->IsOpen())
        {
          delete m_fh;
          m_fh=0;
        }
        if (m_fh) 
        {
          WDL_StringKeyedArray<char*> metadata(false, WDL_StringKeyedArray<char*>::freecharptr);
          if (EnumCurrentSinkMetadata)
          {
            int cnt=0;
            const char *key=NULL, *val=NULL;
            while ((val=EnumCurrentSinkMetadata(cnt++, &key)))
            {
              if (key && val && val[0])
              {
                metadata.AddUnsorted(key, strdup(val));
              }
            }
            metadata.Resort();

            const char *picfn=metadata.Get("ID3:APIC_FILE");
            if (picfn && picfn[0])
            {
              char resolved_picfn[2048];
              resolved_picfn[0]=0;
              resolve_fn(picfn, resolved_picfn, sizeof(resolved_picfn));
              if (resolved_picfn[0] && strcmp(picfn, resolved_picfn))
              {
                metadata.Insert("ID3:APIC_FILE", strdup(resolved_picfn));
              }
            }
          }
          m_enc=new LameEncoder(srate, nch, m_bitrate,
            m_stereomode, m_quality, m_vbrmethod,
            m_vbrq, m_vbrmax, m_abr, rpgain, &metadata);
        }

        if (m_enc)
          m_enc->SetVBRFilename(m_fn.Get());

        if (buildpeaks && m_enc)
        {
          m_peakbuild=PeakBuild_CreateEx(NULL,fn,m_srate,m_nch,1);
        }

    }

    bool IsOpen()
    {
      return m_enc && m_fh;
    }

    ~PCM_sink_mp3lame()
    {
      if (IsOpen())
      {
        if (m_resampler)
        {
          const double lat=m_resampler->GetCurrentLatency();
          int len=(int) (lat*m_srate);

          // we are in input-fed mode
          ReaSample *in=NULL;
          len = m_resampler->ResamplePrepare(len, m_nch, &in);

          ReaSample *out=m_resampler_buf.ResizeOK(len*m_nch);
          len = WDL_NORMALLY(out) ? m_resampler->ResampleOut(out, 0, len, m_nch) : 0;

          if (len > 0)
          {
            if (m_peakbuild)
            {
              ReaSample *chptrs[REAPER_MAX_CHANNELS];
              for (int c=0; c < m_nch; ++c) chptrs[c]=out+c;
              m_peakbuild->ProcessSamples(chptrs, len, m_nch, 0, m_nch);
            }

            float *spls=m_inbuf.Resize(len*m_nch);
            for (int i=0; i < len*m_nch; ++i) spls[i]=out[i];
            m_lensamples += len;
            m_enc->Encode(spls, len, 1);
            FlushOut();
          }
        }
        m_enc->Encode(NULL,0,1);
        FlushOut();
      }
      delete m_fh;
      delete m_enc; // be sure to delete m_enc AFTER m_fh, to ensure it can write the vbr tag etc
      delete m_peakbuild;
      delete m_resampler;
    }

    const char *GetFileName() { return m_fn.Get(); }
    int GetNumChannels() { return m_nch; } // return number of channels
    double GetLength() { return m_lensamples / (double) m_srate; } // length in seconds, so far
    INT64 GetFileSize()
    {
      return m_filesize;
    }
    int GetLastSecondPeaks(int sz, ReaSample *buf)
    {
      if (m_peakbuild)
        return m_peakbuild->GetLastSecondPeaks(sz,buf);
      return 0;
    }
    void GetPeakInfo(PCM_source_peaktransfer_t *block)
    {
      if (m_peakbuild) m_peakbuild->GetPeakInfo(block);
      else block->peaks_out=0;
    }

    void GetOutputInfoString(char *buf, int buflen);

    void WriteMIDI(MIDI_eventlist *events, int len, double samplerate) { }
    void WriteDoubles(ReaSample **samples, int len, int nch, int offset, int spacing)
    {
      ReaSample *tmpptrs[2];
      tmpptrs[0] = samples[0]+offset;
      tmpptrs[1] = nch > 1 ? samples[1]+offset : samples[0]+offset;

      if (m_resampler)
      {
        ReaSample *resamplebuf=NULL;
        // we are in input-fed mode
        int n=m_resampler->ResamplePrepare(len, m_nch, &resamplebuf);
        WDL_ASSERT(n == len);
        for (int c=0; c < m_nch; ++c)
        {
          ReaSample *in=tmpptrs[c];
          ReaSample *out=resamplebuf+c;
          for (int i=0; i < n; ++i)
          {
            *out=*in;
            out += m_nch;
            in += spacing;
          }
        }

        int max_out=(int)ceil(n*(double)m_srate/(double)m_resampler_srate_in);
        ReaSample *outbuf=m_resampler_buf.ResizeOK(max_out*m_nch);
        len=WDL_NORMALLY(outbuf) ? m_resampler->ResampleOut(outbuf, n, max_out, m_nch) : 0;

        spacing=m_nch;
        tmpptrs[0] = outbuf;
        tmpptrs[1] = m_nch > 1 ? outbuf+1 : outbuf;
      }

      if (m_peakbuild) m_peakbuild->ProcessSamples(tmpptrs, len, m_nch, 0, spacing);

      float *tmpbuf=m_inbuf.Resize(len*m_nch,false);
      for (int c=0; c < m_nch; ++c)
      {
        ReaSample *in=tmpptrs[c];
        float *out=tmpbuf+c;
        for (int i=0; i < len; ++i)
        {
          *out=*in;
          out += m_nch;
          in += spacing;
        }
      }

      m_lensamples += len;
      m_enc->Encode(tmpbuf, len, 1);
      FlushOut();
    }

    void FlushOut()
    {
      int l=m_enc->outqueue.Available();
      if (l>0)
      {
        m_fh->Write(m_enc->outqueue.Get(),l);
        if (update_disk_counters) update_disk_counters(0,l);
        m_filesize+=l;
        m_enc->outqueue.Advance(l);
        m_enc->outqueue.Compact();
      }
    }
    int Extended(int call, void *parm1, void *parm2, void *parm3) 
    {
      if (call == PCM_SINK_EXT_DONE)
      {
        if (parm2 && WDL_NORMALLY(m_enc && m_fh))
        {
          WDL_StringKeyedArray<char*> updated_metadata(true, WDL_StringKeyedArray<char*>::freecharptr);
          ArrayToMetadata((const char**)parm2, &updated_metadata);

          WDL_HeapBuf hb;
          int ixmllen=m_enc->GetIXMLLen();
          if (PackID3Chunk(&hb, &updated_metadata, true, NULL, ixmllen) &&
            WDL_NORMALLY(hb.GetSize() == m_enc->GetID3Len()))
          {
            WDL_INT64 pos=m_fh->GetPosition();
            m_fh->SetPosition(0);
            m_fh->Write(hb.Get(), hb.GetSize());
            m_fh->SetPosition(pos);
            return 1;
          }
        }
        return 0;
      }

      if (call == PCM_SINK_EXT_VERIFYFMT)
      {
        if (parm1 && parm2)
        {
          if (*(int *)parm2 <= 1) *(int *)parm2=1;
          else *(int *)parm2 = 2;
          return 1;
        }
      }
      return 0;
    }


 private:
    WDL_FileWrite *m_fh;
    int m_bitrate;
    int m_vbrq, m_abr, m_vbrmax, m_quality, m_stereomode, m_vbrmethod;

    WDL_TypedBuf<float> m_inbuf;
    int m_nch,m_srate;
    INT64 m_filesize;
    INT64 m_lensamples;
    WDL_String m_fn;
    LameEncoder *m_enc;
    REAPER_PeakBuild_Interface *m_peakbuild;

    int m_resampler_srate_in;
    REAPER_Resample_Interface *m_resampler;
    WDL_TypedBuf<ReaSample> m_resampler_buf;
};

static unsigned int GetFmt(const char **desc) 
{
  if (desc) *desc=__LOCALIZE("MP3 (encoder by LAME project)","mp3");
  return REAPER_FOURCC('m','p','3','l');
}

static const char *GetExtension(const void *cfg, int cfg_l)
{
  if (cfg_l >= 4 && *((int *)cfg) == REAPER_FOURCC('m','p','3','l')) return "mp3";
  return NULL;
}


static int s_bitrates[] = { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
static int s_numbitrates = sizeof(s_bitrates)/sizeof(s_bitrates[0]);


static int QualityToApproxBitrate(int qual)
{
  // http://lame.sourceforge.net/lame_ui_example.php 
  int br = 64+(256-64)*qual/9;
  int i;
  for (i = 0; i < s_numbitrates; ++i)
  {
    if (s_bitrates[i] >= br) return s_bitrates[i];
  }
  return s_bitrates[s_numbitrates-1];
}

static int BitrateToApproxQuality(int br)
{
  // http://lame.sourceforge.net/lame_ui_example.php 
  int qual = 9*(br-64)/(256-64);
  if (qual < 0) return 0;
  if (qual > 9) return 9;
  return qual;
}


enum { MODE_MAX=0, MODE_VBR, MODE_ABR, MODE_CBR };

/*
"quality" stored in configuration file:

  saved by 4.x-5.15: -1 always
  saved by 5.16+: 10=max, 12=cbr, 11=abr, -1=vbr. parameter for lame_enc quality will be determined based on bitrate/method

*/


static void OldToNewConfig(int br, int vbrq, int abr, int vbrmax, int quality, int vbrmethod, 
  int* newmode, int* newqual, int* newbr)
{
  if (quality == 10)
  {
    *newmode = MODE_MAX;
  }
  else if (vbrmethod < 0)
  {
    *newmode = MODE_CBR;
  }
  else if (vbrmethod == 4) // VBR_METHOD_ABR
  {
    *newmode = MODE_ABR;
  }
  else 
  {
    *newmode = MODE_VBR;
  }

  if (vbrq < 0) vbrq=0;
  else if (vbrq > 9) vbrq = 9;
  *newqual = vbrq;

  if (*newmode == MODE_ABR)
  {
      *newbr = abr;
  }
  else
  {
    if (br < s_bitrates[0]) br = s_bitrates[0];
    else if (br > s_bitrates[s_numbitrates-1]) br = s_bitrates[s_numbitrates-1];
    *newbr = br;
  }
}

static void NewToOldConfig(int newmode, int newqual, int newbr, int* br, int* vbrq, int* abr, int* vbrmax, int* vbrmethod)
{
  *abr=0;
  *vbrq = 4;
  *br = 32;
  *vbrmax = 320;

  if (newmode == MODE_MAX)
  {
    *br = 320;
    *vbrmethod = -1;  // VBR_METHOD_NONE
  }
  else if (newmode == MODE_ABR)
  {
    *abr = newbr;
    *vbrmethod = 4; // VBR_METHOD_ABR
  }
  else if (newmode == MODE_CBR)
  {
    *br = *vbrmax = newbr;
    *vbrmethod = -1; // VBR_METHOD_NONE
  }
  else // MODE_VBR
  {
    *vbrq = newqual;
    *vbrmethod = 0; // VBR_METHOD_DEFAULT, this gets overridden anyway in lamencdec.cpp
  }
}


void PCM_sink_mp3lame::GetOutputInfoString(char *buf, int buflen)
{
  int newmode, newqual, newbr;
  OldToNewConfig(m_bitrate, m_vbrq, m_abr, m_vbrmax, m_quality, m_vbrmethod, 
    &newmode, &newqual, &newbr);

  snprintf(buf,buflen,__LOCALIZE_VERFMT("MP3 (lame_enc) %dHz %dch","mp3"),m_srate, m_enc?m_enc->GetNumChannels():m_nch);
  lstrcatn(buf," ",buflen);
  if (newmode == MODE_MAX || newmode == MODE_CBR)
  {
    snprintf_append(buf,buflen,__LOCALIZE_VERFMT("%dkbps CBR","mp3"),newbr);
  }
  else if (newmode == MODE_ABR)
  {
    snprintf_append(buf,buflen,__LOCALIZE_VERFMT("%dkbps ABR","mp3"),newbr);
  }
  else
  {
    lstrcatn(buf,__LOCALIZE("VBR","mp3"),buflen);
  }
  snprintf_append(buf,buflen," q=%d%s",m_quality,m_stereomode==0 ? " [JS=0]":"");
}


static int LoadDefaultConfig(void **data, const char *desc)
{
  static WDL_HeapBuf m_hb;
  const char *fn=get_ini_file();
  int l=GetPrivateProfileInt(desc,"default_size",0,fn);
  if (l<1) return 0;
  
  if (GetPrivateProfileStruct(desc,"default",m_hb.Resize(l),l,fn))
  {
    *data = m_hb.Get();
    return l;
  }
  return 0;
}
static void SaveDefaultConfig(void *data, int l, const char *desc)
{
  const char *fn=get_ini_file();
  char buf[64];
  sprintf(buf,"%d",l);
  WritePrivateProfileString(desc,"default_size",buf,fn);
  WritePrivateProfileStruct(desc,"default",data,l,fn);
}

WDL_DLGRET wavecfgDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        void *cfgdata=((void **)lParam)[0];
        int cfgdata_l=(int)(INT_PTR)((void**)lParam)[1];
        int br=128, vbrq=2, abr=128, vbrmax=320, quality=2, vbrmethod=-1, stereomode=0;
        if (cfgdata_l < 32 || *((int *)cfgdata) != REAPER_FOURCC('m','p','3','l'))
        {
          cfgdata_l=LoadDefaultConfig(&cfgdata,"mp3 encoder defaults");
        }
        if (cfgdata_l >= 32 && *((int *)cfgdata) == REAPER_FOURCC('m','p','3','l'))
        {
          br=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[0]);
          stereomode=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[1]);
          quality=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[2]);
          vbrmethod=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[3]);
          vbrq=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[4]);
          vbrmax=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[5]);
          abr=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[6]);
        }

      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg,IDC_MODE));
      SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Maximum bitrate/quality","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Target quality  (VBR)","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Target bitrate  (ABR)","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Constant bitrate  (CBR)","mp3dec_DLG_155"));

      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg,IDC_COMBO1));
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Maximum q=0 (slow)","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Better q=2 (recommended)","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Normal q=3","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Fast encode q=5","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Faster encode q=7","mp3dec_DLG_155"));
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Fastest encode q=9","mp3dec_DLG_155"));

      if ((stereomode&STEREO_JSMASK) == DISABLE_JOINT_STEREO)
        CheckDlgButton(hwndDlg,IDC_STEREO_MODE,BST_CHECKED);
      if ((stereomode&STEREO_REPLAY_GAIN_MASK) == ENABLE_REPLAY_GAIN)
        CheckDlgButton(hwndDlg,IDC_REPLAY_GAIN,BST_CHECKED);

      int idx = 1;
      if (quality == 10) idx=1; // default to better if last was maximum bitrate/quality
      else if (quality < 2 || quality > 10) idx=0;
      else if (quality < 3) idx=1;
      else if (quality < 5) idx=2;
      else if (quality < 7) idx=3;
      else if (quality < 9) idx=4;
      else idx=5;
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_SETCURSEL,idx,0);
      
      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg,IDC_BITRATE));
      int i;
      char buf[256];
      for (i = 0; i < s_numbitrates; ++i)
      {         
        sprintf(buf, "%d", s_bitrates[i]);
        strcat(buf," ");
        strcat(buf,__LOCALIZE("kbps","mp3dec_DLG_155"));
        SendDlgItemMessage(hwndDlg,IDC_BITRATE,CB_ADDSTRING,0,(LPARAM)buf);
      }

      WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg,IDC_QUALITY));
      for (i = 10; i <= 100; i += 10)
      {
        sprintf(buf, "%d", i);
        if (i == 10)
        {
          strcat(buf," ");
          strcat(buf,__LOCALIZE("(worst)","mp3dec_DLG_155"));
        }
        else if (i == 100)
        {
          strcat(buf," ");
          strcat(buf,__LOCALIZE("(best)","mp3dec_DLG_155"));
        }
        SendDlgItemMessage(hwndDlg, IDC_QUALITY, CB_ADDSTRING, 0, (LPARAM)buf);
      }

      int newmode, newqual, newbr;
      OldToNewConfig(br, vbrq, abr, vbrmax, quality, vbrmethod, &newmode, &newqual, &newbr);

      SendDlgItemMessage(hwndDlg, IDC_MODE, CB_SETCURSEL, newmode, 0);
      SendDlgItemMessage(hwndDlg, IDC_QUALITY, CB_SETCURSEL, 9-newqual, 0);
      SendMessage(hwndDlg, WM_USER+200, newbr, 0);
      SendMessage(hwndDlg, WM_USER + 201, 0, 0);
    }
    // fall through

    case WM_USER+1000:  // refresh
    {
      int mode = SendDlgItemMessage(hwndDlg, IDC_MODE, CB_GETCURSEL, 0, 0);
      bool en_qual=true;
      bool en_br=true;     

      if (mode == MODE_MAX)
      {
        SendDlgItemMessage(hwndDlg, IDC_QUALITY, CB_SETCURSEL, 9, 0);
        SendMessage(hwndDlg, WM_USER+200, s_bitrates[s_numbitrates-1], 0);
        en_br=en_qual=false;
      }
      else if (mode == MODE_VBR)
      {
        int qual = SendDlgItemMessage(hwndDlg, IDC_QUALITY, CB_GETCURSEL, 0, 0);
        int br = QualityToApproxBitrate(qual);
        SendMessage(hwndDlg, WM_USER+200, br, 0);
        en_br=false;
      }
      else if (mode == MODE_ABR || mode == MODE_CBR)
      {      
        int br = SendDlgItemMessage(hwndDlg, IDC_BITRATE, CB_GETCURSEL, 0, 0);
        if (br < 0 || br >= s_numbitrates) br = 128;
        else br = s_bitrates[br];
        int qual = BitrateToApproxQuality(br);        
        SendDlgItemMessage(hwndDlg, IDC_QUALITY, CB_SETCURSEL, qual, 0);
        en_qual=false;
      }

      ShowWindow(GetDlgItem(hwndDlg,IDC_COMBO1),mode == MODE_MAX ? SW_HIDE : SW_SHOWNA);
      ShowWindow(GetDlgItem(hwndDlg,IDC_STEREO_MODE),mode == MODE_MAX ? SW_HIDE : SW_SHOWNA);

      EnableWindow(GetDlgItem(hwndDlg, IDC_QUALITY_LBL), en_qual);
      EnableWindow(GetDlgItem(hwndDlg, IDC_QUALITY), en_qual);
      EnableWindow(GetDlgItem(hwndDlg, IDC_QUALITY_APPROX), en_qual);
      ShowWindow(GetDlgItem(hwndDlg, IDC_QUALITY_APPROX), (mode == MODE_MAX || en_qual ? SW_HIDE : SW_SHOW));

      EnableWindow(GetDlgItem(hwndDlg, IDC_BITRATE_LBL), en_br);
      EnableWindow(GetDlgItem(hwndDlg, IDC_BITRATE), en_br);
      EnableWindow(GetDlgItem(hwndDlg, IDC_BITRATE_APPROX), en_br);
      ShowWindow(GetDlgItem(hwndDlg, IDC_BITRATE_APPROX), (mode == MODE_MAX || en_br ? SW_HIDE : SW_SHOW));
    }
    return 0;
    case  WM_USER + 201:
    {
      const char *p = LameEncoder::GetInfo();
      if (!p) p = __LOCALIZE("LAME unavailable","mp3dec_DLG_155");
      SetDlgItemText(hwndDlg, IDC_LAMEVER, p);
    }
    return 0;
    case WM_USER+200: // set bitrate
    {
      int i;
      for (i = 0; i < s_numbitrates; ++i)
      {
        if (s_bitrates[i] >= (int)wParam) break;
      }
      if (i >= s_numbitrates) i = s_numbitrates-1;
      SendDlgItemMessage(hwndDlg, IDC_BITRATE, CB_SETCURSEL, i, 0);
    }
    return 0;

    case WM_USER+1024:  // apply
      if (wParam)
      {
        *(int*)wParam = 32;
      }
      if (lParam)
      {
        int newmode = SendDlgItemMessage(hwndDlg, IDC_MODE, CB_GETCURSEL, 0, 0);
        int newqual = SendDlgItemMessage(hwndDlg, IDC_QUALITY, CB_GETCURSEL, 0, 0);
        newqual = 9-newqual;
      
        int newbr = SendDlgItemMessage(hwndDlg, IDC_BITRATE, CB_GETCURSEL, 0, 0);
        if (newbr < 0 || newbr >= s_numbitrates) newbr = 128;
        else newbr = s_bitrates[newbr];

        int br, vbrq, abr, vbrmax, quality = 2, vbrmethod;
        NewToOldConfig(newmode, newqual, newbr, &br, &vbrq, &abr, &vbrmax, &vbrmethod);

        int stereomode = 0;
        if (newmode == MODE_MAX)
        {
          quality = 10;
        }
        else
        {
          int a = (int) SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_GETCURSEL,0,0);
          if (a == 0) quality=0;
          else if (a==1) quality=2;
          else if (a==2) quality=3;
          else if (a==3) quality=5;
          else if (a==4) quality=7;
          else if (a==5) quality=9;

          if (IsDlgButtonChecked(hwndDlg,IDC_STEREO_MODE)) 
            stereomode = DISABLE_JOINT_STEREO;
        }

        if (IsDlgButtonChecked(hwndDlg,IDC_REPLAY_GAIN)) 
          stereomode |= ENABLE_REPLAY_GAIN;


        int* p = (int*)lParam;
        p[0] = REAPER_FOURCC('m','p','3','l');
        p[1] = REAPER_MAKELEINT(br);
        p[2] = REAPER_MAKELEINT(stereomode);
        p[3] = REAPER_MAKELEINT(quality);
        p[4] = REAPER_MAKELEINT(vbrmethod);
        p[5] = REAPER_MAKELEINT(vbrq);
        p[6] = REAPER_MAKELEINT(vbrmax);
        p[7] = REAPER_MAKELEINT(abr);
    }
    return 0;

    case WM_COMMAND:
      if (HIWORD(wParam) == CBN_SELCHANGE)
      {
        SendMessage(hwndDlg, WM_USER+1000, 0, 0);
      }
    return 0;

    case WM_DESTROY:
      {
        char buf[32];
        SendMessage(hwndDlg,WM_USER+1024,0,(LPARAM)buf);
        SaveDefaultConfig(buf,32,"mp3 encoder defaults");
      }
    return 0;

  }
  return 0;
}


static HWND ShowConfig(const void *cfg, int cfg_l, HWND parent)
{
  if (cfg_l >= 4 && *((int *)cfg) == REAPER_FOURCC('m','p','3','l')) 
  {
    const void *x[2]={cfg,(void *)(INT_PTR)cfg_l};
    return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_MP3SINK_CFG),parent,wavecfgDlgProc,(LPARAM)x);
  }
  return 0;
}

#ifdef __APPLE__
  #define REAPERAPP "REAPER.app"
#else
#ifdef _WIN32
  #define REAPERAPP "REAPER.exe"
#else // generic
  #define REAPERAPP "REAPER"
#endif
#endif


static PCM_sink *CreateSink(const char *filename, void *cfg, int cfg_l, int nch, int srate, bool buildpeaks)
{
  if (cfg_l >= 4 && *((int *)cfg) == REAPER_FOURCC('m','p','3','l')) 
  {
    PCM_sink_mp3lame *v=new PCM_sink_mp3lame(filename,cfg,cfg_l,nch,srate,buildpeaks);
    if (v->IsOpen()) return v;
    delete v;
  }
  return 0;
}


static int ExtendedSinkInfo(int call, void* parm1, void* parm2, void* parm3)
{
  if (call == PCMSINKEXT_GETFORMATDESC||call==PCMSINKEXT_GETFORMATDATARATE)
  {
    void* cfg = parm1;
    int len = (int)(INT_PTR)parm2;
    if (len<4 || *((int*)cfg) != REAPER_FOURCC('m','p','3','l')) return 0;

    int br = 128;
    int vbrm = -1;
    int abr = 128;
    int stereomode=0;

    char* desc = (char*)parm3;
    if (len >= 32) 
    {
      br=REAPER_MAKELEINT(((int *)(((unsigned char *)cfg)+4))[0]);
      stereomode=REAPER_MAKELEINT(((int *)(((unsigned char *)cfg)+4))[1]);
      vbrm=REAPER_MAKELEINT(((int *)(((unsigned char *)cfg)+4))[3]);
      abr=REAPER_MAKELEINT(((int *)(((unsigned char *)cfg)+4))[6]);     
    }   
    if (call==PCMSINKEXT_GETFORMATDATARATE)
    {
      if (vbrm >= 0) return (abr*1000*3/2)/8;
      
      return (br*1000)/8;
    }

    if (vbrm < 0) sprintf(desc,__LOCALIZE_VERFMT("%dkbps MP3","mp3"),br);
    else sprintf(desc,"%s",__LOCALIZE("VBR MP3","mp3"));
    if ((stereomode&STEREO_JSMASK)==DISABLE_JOINT_STEREO) strcat(desc," [JS=0]");
    return 1;
  }



  return 0;
}

static pcmsink_register_ext_t mySinkRegStruct={{GetFmt,GetExtension,ShowConfig,CreateSink},ExtendedSinkInfo};

void register_sink(reaper_plugin_info_t *rec)
{
  char buf[1024];   
  GetModuleFileName(NULL,buf,sizeof(buf));
#ifdef __APPLE__
  lstrcatn(buf,"/Contents/Plugins",sizeof(buf));
#else
  WDL_remove_filepart(buf);
  lstrcatn(buf,WDL_DIRCHAR_STR "Plugins",sizeof(buf));
#endif
  LameEncoder::InitDLL(buf);
  if (LameEncoder::CheckDLL())
  {
    if (!rec->Register("pcmsink_ext",&mySinkRegStruct))
      rec->Register("pcmsink",&mySinkRegStruct);
  }
}
