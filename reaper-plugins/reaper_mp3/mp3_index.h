#ifndef _MP3_INDEX_H_
#define _MP3_INDEX_H_

#include "../../WDL/fileread.h"

class mp3_index
{
protected:
  mp3_index(const char *fn) 
  {
    m_refcnt=0; 
    m_numframes=m_framestart=0;
    m_framefile=0; 
    m_fn.Set(fn); 
    m_start_eatsamples=0;
    m_end_eatsamples=0; 
    m_encodingtag=-1;
    m_cbr_base = 0;
    m_cbr_frame_size = 0;
  }
  
public:

  ~mp3_index()
  {
    delete m_framefile;
    m_framefile=0; 
  }

  bool has_file_open() const { return m_framefile != NULL; } // only signifies whether a file is open (file could be closed but index is valid)

  bool IsCBR() const { return m_cbr_frame_size>0; }

  int GetFrameCount() 
  {
    return m_cbr_frame_size>0||m_framefile||m_numframes==m_frameposmemcache.GetSize()?m_numframes:0;
  }
  
  unsigned int GetStreamStart()
  {
    return GetFramePos(0);
  }

  unsigned int GetSeekPositionForSample(INT64 splpos, int decsr, int *dumpSamples)
  {
    int framesize=(decsr < 32000 ? 576 : 1152);


    // todo: support reading/using index from tag etc
    int frame_pos = (int)(splpos / framesize) - 10; // seek ahead
    if (frame_pos < 0) frame_pos=0;
    else if (frame_pos >= GetFrameCount()) frame_pos=GetFrameCount()-1;

    *dumpSamples = (int) (splpos - (((INT64)frame_pos)*framesize)); 

    return GetFramePos(frame_pos);

  }

  struct mp3_metadata {
    double len;
    int srate, nch;
  };

  static bool quickMetadataRead(const char *fn, WDL_FileRead *fr, mp3_metadata *metadata, bool allow_index_file);
  static mp3_index *indexFromFilename(const char *fn, WDL_FileRead *fr, bool allow_index_file);
  static void release_index(mp3_index *idx)
  {
    WDL_MutexLock lock(&indexMutex);
    if (--idx->m_refcnt<=0)
    {
      int x;
      for (x = 0; x < g_indexes.GetSize() && g_indexes.Get(x) != idx; x ++);
      if (x < g_indexes.GetSize())
        g_indexes.Delete(x);

      delete idx;
    }
  }

private:

  unsigned int GetFramePos(int f) 
  { 
    WDL_MutexLock lock(&m_mutex);
    if (m_framefile && m_numframes > 0)
    {
      m_framefile->SetPosition(m_framestart + f * 4);
      unsigned char buf[4];
      if (m_framefile->Read(buf,4) == 4)
        return buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);        
    }
    else if (m_cbr_frame_size > 0 && f >= 0 && f < m_numframes && WDL_NORMALLY(m_encodingtag==2))
    {
      return m_cbr_base + m_cbr_frame_size * f;
    }
    else if (m_numframes==m_frameposmemcache.GetSize() && f>=0&&f<m_numframes) return m_frameposmemcache.Get()[f];

    return 0;
  }

  int m_refcnt;
  WDL_Mutex m_mutex;
  WDL_String m_fn;
  static WDL_Mutex indexMutex;
  static WDL_PtrList<mp3_index> g_indexes;
  static int _sortfunc(const void *a, const void *b);

  int ReadFrameListFromCache(); // 0 if found
  void BuildFrameList(WDL_FileRead *fr, mp3_metadata *quick_length_check, bool allow_index_file);
  int m_numframes;
  int m_framestart;
  unsigned int m_cbr_base;
  int m_cbr_frame_size;
  WDL_FileRead *m_framefile;

  WDL_TypedQueue<unsigned int> m_frameposmemcache; // only used if !m_framefile, and then only if the mp3 is small enough

public:
  int m_start_eatsamples;
  int m_end_eatsamples;

  int m_encodingtag; // -1=unknown, 0=VBR, 1=ABR, 2=CBR
};


#endif
