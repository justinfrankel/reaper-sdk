#ifndef POOLEDSRC_ONLY_HELPER_MACROS

#include "../WDL/rpool.h"
#include "../WDL/poollist.h"

#define DECODER_POOL_OWNAFTERREAD 3000

#ifndef NUM_SOURCE_POOLS
#define NUM_SOURCE_POOLS 3 // default is 0=normal, 1=peaks, 2=hi-res peaks
#endif

// out of range peaksmode (<0 or >=NUM_SOURCE_POOLS) will result in an unpooled source, which may not support peaks or duplicate or other APIs (metadata retrieval only really)

#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
class PooledDecoderMediaInfo : public PooledDecoderMediaInfo_Base
{
  public:
    PooledDecoderMediaInfo()
    {
      m_peakbuilder=0;
      m_peakgetter=0;
      m_hiressrc=0;
      m_hiressrc_lastused=0;
    }
    ~PooledDecoderMediaInfo()
    {
      delete m_peakbuilder;
      delete m_peakgetter;
      delete m_hiressrc;
    }

    REAPER_PeakGet_Interface *m_peakgetter;
    REAPER_PeakBuild_Interface *m_peakbuilder;
    PCM_source *m_hiressrc; // can be shared across all instances since it's singlethreaded and without state
    DWORD m_hiressrc_lastused;

};
#endif

static WDL_PoolList_NoFreeOnDestroy<WDL_ResourcePool<PooledDecoderInstance,PooledDecoderMediaInfo> > g_file_pool[NUM_SOURCE_POOLS]; // [m_peaksMode]

class FileNameListEnt
{
public:
  FileNameListEnt(char *identstr) { WDL_POOLLIST_identstr=identstr; WDL_POOLLIST_refcnt=0; }
  ~FileNameListEnt() {}  // do NOT free or delete WDL_POOLLIST_identstr

  void Clear() {}  // will be called if ReleasePool(x,false) is called and refcnt gets to 0
  int WDL_POOLLIST_refcnt;
  char *WDL_POOLLIST_identstr;
};

static WDL_PoolList_NoFreeOnDestroy<FileNameListEnt> s_filename_list;


class POOLED_PCMSOURCE_CLASSNAME : public PCM_source
{
  public:
    POOLED_PCMSOURCE_CLASSNAME()
    {
      m_filename_ent=0;
      m_isavail=false;
      m_isOffline=0;
      m_filepool=NULL;

      m_peaksMode=0;
      m_resampler_rsmode = (void *)(INT_PTR)-1;
#ifdef POOLEDSRC_USE_SLICES
      m_slice=-1;
      m_tempo=0.0;
#endif
#ifdef POOLED_PCMSOURCE_EXTRASTUFF_INIT
      POOLED_PCMSOURCE_EXTRASTUFF_INIT
#endif
      Close();      
    }
#ifdef POOLEDSRC_USE_SLICES
    int m_slice;
    double m_tempo WDL_FIXALIGN;
#endif

    virtual ~POOLED_PCMSOURCE_CLASSNAME() 
    {
      Close();
      if (m_filename_ent)
      {
        s_filename_list.Release(m_filename_ent);
        m_filename_ent=0;
      }
    }

    PCM_source *Duplicate()
    {
      POOLED_PCMSOURCE_CLASSNAME *ns=new POOLED_PCMSOURCE_CLASSNAME;
#ifdef POOLEDSRC_USE_SLICES
      ns->m_slice = m_slice;
      ns->m_tempo = m_tempo;
#endif
      if (m_filename_ent) ns->Open(m_filename_ent->WDL_POOLLIST_identstr);
#ifdef POOLED_PCMSOURCE_EXTRASTUFF_DUPLICATECODE
      POOLED_PCMSOURCE_EXTRASTUFF_DUPLICATECODE
#endif
      return ns;
    }
    const char *GetFileName() 
    { 
      const char *p = m_filename_ent ? m_filename_ent->WDL_POOLLIST_identstr : NULL;
      if (!p) p = m_filepool ? m_filepool->WDL_POOLLIST_identstr : NULL;
      return p ? p : "";
    }
    bool SetFileName(const char *newfn) 
    { 
      if (m_filename_ent)
      {
        s_filename_list.Release(m_filename_ent);
        m_filename_ent=0;
      }
      m_filename_ent=s_filename_list.Get(newfn);
      return true; 
    } 


