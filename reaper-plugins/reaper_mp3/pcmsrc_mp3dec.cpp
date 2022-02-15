#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#define LOCALIZE_IMPORT_PREFIX "mp3dec_" // causes main.h to include localize-import.h
#include "main.h"

#include "resource.h"
#include "mp3dec.h"

void (*gOnMallocFailPtr)(int);
// todo: lame gapless support?

//#include "main.h"
//#include "resource.h"
#include "../reaper_plugin.h"
#include "../../WDL/lineparse.h"
#include "../../WDL/wdlcstring.h"
#include "../../WDL/wdlstring.h"
#include "../../WDL/ptrlist.h"
#include "../../WDL/assocarray.h"
#include "../../WDL/mutex.h"

#include "../../WDL/fileread.h"


#define WDL_WIN32_UTF8_NO_UI_IMPL
#define WDL_WIN32_UTF8_IMPL static
#include "../../WDL/win32_utf8.c"


REAPER_PLUGIN_HINSTANCE g_hInst;

HWND g_main_hwnd;

REAPER_Resample_Interface *(*Resampler_Create)();
void (*format_timestr)(double tpos, char *buf, int buflen);
REAPER_PeakGet_Interface *(*PeakGet_Create)(const char *fn, int srate, int nch);
REAPER_PeakBuild_Interface *(*PeakBuild_CreateEx)(PCM_source *src, const char *fn, int srate, int nch, int flags);
void (*resolve_fn)(const char *in, char *out, int outlen);
void (*relative_fn)(const char *in, char *out, int outlen);
void (*GetPeakFileName)(const char *fn, char *buf, int bufmax);
void (*GetPeakFileNameEx2)(const char *fn, char *buf, int bufmax, bool forWrite, const char *ext);
void (*update_disk_counters)(int read, int write);
const char *(*get_ini_file)();
const char *(*GetResourcePath)();
const char *(*GetExePath)();
void (*GetPreferredDiskReadMode)(int *mode, int *nb, int *bs);
void (*GetPreferredDiskReadModePeak)(int *mode, int *nb, int *bs);
void (*HiresPeaksFromSource)(PCM_source *src, PCM_source_peaktransfer_t *block);
const char *(*EnumCurrentSinkMetadata)(int cnt, const char **id);
void (*__mergesort)(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *), void *tmpspace);

#define SOURCE_TYPE "MP3"

#define POOLEDSRC_ALLOWPOOLMETASAVE 2

#include "mp3_index.h"
#include "../tag.h"
#include "../metadata.h"

int PackID3Chunk(WDL_HeapBuf *hb, WDL_StringKeyedArray<char*> *metadata,
  bool want_embed_otherschemes, int *ixml_lenwritten, int ixml_padtolen);
int PackApeChunk(WDL_HeapBuf *hb, WDL_StringKeyedArray<char*> *metadata);

struct ID3Cue
{
  double pos, endpos;
  int type; // &1=region, &2=chapter
  const char *name; // we don't own this
};

static int _cuecmp(const void *_a, const void *_b)
{
  ID3Cue* a=(ID3Cue*)_a;
  ID3Cue* b=(ID3Cue*)_b;
  if (a->pos < b->pos) return -1;
  if (a->pos > b->pos) return 1;
  return 0;
}

#include "../../WDL/rpool.h"


class PooledDecoderInstance
{
public:
  PooledDecoderInstance() 
  {
    m_resampler=0;
    m_resampler_state=0;
    m_lastpos = -10000000; 
    m_decode_srcsplpos=0;
    m_file=0;
    m_dump_samples=0;
    m_read_pos=0;
    m_need_initial_dump=true;
  }
  ~PooledDecoderInstance() 
  {
    delete m_resampler;
    delete m_file;
  }

  WDL_ResourcePool_ResInfo m_rpoolinfo;

  REAPER_Resample_Interface *m_resampler;
  int m_resampler_state;

  INT64 m_lastpos,m_decode_srcsplpos;
  mp3_decoder m_decoder;    
  unsigned int m_read_pos;
  int m_dump_samples;
  bool m_need_initial_dump;
  WDL_FileRead *m_file;

};


class PooledDecoderMediaInfo_Base
{
public:
  const char *m_err;
  PooledDecoderMediaInfo_Base()
  : m_metadata(false, WDL_StringKeyedArray<char*>::freecharptr)
  {
    m_channel_mode=-1;
    m_err=0;
    memset(&m_syncframeinfo,0,sizeof(m_syncframeinfo));
    m_index=NULL;
    m_lengthsamples=0;
    m_stream_startpos=0;
    m_stream_endpos = 0;
    m_srate=0;
    m_nch=0;
  }
  ~PooledDecoderMediaInfo_Base()
  {
    if (m_index)
    {
      mp3_index::release_index(m_index);
      m_index=0;
    }
  }
  int GetNumChannels() { return m_nch; } // return number of channels
  double GetSampleRate() { return m_srate; }

  INT64 GetLengthSamples(bool wantLatencyAdjust)
  {
    INT64 ls = m_lengthsamples;
    if (wantLatencyAdjust && m_index)
      ls -= m_index->m_start_eatsamples + m_index->m_end_eatsamples;
    return ls;
  }

