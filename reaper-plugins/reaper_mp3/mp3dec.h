#ifndef _MP3DEC_H_
#define _MP3DEC_H_

extern void (*gOnMallocFailPtr)(int);
#ifndef WDL_HEAPBUF_ONMALLOCFAIL
#define WDL_HEAPBUF_ONMALLOCFAIL(x) if (gOnMallocFailPtr) gOnMallocFailPtr(x);
#endif

#include "mpglib/mpglib.h"

#include "../../WDL/queue.h"

class mp3_decoder
{
public:
  mp3_decoder();
  ~mp3_decoder();

  WDL_Queue queue_samples_out;
  WDL_Queue queue_bytes_in;

  void Reset(bool full);

  int SyncState() { return m_sync_state >= m_sync_mode; } // returns 1 if synched

  int Run(); // returns -1 on non-recoverable error
  void SetSyncMode(int extraFrame) { m_sync_mode=extraFrame?2:1; }

  int GetByteRate() { return m_lastframe.get_bitrate()/8; }
  double GetFrameRate() { 
    if (m_lastframe.get_sample_rate() >= 32000) return m_lastframe.get_sample_rate()/1152.0;
    else return m_lastframe.get_sample_rate()/576.0;
  }
  int GetSampleRate() { return m_lastframe.get_sample_rate(); } 
  int GetNumChannels() { return m_lastframe.get_channels(); } 
  int GetLayer() { return m_lastframe.lay; }
  int GetChannelMode() { return m_lastframe.mode; }
  
  static bool CompareHeader(unsigned int ref, unsigned int tf); // returns 0 if this decoder would let it pass

  struct frame m_lastframe;

private:
  mpglib m_decoder,m_peakdec;
  WDL_TypedBuf<double> m_spltmp;

  int m_sync_mode;
  int m_sync_skipped_bytes;
  unsigned int m_sync_frame;
  int m_sync_state;
};



#endif