    const char *GetType() { return SOURCE_TYPE; }
    int GetNumChannels() { return m_filepool && m_filepool->extraInfo ? m_filepool->extraInfo->GetNumChannels() : 1; } // return number of channels

    double GetPreferredPosition() 
    { 
      return m_filepool && m_filepool->extraInfo ? m_filepool->extraInfo->GetPreferredPosition() : -1.0; 
    } 

    double GetSampleRate() { return m_filepool && m_filepool->extraInfo ? m_filepool->extraInfo->GetSampleRate() : 0.0; }
    
#ifdef POOLEDSRC_USE_SLICES
    double GetLength();
#else
    double GetLength()    
    {
      return m_filepool && m_filepool->extraInfo ? m_filepool->extraInfo->GetLength(
#ifdef POOLED_PCMSOURCE_EXTRASTUFF_GETLENGTH_PARM
        POOLED_PCMSOURCE_EXTRASTUFF_GETLENGTH_PARM
#endif
        ) : 0;
    } 
#endif

#ifdef POOLEDSRC_USE_SLICES
    double GetLengthBeats();
#endif

    int GetBitsPerSample() { return m_filepool && m_filepool->extraInfo ? m_filepool->extraInfo->GetBitsPerSample() : 0; }

    bool IsAvailable() 
    { 
      return m_isavail && m_filepool && m_filepool->extraInfo && m_filepool->extraInfo->IsAvailable(); 
    }

    void SetAvailable(bool avail) 
    { 
      if (!avail)
      {
        m_isOffline=1;
        if (IsAvailable()) 
        {
          Close(false);
        }
      }
      else
      {
        m_isOffline=0;
        if (!IsAvailable())
          Open(NULL);
      }
   
    } // optional, if called with avail=false, close files/etc, and so on


    void OpenPeaks()
    {
#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
      if (!m_filepool||!m_filepool->extraInfo || m_filepool->extraInfo->m_peakbuilder)
      {
        return;
      }

      if (!m_filepool->extraInfo->m_peakgetter && m_filename_ent) 
      {
        int sr = (int)m_filepool->extraInfo->GetSampleRate();
        int nch = m_filepool->extraInfo->GetNumChannels();
        if (sr>0 && nch>0)
          m_filepool->extraInfo->m_peakgetter=PeakGet_Create(m_filename_ent->WDL_POOLLIST_identstr,sr,nch);
      }
#endif
    }


    void Open(const char *filename, int peaksMode=0);
    void Close(bool full=true)
    {
#ifndef POOLEDSRC_ALLOWPOOLMETASAVE
      full=true;
#endif

      if (m_peaksMode>=0 && m_peaksMode < NUM_SOURCE_POOLS)
      {
        if (g_file_pool[m_peaksMode].Release(m_filepool,full)==0 && !full)
        {
          if (m_filepool && m_filepool->extraInfo)
          {
#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
            delete m_filepool->extraInfo->m_hiressrc;
            m_filepool->extraInfo->m_hiressrc=0;
            delete m_filepool->extraInfo->m_peakgetter;
            m_filepool->extraInfo->m_peakgetter=NULL;
            delete m_filepool->extraInfo->m_peakbuilder;
            m_filepool->extraInfo->m_peakbuilder=NULL;
#endif
#ifdef POOLEDSRC_POOLEDMEDIAINFO_HAS_CLOSE
            m_filepool->extraInfo->Close();
#endif
          }
        }
      }
      else if (m_filepool)
      {
        free(m_filepool->WDL_POOLLIST_identstr);
        delete m_filepool;
      }
      m_filepool=0;

      m_isavail=false;
    }


    int PropertiesWindow(HWND hwndParent);

    void PooledGetSamples(PCM_source_transfer_t *block, PooledDecoderInstance *poolreadinst, void *rsModeToUse);
    void GetSamples(PCM_source_transfer_t *block)
    {
      block->samples_out=0;

      PooledDecoderInstance *poolreadinst=0;
      if (m_filepool && m_filepool->extraInfo && m_filepool->extraInfo->IsAvailable() && (poolreadinst=GetFile())) 
      {
        PooledGetSamples(block,poolreadinst,m_resampler_rsmode);

        // output to block
        ReleaseFile(poolreadinst,m_peaksMode ? 0 : DECODER_POOL_OWNAFTERREAD);
      }
    }