  double GetLength(bool wantLatencyAdjust)  // length in seconds
  { 
    if (m_srate<1) return 0.0;
    return GetLengthSamples(wantLatencyAdjust) / (double)m_srate;
  }

  double GetPreferredPosition()
  {
    return ReadMetadataPrefPos(&m_metadata, (double)m_srate);
  }

  int GetBitsPerSample() { return 16; }

  bool IsAvailable() { return m_srate>0 && m_nch; }

  PooledDecoderInstance *Open(const char *filename, int peaksMode, bool forceread)
  {
    int mode=1,bs=8192, nb=2;
    if (peaksMode)
    {
      mode=0;
      nb=1;
      bs=8192;
      if (GetPreferredDiskReadModePeak) GetPreferredDiskReadModePeak(&mode,&nb,&bs);
    }
    else
    {
      if (GetPreferredDiskReadMode) GetPreferredDiskReadMode(&mode,&nb,&bs);
    }
    WDL_FileRead *file = new WDL_FileRead(filename,mode,min(bs,32768),min(nb,4));

    if (!file->IsOpen()) 
    {
      m_err=__LOCALIZE("Error opening file","mp3");
      delete file;
      return NULL;
    }

    PooledDecoderInstance *pdi=new PooledDecoderInstance;
    if (!m_index || forceread || !m_srate || 
        !m_nch || !m_syncframeinfo.framesize || m_err) // sync
    {
      m_stream_startpos=0;
      m_stream_endpos = (unsigned int)file->GetSize();

      WDL_INT64 fstart=0, fend=m_stream_endpos;
      if (ReadMediaTags(file, &m_metadata, &fstart, &fend))
      {
        m_stream_startpos=fstart;
        m_stream_endpos=fend;
      }

      file->SetPosition(0);

      if (!m_index)
      {
        m_index = mp3_index::indexFromFilename(filename,file); // skips leading id3v2 tags
      }

      if (m_index && m_index->GetFrameCount())
        m_stream_startpos = m_index->GetStreamStart();

      file->SetPosition(m_stream_startpos);

      pdi->m_read_pos = m_stream_startpos;

      while (!pdi->m_decoder.SyncState())
      {
        if (pdi->m_decoder.queue_bytes_in.Available()<4096)
        {
          char buf[4096];
          int l=m_stream_endpos-pdi->m_read_pos;
          if (l > sizeof(buf)) l=sizeof(buf);
          l=file->Read(buf,l);
          if (!l) break;

          pdi->m_read_pos+=l;
          if (l > 0) pdi->m_decoder.queue_bytes_in.Add(buf,l);
        }
        if (pdi->m_decoder.Run()) break;
      }
      pdi->m_decoder.queue_bytes_in.Compact();

      if (pdi->m_decoder.SyncState())
      {
        m_channel_mode = pdi->m_decoder.GetChannelMode();
        m_srate = pdi->m_decoder.GetSampleRate();
        m_nch = pdi->m_decoder.GetNumChannels();
        if (m_index) m_lengthsamples = (INT64) ((m_index->GetFrameCount()*(double)m_srate) / pdi->m_decoder.GetFrameRate() + 0.5);
        else m_lengthsamples=0;    
        m_syncframeinfo = pdi->m_decoder.m_lastframe;
      }
      else 
      {
        m_err=__LOCALIZE("Error synchronizing to MPEG bitstream","mp3");
        delete pdi;
        delete file;
        return NULL;
      }
    }
    else
    {
      pdi->m_read_pos = 0;
      pdi->m_decoder.m_lastframe = m_syncframeinfo;
    }
    m_err=0;
    pdi->m_file = file;

    return pdi;
  }

#define POOLEDSRC_POOLEDMEDIAINFO_HAS_CLOSE
  void Close()
  {
    if (m_index && m_index->has_file_open()) // do not release index if it was built to memory (regenerating would be slow)
    {
      mp3_index::release_index(m_index);
      m_index=NULL;
    }
  }

  int m_srate,m_nch, m_channel_mode;
  INT64 m_lengthsamples;
  unsigned int m_stream_startpos, m_stream_endpos;
  mp3_index *m_index;
  struct frame m_syncframeinfo;
  WDL_StringKeyedArray<char*> m_metadata;
  WDL_TypedBuf<ID3Cue> m_cues;
};


#define POOLED_PCMSOURCE_CLASSNAME PCM_source_mp3

#define POOLED_PCMSOURCE_EXTRASTUFF_GETLENGTH_PARM (m_adjustLatency)
#define POOLED_PCMSOURCE_EXTRASTUFF_INIT m_adjustLatency=true;
#define POOLED_PCMSOURCE_EXTRASTUFF bool m_adjustLatency;
#define POOLED_PCMSOURCE_EXTRASTUFF_DUPLICATECODE ns->m_adjustLatency = m_adjustLatency; 
#define POOLEDSRC_WANTFPPEAKS 1

#include "../pooled_pcmsource_impl.h"
#include "../metadata.h"