    void SaveState(ProjectStateContext *ctx);
    int LoadState(const char *firstline, ProjectStateContext *ctx); // -1 on error


#ifdef POOLEDSRC_USE_SLICES
#define STORE_BLOCK_TIMEINFO \
  double _ot=block->start_time; double _oat=block->absolute_time_s;
#define RESTORE_BLOCK_TIMEINFO \
  block->start_time=_ot; block->absolute_time_s=_oat;
#else
#define STORE_BLOCK_TIMEINFO
#define RESTORE_BLOCK_TIMEINFO
#endif

    void GetPeakInfo(PCM_source_peaktransfer_t *block)
#ifdef POOLEDSRC_GETPEAKINFO_NOIMPL
      ;
#else
    {
      block->peaks_out=0;
      if (!m_filepool || !m_filepool->extraInfo) return;

#ifdef POOLEDSRC_USE_SLICES
      STORE_BLOCK_TIMEINFO;
      if (m_slice >= 0 && m_slice < m_filepool->extraInfo->m_slices.GetSize())
      {
        WDL_INT64 offs=m_filepool->extraInfo->m_slices.Get()[m_slice];
        double toffs=(double)offs/m_filepool->extraInfo->m_srate;
        block->start_time += toffs;
        block->absolute_time_s += toffs;
      }
#endif

      REAPER_PeakBuild_Interface *pb = m_filepool->extraInfo->m_peakbuilder;

      if (pb)
      {
        double peakres = REAPER_PEAKRES_MAX_NOPKS;
        if (block->extra_requested_data && block->extra_requested_data_type == PEAKINFO_EXTRADATA_SPECTROGRAM1)
          peakres = 40.0;
        if (block->extra_requested_data2 && block->extra_requested_data_type2 == PEAKINFO_EXTRADATA_SPECTROGRAM1)
          peakres = 40.0;
        if (block->peakrate >= peakres)
        {
          if (!m_filepool->extraInfo->m_hiressrc && m_filename_ent)
          {
            POOLED_PCMSOURCE_CLASSNAME *t=new POOLED_PCMSOURCE_CLASSNAME;
            t->Open(m_filename_ent->WDL_POOLLIST_identstr,2);
            m_filepool->extraInfo->m_hiressrc=t;
          }
          if (m_filepool->extraInfo->m_hiressrc)
          {
            m_filepool->extraInfo->m_hiressrc_lastused = GetTickCount();
#ifndef POOLED_PCMSOURCE_NOCHECKFUNCTIONPOINTERS
            if (HiresPeaksFromSource)
#endif
            HiresPeaksFromSource(m_filepool->extraInfo->m_hiressrc,block);
            if (block->peaks_out)
            {
              RESTORE_BLOCK_TIMEINFO;
              return;
            }
          }
        }

        pb->GetPeakInfo(block);
        RESTORE_BLOCK_TIMEINFO;
        return;
      }

      if (!m_filepool->extraInfo->GetNumChannels() || !block->numpeak_points)
      {
        RESTORE_BLOCK_TIMEINFO;
        return;
      }

      REAPER_PeakGet_Interface *pg = m_filepool->extraInfo->m_peakgetter;

      double peakres=pg ? pg->GetMaxPeakRes() : REAPER_PEAKRES_MAX_NOPKS;

      if (pg && block->extra_requested_data && block->extra_requested_data_type == PEAKINFO_EXTRADATA_SPECTROGRAM1)
        peakres = 40.0;
      if (pg && block->extra_requested_data2 && block->extra_requested_data_type2 == PEAKINFO_EXTRADATA_SPECTROGRAM1)
        peakres = 40.0;

      if (block->peakrate >= peakres*REAPER_PEAKRES_MUL_MAX)
      {
        if (!m_filepool->extraInfo->m_hiressrc && m_filename_ent)
        {
          POOLED_PCMSOURCE_CLASSNAME *t=new POOLED_PCMSOURCE_CLASSNAME;
          t->Open(m_filename_ent->WDL_POOLLIST_identstr,2);
          m_filepool->extraInfo->m_hiressrc=t;
        }
        if (m_filepool->extraInfo->m_hiressrc)
        {
          m_filepool->extraInfo->m_hiressrc_lastused = GetTickCount();
          block->__peakgetter = pg;
#ifndef POOLED_PCMSOURCE_NOCHECKFUNCTIONPOINTERS
          if (HiresPeaksFromSource)
#endif
          HiresPeaksFromSource(m_filepool->extraInfo->m_hiressrc,block);
          block->__peakgetter = NULL;
          if (block->peaks_out)
          {
            RESTORE_BLOCK_TIMEINFO;
            return;
          }
        }
      }

      if (pg) pg->GetPeakInfo(block);
      RESTORE_BLOCK_TIMEINFO;
    }
#endif
    void Peaks_Clear(bool deleteFile) 
    { 
#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
      if (m_filepool && m_filepool->extraInfo)
      {
        delete m_filepool->extraInfo->m_peakgetter;
        m_filepool->extraInfo->m_peakgetter=0;
        delete m_filepool->extraInfo->m_peakbuilder;
        m_filepool->extraInfo->m_peakbuilder=0;
      }

      if (deleteFile &&
#ifndef POOLED_PCMSOURCE_NOCHECKFUNCTIONPOINTERS
        GetPeakFileName &&
#endif
        m_filename_ent)
      {
        char fn[2048],lfn[2048];
        lfn[0]=0;

        // handle fallback peak paths
        int n=8;
        while (n--) 
        {
          GetPeakFileName(m_filename_ent->WDL_POOLLIST_identstr,fn,sizeof(fn));
          if (!strcmp(lfn,fn)) break;
          DeleteFile(fn);
          strcpy(lfn,fn);
        }
      }
#endif
    }
    int PeaksBuild_Begin() 
    { 
      if (!m_filepool || !m_filepool->extraInfo) return 0;
#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
      if (m_filepool->extraInfo->m_peakbuilder) return 1;
      OpenPeaks();

      if (m_filepool->extraInfo->m_peakgetter||!IsAvailable()||!m_filename_ent) return 0;

      delete m_filepool->extraInfo->m_peakbuilder;
      m_filepool->extraInfo->m_peakbuilder=NULL;

      POOLED_PCMSOURCE_CLASSNAME *ni = new POOLED_PCMSOURCE_CLASSNAME;
      ni->Open(m_filename_ent->WDL_POOLLIST_identstr,1);
      int sr = (int)m_filepool->extraInfo->GetSampleRate();
      int nch = m_filepool->extraInfo->GetNumChannels();
      if (sr>0 && nch>0)
      {
#ifdef POOLEDSRC_WANTFPPEAKS
        m_filepool->extraInfo->m_peakbuilder = PeakBuild_CreateEx(ni,m_filename_ent->WDL_POOLLIST_identstr,sr,nch, (POOLEDSRC_WANTFPPEAKS) ? 1 : 0);
#else
        m_filepool->extraInfo->m_peakbuilder = PeakBuild_Create(ni,m_filename_ent->WDL_POOLLIST_identstr,sr,nch);
#endif
        return 1; 
      }
      delete ni;
      return 0;

#else
      return 0;
#endif
    } // returns nonzero if building is opened, otherwise it may mean building isn't necessary
    int PeaksBuild_Run() 
    { 
#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
      if (m_filepool && m_filepool->extraInfo && m_filepool->extraInfo->m_peakbuilder)
      {
        return m_filepool->extraInfo->m_peakbuilder->Run();
      }
#endif
      return 0; 
    } // returns nonzero if building should continue

    void PeaksBuild_Finish() 
    { 
#ifndef POOLEDSRC_GETPEAKINFO_NOIMPL
      if (m_filepool && m_filepool->extraInfo && m_filepool->extraInfo->m_peakbuilder)
      {
        delete m_filepool->extraInfo->m_peakbuilder;
        m_filepool->extraInfo->m_peakbuilder=0;
      }
      OpenPeaks();
#endif
    } // called when done


#define PCM_SOURCE_EXT_GETINFOSTRING_HANDLER \
  if (call == PCM_SOURCE_EXT_GETINFOSTRING && parm1 && parm2) \
  { \
    WDL_FastString s; GetPropsStr(s); \
    lstrcpyn((char*)parm1, s.Get(), (int)(INT_PTR)parm2); \
    return s.GetLength() ? 1 : 0; \
  }

#define PCM_SOURCE_EXT_GETMETADATA_HANDLER(metadata) \
  if (call == PCM_SOURCE_EXT_GETMETADATA && parm1 && parm2 && parm3) \
  { \
    HandleMexMetadataRequest((const char*)parm1, (char*)parm2, (int)(INT_PTR)parm3, (metadata)); \
    return strlen((char*)parm2); \
  } \
  if (call == PCM_SOURCE_EXT_ENUMMETADATA && parm2 && parm3) \
  { \
    const char *k, *v=(metadata)->Enumerate((int)(INT_PTR)parm1, &k); if (!v) return 0; \
    *(const char**)parm2=k; *(const char**)parm3=v; return 1; \
  }