int POOLED_PCMSOURCE_CLASSNAME::PoolExtended(int call, void* parm1, void* parm2, void* parm3)
{
  PCM_SOURCE_EXT_GETINFOSTRING_HANDLER
  if (m_filepool && m_filepool->extraInfo)
  {
    PCM_SOURCE_EXT_GETMETADATA_HANDLER(&m_filepool->extraInfo->m_metadata)
  }

  if (call == PCM_SOURCE_EXT_GETBITRATE && m_filepool && m_filepool->extraInfo && parm1)
  {
    WDL_INT64 datalen=m_filepool->extraInfo->m_stream_endpos-m_filepool->extraInfo->m_stream_startpos;
    WDL_INT64 spllen=m_filepool->extraInfo->GetLengthSamples(m_adjustLatency);
    WDL_INT64 srate=m_filepool->extraInfo->m_srate;
    if (datalen > 0 && spllen > 0 && srate > 0)
    {
      *(double*)parm1=8.0*(double)datalen/(double)(spllen/srate);
    }
    return 1;
  }

  if (call == PCM_SOURCE_EXT_WRITE_METADATA && parm1 && parm2 && m_filepool && m_filepool->extraInfo)
  {
    const char *newfn=(const char*)parm1;
    const char **metadata_arr=(const char**)parm2;
    bool merge=!!parm3;

    WDL_StringKeyedArray<char*> mex_metadata(true, WDL_StringKeyedArray<char*>::freecharptr);
    ArrayToMetadata(metadata_arr, &mex_metadata);

    WDL_StringKeyedArray<char*> metadata(false, WDL_StringKeyedArray<char*>::freecharptr);
    if (merge) CopyMetadata(&m_filepool->extraInfo->m_metadata, &metadata);
    AddMexMetadata(&mex_metadata, &metadata, m_filepool->extraInfo->m_srate);

    const char *fn=GetFileName();
    WDL_FileRead *fr=new WDL_FileRead(fn);
    WDL_FileWrite *fw=new WDL_FileWrite(newfn);
    bool ok = fr->IsOpen() && fw->IsOpen();

    unsigned int spos=m_filepool->extraInfo->m_stream_startpos;
    unsigned int epos=m_filepool->extraInfo->m_stream_endpos;

    WDL_HeapBuf hb;
    if (merge)
    {
      // preserve existing ID3 tags that we don't handle
      WDL_PtrList<ID3RawTag> rawtags;
      if (ok) ok = ReadID3Raw(fr, &rawtags) == spos;
      AddMexID3Raw(&metadata, &rawtags);
      if (ok) WriteID3Raw(fw, &rawtags);
    }
    else
    {
      int id3len=PackID3Chunk(&hb, &metadata, true, NULL, 0);
      if (id3len && ok) ok = fw->Write(hb.Get(), id3len) == id3len;
      hb.Resize(0, false);
    }

    if (ok) ok = !fr->SetPosition(spos) && CopyFileData(fr, fw, epos-spos);

    int apelen=PackApeChunk(&hb, &metadata);
    if (apelen && ok) ok = fw->Write(hb.Get(), apelen) == apelen;

    delete fr;
    delete fw;
    return ok;
  }

  if (call == PCM_SOURCE_EXT_ENUMCUES_EX && parm2 && m_filepool && m_filepool->extraInfo)
  {
    int idx=(int)(INT_PTR)parm1;
    REAPER_cue *cue=(REAPER_cue*)parm2;
    if (cue) memset(cue, 0, sizeof(REAPER_cue));

    if (!m_filepool->extraInfo->m_cues.GetSize())
    {
      const char *evts=m_filepool->extraInfo->m_metadata.Get("ID3:ETCO");
      if (evts && !evts[0]) evts=NULL;
      const char *chap=m_filepool->extraInfo->m_metadata.Get("ID3:CHAP001");
      if (chap && !chap[0]) chap=NULL;
      if (!evts && !chap) return 0;

      if (evts)
      {
        const char *p=evts;
        while (*p)
        {
          const char *t=p;
          while (*p && *p != ':') ++p;
          const char *sep=p;
          while (*p && *p != ',') ++p;
          if (p <= sep || sep <= t) break; // ill formed
          unsigned int ms=atoi(t);
          int type=GetETCOType(sep+1, p-sep);
          if (type >= 0)
          {
            ID3Cue id3_cue={0};
            id3_cue.pos=(double)ms*0.001;
            id3_cue.name=ID3_ETCO_TYPE[type];
            m_filepool->extraInfo->m_cues.Add(id3_cue);
          }
        }
      }

      int i=1;
      while (chap)
      {
        const char *sep1=strchr(chap, ':');
        const char *sep2 = sep1 ? strchr(sep1+1, ':') : NULL;
        if (sep1)
        {
          int startms=atoi(chap);
          int endms=atoi(sep1+1);
          if (startms >= 0)
          {
            ID3Cue id3_cue={0};
            if (endms > startms) id3_cue.type |= 1;
            id3_cue.type |= 2;
            id3_cue.pos=(double)startms*0.001;
            if (endms > startms) id3_cue.endpos=(double)endms*0.001;
            id3_cue.name = sep2 ? (char*)sep2+1 : (char*)"";
            m_filepool->extraInfo->m_cues.Add(id3_cue);
          }
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "ID3:CHAP%03d", ++i);
        chap=m_filepool->extraInfo->m_metadata.Get(buf);
        if (chap && !chap[0]) chap=NULL;
      }

      __mergesort(m_filepool->extraInfo->m_cues.Get(),
        m_filepool->extraInfo->m_cues.GetSize(),
        sizeof(ID3Cue), _cuecmp, NULL);
    }

    if (idx >= 0 && idx < m_filepool->extraInfo->m_cues.GetSize())
    {
      ID3Cue *id3_cue=m_filepool->extraInfo->m_cues.Get()+idx;
      cue->m_id=idx;
      cue->m_flags = (id3_cue->type&2) ? 4 : 0;
      cue->m_time=id3_cue->pos;
      cue->m_endtime=id3_cue->endpos;
      cue->m_isregion = id3_cue->endpos > id3_cue->pos;
      cue->m_name=(char*)id3_cue->name;
      return 1;
    }

    return 0;
  }

  return 0;
}