    int Extended(int call, void *parm1, void *parm2, void *parm3) 
    { 
      if (call == PCM_SOURCE_EXT_ENDPLAYNOTIFY)
      {
        if (m_filepool) m_filepool->ReleaseResources(this);
        
        return 1;
      }
      if (call == PCM_SOURCE_EXT_SETRESAMPLEMODE) 
      {
        m_resampler_rsmode=parm1;
        return 1;
      }
      if (m_filepool && m_filepool->extraInfo)
        return PoolExtended(call,parm1,parm2,parm3);
      return 0;
    }

    int PoolExtended(int call, void *parm1, void *parm2, void *parm3);


    FileNameListEnt *m_filename_ent;

    WDL_ResourcePool<PooledDecoderInstance, PooledDecoderMediaInfo> *m_filepool;

    char m_isOffline;
    int m_peaksMode; // <0 for non-pooled source
  protected:

    void *m_resampler_rsmode;

  void GetPropsStr(WDL_FastString &s);
  WDL_DLGRET propsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static WDL_DLGRET _dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

  bool m_isavail;

  PooledDecoderInstance *GetFile();
  void ReleaseFile(PooledDecoderInstance *f, int len);

  PooledDecoderInstance *CreateInstance();

#ifdef POOLED_PCMSOURCE_EXTRASTUFF
  POOLED_PCMSOURCE_EXTRASTUFF
#endif
};

void POOLED_PCMSOURCE_CLASSNAME::Open(const char *filename, int peaksMode)
{
  Close(!!filename);

  m_peaksMode=peaksMode;

  if (peaksMode>=0 && peaksMode<NUM_SOURCE_POOLS)
  {
    if (filename && strcmp(filename,GetFileName())) 
    {
      if (m_filename_ent)
      {
        s_filename_list.Release(m_filename_ent);
        m_filename_ent=0;
      }
      m_filename_ent=s_filename_list.Get(filename);
    }
    m_filepool=m_filename_ent ? g_file_pool[peaksMode].Get(m_filename_ent->WDL_POOLLIST_identstr) : 0;
  }
  else
  {
    m_filepool=new WDL_ResourcePool<PooledDecoderInstance, PooledDecoderMediaInfo>(strdup(filename));
  }

  if (m_filepool)
  {
    if (!m_filepool->HasResources())
    {
      PooledDecoderInstance *poolreadinst=GetFile();
      if (poolreadinst) 
      {
        m_isavail=true;
        ReleaseFile(poolreadinst,0);
        if (!peaksMode) OpenPeaks();
      }
    }
    else 
    {
      m_isavail=true;
      if (!peaksMode) OpenPeaks();
    }
  }

}

void POOLED_PCMSOURCE_CLASSNAME::ReleaseFile(PooledDecoderInstance *f, int len)
{
  if (!m_filepool) delete f;
  else
  {
    m_filepool->AddResource(f,this,len > 0 ? (GetTickCount()+len) : 0);
  }
}

PooledDecoderInstance *POOLED_PCMSOURCE_CLASSNAME::GetFile()
{
  if (!m_filepool) return 0;

  m_filepool->LockList();
  PooledDecoderInstance *p;
  p = m_filepool->GetResource(this,GetTickCount());
  if (!p) p = CreateInstance();
  m_filepool->UnlockList();

  return p;
}


/*

  
#include "../../WDL/ptrlist.h"
#include "../../WDL/rpool.h"


class PooledDecoderInstance
{
public:
  PooledDecoderInstance() 
  {
    m_resampler=0;
    m_resampler_state=0;
    m_lastpos = -10000000; 
  }
  ~PooledDecoderInstance() 
  { 
    delete m_resampler;
  }

  WDL_ResourcePool_ResInfo m_rpoolinfo;

  REAPER_Resample_Interface *m_resampler;
  int m_resampler_state;
  INT64 m_lastpos;
};

class PooledDecoderMediaInfo_Base
{
public:
  PooledDecoderMediaInfo_Base()
  {
  }
  ~PooledDecoderMediaInfo_Base()
  {
  }

  PooledDecoderInstance *Open(const char *filename, int peaksMode, bool forceread)
  {
    PooledDecoderInstance *ndec = new PooledDecoderInstance;
    return ndec;
  }


  int GetNumChannels() { return m_nch; } // return number of channels
  double GetSampleRate() { return m_srate; }
  double GetLength()  // length in seconds
  { 
    if (m_srate<1.0) return 0; 
    return m_lengthsamples / m_srate; 
  } 
  int GetBitsPerSample() { return m_bps; }
  bool IsAvailable() 
  { 
    return m_srate>=1.0 && m_bps && m_nch; 
  }

  double GetPreferredPosition()
  {
    return -1.0;
  }


};


#define POOLED_PCMSOURCE_CLASSNAME PCM_source_whatever

#include "../pooled_pcmsource_impl.h"

PooledDecoderInstance *POOLED_PCMSOURCE_CLASSNAME::CreateInstance()
{
  if (!m_filepool->extraInfo) m_filepool->extraInfo = new PooledDecoderMediaInfo;
  return m_filepool->extraInfo->Open(GetFileName(),m_peaksMode,!m_filepool->HasResources());
}

void POOLED_PCMSOURCE_CLASSNAME::PooledGetSamples(PCM_source_transfer_t *block, PooledDecoderInstance *poolreadinst)
{
}
int POOLED_PCMSOURCE_CLASSNAME::PoolExtended(int call, void* parm1, void* parm2, void* parm3)
{
  return 0;
}

  */