PooledDecoderInstance *POOLED_PCMSOURCE_CLASSNAME::CreateInstance()
{
  if (!m_filepool->extraInfo) m_filepool->extraInfo = new PooledDecoderMediaInfo;
  return m_filepool->extraInfo->Open(GetFileName(),m_peaksMode,!m_filepool->HasResources());
}



void PCM_source_mp3::PooledGetSamples(PCM_source_transfer_t *block, PooledDecoderInstance *poolreadinst, void *rsModeToUse)
{
  if (!poolreadinst->m_file) return;
  // process from file
  if (poolreadinst->m_rpoolinfo.m_ownerptr != this)
  {
    poolreadinst->m_lastpos=-1000; // force a reset of any SRC
  }

  ReaSample *sampleoutptr=block->samples;
  int len=block->length;
  const int decsr=m_filepool->extraInfo->m_srate;

  POOLED_GETSAMPLES_MANAGE_RESAMPLER(decsr)

  if (block->absolute_time_s==-100000.0) poolreadinst->m_lastpos=-100000;

  if (fabs(poolreadinst->m_lastpos - (block->time_s+lat)*decsr) >= 8.0)
  {
    mp3_index *mindex = m_filepool->extraInfo->m_index;
    INT64 splpos = (INT64) (block->time_s * decsr + 0.5);

    POOLED_GETSAMPLES_ON_SEEK(splpos, decsr)

    poolreadinst->m_decode_srcsplpos = splpos;

    if (mindex && mindex->GetFrameCount() > 0)
    {

      poolreadinst->m_read_pos=
        mindex->GetSeekPositionForSample(splpos,decsr,&poolreadinst->m_dump_samples);

      if (m_adjustLatency) poolreadinst->m_dump_samples += mindex->m_start_eatsamples;
      else
      {
        int framesize=(decsr < 32000 ? 576 : 1152);

        // ugh, old code was so inaccurate, this should make it match
        poolreadinst->m_dump_samples -= framesize; // insert an extra frame of latency  (old seek code had an excess +1 in the frame count)
        if (poolreadinst->m_dump_samples<0)poolreadinst->m_dump_samples=0;
      }

      poolreadinst->m_need_initial_dump=false;
    }
    else
    {
      poolreadinst->m_dump_samples=0;
      poolreadinst->m_read_pos=m_filepool->extraInfo->m_stream_startpos +
           (int) ((m_filepool->extraInfo->m_stream_endpos - m_filepool->extraInfo->m_stream_startpos) * 
              block->time_s * decsr / m_filepool->extraInfo->GetLengthSamples(m_adjustLatency));
    }
    poolreadinst->m_file->SetPosition(poolreadinst->m_read_pos);

    poolreadinst->m_decoder.Reset(false);
    poolreadinst->m_lastpos=(INT64) (block->time_s*decsr);
    lat=0.0;
  }

  if (poolreadinst->m_need_initial_dump)
  {
    mp3_index *mindex = m_filepool->extraInfo->m_index;

    if (m_adjustLatency && mindex) poolreadinst->m_dump_samples += mindex->m_start_eatsamples;

    poolreadinst->m_need_initial_dump=false;
  }

  if (do_resample)
  {
    len = poolreadinst->m_resampler->ResamplePrepare(block->length,block->nch,&sampleoutptr);
  }

  if (poolreadinst->m_dump_samples<0)
  {
    int l=(-poolreadinst->m_dump_samples)*sizeof(double)*poolreadinst->m_decoder.GetNumChannels();
    poolreadinst->m_dump_samples=0;
    void *b=poolreadinst->m_decoder.queue_samples_out.Add(NULL,l);
    memset(b,0,l);
  }

  int tr=0;
  int hasHadRdError=0;
  while (poolreadinst->m_decoder.queue_samples_out.Available() < len*(int)sizeof(double)*poolreadinst->m_decoder.GetNumChannels())
  {
    if (poolreadinst->m_decoder.queue_bytes_in.Available() < 4096)
    {
      int l=m_filepool->extraInfo->m_stream_endpos-poolreadinst->m_read_pos;
      char buf[4096];
      if (l > sizeof(buf)) l=sizeof(buf);
      l=poolreadinst->m_file->Read(buf,l);
      if (l<1) hasHadRdError=1;
      else
      {
        poolreadinst->m_decoder.queue_bytes_in.Add(buf,l);
        tr+=l;
        poolreadinst->m_read_pos+=l;
      }
    }


    int os=poolreadinst->m_decoder.queue_samples_out.Available();
    if (poolreadinst->m_decoder.Run()) break;
    int l=poolreadinst->m_decoder.queue_samples_out.Available();

    if (l <= os && hasHadRdError) break;

    if (poolreadinst->m_dump_samples>0 && l > 0)
    {
      l /= sizeof(double) * poolreadinst->m_decoder.GetNumChannels();
      if (l > poolreadinst->m_dump_samples) l=poolreadinst->m_dump_samples;
      poolreadinst->m_decoder.queue_samples_out.Advance(l*sizeof(double)*poolreadinst->m_decoder.GetNumChannels());
      poolreadinst->m_dump_samples -= l;
      if (poolreadinst->m_dump_samples<0) poolreadinst->m_dump_samples=0;
    }
  }
  poolreadinst->m_decoder.queue_bytes_in.Compact();

  if (tr&&update_disk_counters) update_disk_counters(tr,0);

  int samples_read=0;
  if (poolreadinst->m_decoder.GetNumChannels()) 
  {
    samples_read=poolreadinst->m_decoder.queue_samples_out.Available()/sizeof(double)/poolreadinst->m_decoder.GetNumChannels();

    INT64 maxs  = (m_filepool->extraInfo->GetLengthSamples(m_adjustLatency) - poolreadinst->m_decode_srcsplpos);

    if (maxs<0)maxs=0;
    if (!m_adjustLatency && samples_read > maxs) samples_read = (int)maxs;
    if (samples_read > len) samples_read=len;
    poolreadinst->m_decode_srcsplpos+=samples_read;

  }

  if (samples_read > 0)
  {
    poolreadinst->m_lastpos += samples_read;
    // copy the samples to sampleoutptr, converting to stereo if necessary
    if (poolreadinst->m_decoder.GetNumChannels() == block->nch)
    {
      memcpy(sampleoutptr,poolreadinst->m_decoder.queue_samples_out.Get(),samples_read*sizeof(double)*block->nch);
      poolreadinst->m_decoder.queue_samples_out.Advance(samples_read*sizeof(double)*block->nch);
    }
    else if (poolreadinst->m_decoder.GetNumChannels() == 1)
    {
      double *inptr=(double *)poolreadinst->m_decoder.queue_samples_out.Get();
      ReaSample *outptr=sampleoutptr;
      int i;
      for (i = 0; i < samples_read; i ++)
      {
        double s=*inptr++;
        int ch;
        for (ch = 0; ch < block->nch; ch ++)
          *outptr++ = s;
      }
      poolreadinst->m_decoder.queue_samples_out.Advance(samples_read*sizeof(double));
    }
    else if (poolreadinst->m_decoder.GetNumChannels() == 2)
    {
      double *inptr=(double *)poolreadinst->m_decoder.queue_samples_out.Get();
      ReaSample *outptr=sampleoutptr;
      const int nch = block->nch;
      if (nch == 1)
      {
        for (int i = 0; i < samples_read; i ++)
        {
          *outptr++ = inptr[0] * 0.5 + inptr[1] * 0.5;
          inptr+=2;
        }
      }
      else if (nch>1)
      {
        for (int i = 0; i < samples_read; i ++)
        {
          *outptr++ = inptr[0];
          *outptr++ = inptr[1];
          inptr+=2;
          for (int ch = 2; ch < nch; ch ++) *outptr++ = 0.0;
        }
      }
      poolreadinst->m_decoder.queue_samples_out.Advance(samples_read*sizeof(double)*2);
    }
    poolreadinst->m_decoder.queue_samples_out.Compact();
  }

  if (!do_resample)
  {
    block->samples_out = samples_read;
  }
  else // resample it now
  {
    block->samples_out=poolreadinst->m_resampler->ResampleOut(block->samples,samples_read, block->length,block->nch);
  }

  // output to block

}

WDL_DLGRET PCM_source_mp3::_dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_INITDIALOG) SetWindowLongPtr(hwndDlg,GWLP_USERDATA,lParam);
  PCM_source_mp3 *_this = (PCM_source_mp3 *)GetWindowLongPtr(hwndDlg,GWLP_USERDATA);
  return _this->propsDlgProc(hwndDlg,uMsg,wParam,lParam);
}


int PCM_source_mp3::PropertiesWindow(HWND hwndParent)
{
  return (int)DialogBoxParam(g_hInst,MAKEINTRESOURCE(IDD_MP3ITEMINFO),hwndParent,_dlgProc,(LPARAM)this);
}

void PCM_source_mp3::GetPropsStr(WDL_FastString &s)
{
  if (!IsAvailable())
  {
    if (!m_filepool || !m_filepool->extraInfo)
    {
      s.Set(m_isOffline ? __LOCALIZE("File offline","mp3dec_DLG_120") : __LOCALIZE("File not opened","mp3dec_DLG_120"));
    }
    else if (m_filepool->extraInfo->m_err)
    {
      s.Set(m_filepool->extraInfo->m_err);
    }
    else
    {
      s.Set(__LOCALIZE("Unknown error/status","mp3dec_DLG_120"));
    }
  }
  else
  {
    char buf[512];
    PooledDecoderInstance *inst=GetFile();
    if (inst)
    {
      s.AppendFormatted(512,__LOCALIZE_VERFMT("MPEG layer %d, @ %d Hz %d channels","mp3dec_DLG_120"),inst->m_decoder.GetLayer(),inst->m_decoder.GetSampleRate(),inst->m_decoder.GetNumChannels());
      s.Append("\r\n");
      ReleaseFile(inst,0);
    }

    if (m_filepool && m_filepool->extraInfo && m_filepool->extraInfo->m_index && m_filepool->extraInfo->m_index->GetFrameCount())
    {
      s.Append(__LOCALIZE("Length:","mp3dec_DLG_120"));
      s.Append(" ");
      format_timestr(m_filepool->extraInfo->GetLengthSamples(m_adjustLatency) / (double)m_filepool->extraInfo->m_srate,buf,sizeof(buf));
      s.Append(buf);
      s.Append("\r\n");

      const char* encstr = __LOCALIZE("CBR (no header)","mp3dec_DLG_120");
      int t = m_filepool->extraInfo->m_index->m_encodingtag;
      if (t == 0) encstr = __LOCALIZE("VBR","mp3dec_DLG_120");
      else if (t == 1) encstr = __LOCALIZE("ABR","mp3dec_DLG_120");
      else if (t == 2) encstr = __LOCALIZE("CBR","mp3dec_DLG_120");
      s.AppendFormatted(512,__LOCALIZE_VERFMT("Encoding: %s","mp3dec_DLG_120"),encstr);
      s.Append("\r\n");
      s.AppendFormatted(512,__LOCALIZE_VERFMT("Channel Mode: %s","mp3dec_DLG_120"),
          m_filepool->extraInfo->m_channel_mode==0 ? __LOCALIZE("Stereo","mp3dec_DLG_120") :
          m_filepool->extraInfo->m_channel_mode==1 ? __LOCALIZE("Joint Stereo","mp3dec_DLG_120") :
          m_filepool->extraInfo->m_channel_mode==2 ? __LOCALIZE("Dual Stereo","mp3dec_DLG_120") :
          m_filepool->extraInfo->m_channel_mode==3 ? __LOCALIZE("Mono","mp3dec_DLG_120") :
                                                      __LOCALIZE("(not available)","mp3dec_DLG_120"));
      s.Append("\r\n");

      s.AppendFormatted(512,__LOCALIZE_VERFMT("Bitrate (average): %.0f kbps","mp3dec_DLG_120"),
        (double)(m_filepool->extraInfo->m_stream_endpos - m_filepool->extraInfo->m_stream_startpos) /
            (m_filepool->extraInfo->GetLengthSamples(m_adjustLatency)/(double)m_filepool->extraInfo->m_srate) * 8.0/1000.0);
      s.Append("\r\n");

      s.AppendFormatted(512,__LOCALIZE_VERFMT("%d frames in file [indexed]","mp3dec_DLG_120"),m_filepool->extraInfo->m_index->GetFrameCount());
      s.Append("\r\n");
    }
    else
    {
      s.Append(__LOCALIZE("No index found [error]!","mp3dec_DLG_120"));
      s.Append("\r\n");
    }

    double prefpos=GetPreferredPosition();
    if (prefpos > 0.0)
    {
      format_timestr(prefpos, buf, sizeof(buf));
      s.AppendFormatted(512, "Start offset: %s\r\n", buf);
    }

    if (m_filepool && m_filepool->extraInfo)
    {
      DumpMetadata(&s, &m_filepool->extraInfo->m_metadata);
    }
  }
}