#ifndef POOLEDSRC_NOGARBAGECOLLECT
static int pooled_pcm_source_garbage_collect(int flags)
{
  int rv = 0;
  WDL_PtrList< WDL_ResourcePool<PooledDecoderInstance,PooledDecoderMediaInfo> > * list = &g_file_pool[0].pool;
  const int list_sz = list->GetSize();

  int n = list_sz;
  static int pos;
  if (!(flags & 1)) 
  {
    // scan everything
    pos = 0;
  }
  else
  {
    if (pos >= list_sz) pos = 0;
    if (n > 30) n = 30;
  }

  const DWORD now = GetTickCount();

  while (n-->0)
  {
    WDL_ResourcePool<PooledDecoderInstance,PooledDecoderMediaInfo> *p = list->Get(pos);
    if (p)
    {
      if (p->extraInfo && p->extraInfo->m_hiressrc && 
          ((flags&2) || (now - p->extraInfo->m_hiressrc_lastused) > 60000)
         )
      {
        delete p->extraInfo->m_hiressrc;
        p->extraInfo->m_hiressrc=NULL;
        rv++;
      }

      // see if we can get any resources to destroy
      for (;;)
      {
        PooledDecoderInstance *r = p->GetResource(NULL,0,(flags&2) ? 2 : 1);
        if (!r) break;
        delete r;
        rv++;
  
        if (flags & 1) 
          return rv; // incremental mode: only close one file at a time
      }
    }
    if (++pos >= list_sz) pos=0;
  }
  return rv;
}

#define POOLED_PCM_INIT(rec) \
  (rec)->Register("open_file_reduce",(void *)&pooled_pcm_source_garbage_collect)

#endif // POOLEDSRC_NOGARBAGECOLLECT

#endif // POOLEDSRC_ONLY_HELPER_MACROS

// m_resampler_state=0 for not yet initialized, 1=active, 2=inactive
#define POOLED_GETSAMPLES_MANAGE_RESAMPLER(msrate) \
  double lat=0.0; \
  int do_resample=0; \
  if (fabs(msrate-block->samplerate)>=0.00001) { \
    if (!poolreadinst->m_resampler_state && WDL_NORMALLY(!poolreadinst->m_resampler)) \
      poolreadinst->m_resampler = Resampler_Create(); \
    do_resample = poolreadinst->m_resampler_state = 1; \
  } else if (poolreadinst->m_resampler_state == 1) do_resample = 2; \
  if (do_resample) { \
    if (WDL_NORMALLY(poolreadinst->m_resampler)) { \
      poolreadinst->m_resampler->Extended(RESAMPLE_EXT_SETRSMODE,rsModeToUse,NULL,NULL); \
      poolreadinst->m_resampler->SetRates(msrate,block->samplerate); \
      lat = poolreadinst->m_resampler->GetCurrentLatency(); \
    } else do_resample = 0; \
  }

#define POOLED_GETSAMPLES_ON_SEEK(tpos, msrate) \
        if (do_resample) { \
          if (poolreadinst->m_resampler) poolreadinst->m_resampler->Reset(); \
          if (do_resample == 2) { \
            do_resample = 0; \
            poolreadinst->m_resampler_state = 2; \
            tpos=(INT64)floor(block->time_s * msrate + 0.5); \
          } else { \
            tpos=(INT64)floor(block->time_s * msrate); /* this should probably be +0.5, but a35f321 bleh */ \
          } \
        }