WDL_DLGRET PCM_source_mp3::propsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      SetDlgItemText(hwndDlg,IDC_NAME,GetFileName());
      CheckDlgButton(hwndDlg,IDC_CHECK1,m_adjustLatency?BST_CHECKED:BST_UNCHECKED);
      WDL_FastString s;
      GetPropsStr(s);
      SetDlgItemText(hwndDlg,IDC_INFO,s.Get());
    }
    return 0;

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
        case IDCANCEL:
          m_adjustLatency = !!IsDlgButtonChecked(hwndDlg,IDC_CHECK1);
          EndDialog(hwndDlg,0);
        return 0;
      }
    return 0;
  }
  return 0;
}


void PCM_source_mp3::SaveState(ProjectStateContext *ctx)
{
  char buf[2048];
  if (relative_fn)
  {
    relative_fn(GetFileName(),buf,sizeof(buf));
    ctx->AddLine("FILE %p~ %d",buf,m_adjustLatency);
  }
  else ctx->AddLine("FILE %p~ %d",GetFileName(),m_adjustLatency);
}

int PCM_source_mp3::LoadState(const char *firstline, ProjectStateContext *ctx) // -1 on error
{
  int child_count=1;
  bool comment_state=false;
  m_adjustLatency=false;

  for (;;)
  {
    char linebuf[4096];
    if (ctx->GetLine(linebuf,sizeof(linebuf))) break;
    LineParser lp(comment_state);
    if (lp.parse(linebuf)||lp.getnumtokens()<=0) continue;
    if (child_count == 1)
    {
      if (lp.gettoken_str(0)[0] == '>') return 0;
      
      if (lp.gettoken_str(0)[0] == '<') child_count++;
      else if (!stricmp(lp.gettoken_str(0),"FILE"))
      {
        if (lp.getnumtokens()>1)
        {
          if (!IsAvailable() || stricmp(lp.gettoken_str(1),GetFileName()))
          {
            char buf[4096];
            resolve_fn(lp.gettoken_str(1),buf,sizeof(buf));
            Open(buf);
          }
          if (lp.getnumtokens()>2)
            m_adjustLatency = !!lp.gettoken_int(2);
        }
      }
    }
    else if (lp.gettoken_str(0)[0] == '<') child_count++;
    else if (lp.gettoken_str(0)[0] == '>') child_count--;
  }
  return -1;
}












PCM_source *CreateFromType(const char *type, int priority)
{
  if (priority > 4) // lowest priority wav reader
  {
    if (!strcmp(type,SOURCE_TYPE))
      return new PCM_source_mp3;
  }

  return NULL;
}

PCM_source *CreateFromFile(const char *filename, int priority)
{
  const size_t lfn=strlen(filename);
  if (priority > 4 && lfn>4 && (!stricmp(filename+lfn-4,".mp3") || !stricmp(filename+lfn-4,".mp2")))
  {
    PCM_source_mp3 *w=new PCM_source_mp3;
    w->Open(filename);
    if (w->IsAvailable()) return w;
    delete w;
  }
  return NULL;
}

  // this is used for UI only, not so muc
const char *EnumFileExtensions(int i, const char **descptr) // call increasing i until returns a string, if descptr's output is NULL, use last description
{
  if (i == 0)
  {
    if (descptr) *descptr = __LOCALIZE("MPEG Audio files","mp3");
    return "MP2;MP3";
  }
  if (descptr) *descptr=NULL;

  if (i == 4096) return "REAPINDEX"; // let it know this shit should be deleted, if it can be
  return NULL;
}




pcmsrc_register_t myRegStruct={CreateFromType,CreateFromFile,EnumFileExtensions};

extern pcmsink_register_ext_t mySinkRegStruct;



#include "impegdecode.h"

class mpeg_decoder_inst : public IMPEGDecoder
{
public:
  mpeg_decoder_inst() { }
  virtual ~mpeg_decoder_inst() { }
  
  bool WantInput() 
  {
    return m_dec.queue_bytes_in.Available() < 4096;
  }
  void AddInput(const void *buf, int buflen)
  {
    m_dec.queue_bytes_in.Add(buf,buflen);
  }

  double *GetOutput(int *avail) // avail returns number of doubles avail
  {
    *avail = m_dec.queue_samples_out.Available()/sizeof(double);
    return (double *)m_dec.queue_samples_out.Get();
  }
  void OutputAdvance(int num)  // num = number of doubles
  {
    m_dec.queue_samples_out.Advance(num*sizeof(double));
    m_dec.queue_samples_out.Compact();
  }

  void Reset(bool full)
  {
    m_dec.Reset(full);
  }
  int SyncState() { return m_dec.SyncState(); } // > 0 if synched
  int Run() { int ret=m_dec.Run();  m_dec.queue_bytes_in.Compact(); return ret; } // returns -1 on non-recoverable error
  int GetSampleRate() { return m_dec.GetSampleRate(); }
  int GetNumChannels() { return m_dec.GetNumChannels(); }

  mp3_decoder m_dec;
};


IMPEGDecoder *CreateMPEGdecoder()
{
  return new mpeg_decoder_inst;
}

void register_sink(reaper_plugin_info_t *rec);



class PCM_source_mp3_metadata : public PCM_source
{
public:

  WDL_StringKeyedArray<char*> m_metadata;
  int m_numchan, m_samplerate;
  double m_len, m_bitrate;
  bool m_success;

  PCM_source_mp3_metadata(const char *fn)
  : m_metadata(false, WDL_StringKeyedArray<char*>::freecharptr)
  {
    m_success=false;
    m_numchan=m_samplerate=0;
    m_len=m_bitrate=0.0;
    WDL_FileRead *fr = fn ? new WDL_FileRead(fn, 0) : NULL;
    if (fr && fr->IsOpen())
    {
      WDL_INT64 fstart=0, fend=0;
      ReadMediaTags(fr, &m_metadata, &fstart, &fend);

      if (fend > fstart)
      {
        // slight extra work here, mp3_index::quickMetadataRead will re-skip the ID3v2 tag, we could prime it with pos=fstart but meh that would require more changes
        mp3_index::mp3_metadata md;
        if (mp3_index::quickMetadataRead(fn,fr,&md))
        {
          m_samplerate = md.srate;
          m_numchan = md.nch;
          m_len = md.len;
          if (m_len > 0.0) m_bitrate=8.0*(double)(fend-fstart)/m_len;
          m_success=true;
        }
      }
    }
    delete fr;
  }

  virtual ~PCM_source_mp3_metadata() {}

  virtual PCM_source *Duplicate() { return NULL; }
  virtual bool IsAvailable() { return m_metadata.GetSize()>0; }
  virtual const char *GetType() { return "mp3_metadata"; }
  virtual bool SetFileName(const char *newfn) { return false; }
  virtual int GetNumChannels() { return m_numchan; }
  virtual double GetSampleRate() { return (double)m_samplerate; }
  virtual int GetBitsPerSample() { return 0; }
  virtual double GetLength() { return m_len; }
  virtual int PropertiesWindow(HWND hwndParent) { return -1; }
  virtual void GetSamples(PCM_source_transfer_t *block) { }
  virtual void GetPeakInfo(PCM_source_peaktransfer_t *block) { }
  virtual void SaveState(ProjectStateContext *ctx) { }
  virtual int LoadState(const char *firstline, ProjectStateContext *ctx) { return -1; }
  virtual void Peaks_Clear(bool deleteFile) { }
  virtual int PeaksBuild_Begin() { return 0; }
  virtual int PeaksBuild_Run() { return 0; }
  virtual void PeaksBuild_Finish() { }

  virtual double GetPreferredPosition()
  {
    return ReadMetadataPrefPos(&m_metadata, (double)m_samplerate);
  }

  virtual int Extended(int call, void *parm1, void *parm2, void *parm3)
  {
    PCM_SOURCE_EXT_GETMETADATA_HANDLER(&m_metadata)

    if (call == PCM_SOURCE_EXT_GETBITRATE && parm1)
    {
      *(double*)parm1=m_bitrate;
      return 1;
    }

    return 0;
  }
};

static PCM_source *mp3__createMetadataSource(const char *p)
{
  PCM_source_mp3_metadata *src=new PCM_source_mp3_metadata(p);
  if (src->m_success) return src;
  delete src;
  return NULL;
}


extern "C"
{

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec)
{
  g_hInst=hInstance;
  if (rec)
  {
    if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc)
      return 0;

    g_main_hwnd=rec->hwnd_main;
    *((void **)&Resampler_Create) = rec->GetFunc("Resampler_Create");
    *((void **)&format_timestr) = rec->GetFunc("format_timestr");
    *((void **)&PeakGet_Create) = rec->GetFunc("PeakGet_Create");
    *((void **)&PeakBuild_CreateEx) = rec->GetFunc("PeakBuild_CreateEx");
    *((void **)&resolve_fn) = rec->GetFunc("resolve_fn");   
    *((void **)&relative_fn) = rec->GetFunc("relative_fn");   
    *((void **)&GetPeakFileName) = rec->GetFunc("GetPeakFileName");    
    *((void **)&GetPeakFileNameEx2) = rec->GetFunc("GetPeakFileNameEx2");    
    *((void **)&update_disk_counters) = rec->GetFunc("update_disk_counters");
    *((void **)&get_ini_file) = rec->GetFunc("get_ini_file");
    *((void **)&GetResourcePath) = rec->GetFunc("GetResourcePath");
    *((void **)&GetExePath) = rec->GetFunc("GetExePath");
    *((void **)&EnumCurrentSinkMetadata) = rec->GetFunc("EnumCurrentSinkMetadata");
    *((void **)&__mergesort) = rec->GetFunc("__mergesort");

    *((void **)&GetPreferredDiskReadMode) = rec->GetFunc("GetPreferredDiskReadMode");
    *((void **)&GetPreferredDiskReadModePeak) = rec->GetFunc("GetPreferredDiskReadModePeak");
    *((void **)&HiresPeaksFromSource) = rec->GetFunc("HiresPeaksFromSource");

    *(void **)&gOnMallocFailPtr = rec->GetFunc("gOnMallocFail");

    if (!PeakGet_Create || !PeakBuild_CreateEx || !Resampler_Create || !format_timestr || !resolve_fn ||       
        !rec->Register)
          return 0;

    IMPORT_LOCALIZE_RPLUG(rec)

    rec->Register("pcmsrc",&myRegStruct);
    rec->Register("API_CreateMPEGdecoder",(void*)CreateMPEGdecoder);
    rec->Register("API_mp3__createMetadataSource",(void*)mp3__createMetadataSource);

    POOLED_PCM_INIT(rec);

    register_sink(rec);

    // our plugin registered, return success

    return 1;
  }
  else
  {
    return 0;
  }
}

};


#ifndef _WIN32 // MAC resources
#include "../../WDL/swell/swell-dlggen.h"
#include "res.rc_mac_dlg"
#undef BEGIN
#undef END
#include "../../WDL/swell/swell-menugen.h"
#include "res.rc_mac_menu"
#endif
