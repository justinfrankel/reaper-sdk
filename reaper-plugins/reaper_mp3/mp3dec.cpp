#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#endif

#include <math.h>

#include "mp3dec.h"


mp3_decoder::mp3_decoder()
{
  m_sync_skipped_bytes=0;
  m_sync_frame=0;
  m_sync_state=0;
  m_sync_mode=2; // look at following frame to make sure it matches
  memset(&m_lastframe,0,sizeof(m_lastframe));
}

mp3_decoder::~mp3_decoder()
{
}


void mp3_decoder::Reset(bool full)
{
  queue_samples_out.Clear();

  queue_bytes_in.Clear();

  m_lastframe.framesize=0;
  m_sync_skipped_bytes=0;
  m_decoder.reset();
  if (full)
  {
    memset(&m_lastframe,0,sizeof(m_lastframe));
    m_sync_frame=0;
    m_sync_state=0;
  }
}

#define MAX_BAD_BYTES 256*1024


bool mp3_decoder::CompareHeader(unsigned int ref, unsigned int tf)
{
  unsigned int diff=ref^tf;
  if (diff&(3<<19)) return true; // different mpeg 1/2/2.5
  if ((diff>>17)&3) return true; // different layer
  if ((diff>>10)&0x3) return true; // different sample rate

  bool mono1=((ref>>6)&0x3)==3;
  bool mono2=((tf>>6)&0x3)==3;
  if (mono1 != mono2) return true; // must not change channels

  return false;
}

int mp3_decoder::Run() // return -1 on error that can't be recovered from, otherwise if output data doesn't change, assume more input needed
{
  if (m_sync_skipped_bytes > MAX_BAD_BYTES) return -1;

  if (!m_lastframe.framesize)
  {
    while (queue_bytes_in.Available() > 4)
    {
      unsigned char *in_ptr = (unsigned char *)queue_bytes_in.Get();
      unsigned int this_header=(in_ptr[0]<<24)|(in_ptr[1]<<16)|(in_ptr[2]<<8)|in_ptr[3];
      if ((m_sync_frame && CompareHeader(m_sync_frame,this_header)) || !decode_header(&m_lastframe,this_header))
      {
        m_sync_state=0;
        queue_bytes_in.Advance(1);
        if (m_sync_skipped_bytes++ > MAX_BAD_BYTES) 
        {
//          queue_bytes_in.Compact();
          return -1;
        }
      }
      else
      {
        m_sync_state=1;
        queue_bytes_in.Advance(4);
        m_sync_frame=this_header;
        if (queue_bytes_in.Available() < m_lastframe.framesize) return 0;
        break;
      }
    }
  }

  if (m_lastframe.framesize && queue_bytes_in.Available() >= m_lastframe.framesize)
  {
    if (m_sync_state < m_sync_mode)
    {
      if (queue_bytes_in.Available() >= m_lastframe.framesize+4) // verify next frame if available
      {
        unsigned char *in_ptr = (unsigned char *)queue_bytes_in.Get() + m_lastframe.framesize;
        unsigned int this_header=(in_ptr[0]<<24)|(in_ptr[1]<<16)|(in_ptr[2]<<8)|in_ptr[3];
        if (CompareHeader(m_sync_frame,this_header)) 
        {
          // ditch this frame, resync
          m_lastframe.framesize=0;
          return 0;
        }
      }
      m_sync_state=m_sync_mode;
      //look ahead 
    }


    // decode frame
    int ns=m_lastframe.get_sample_count() * m_lastframe.get_channels();
    m_spltmp.Resize(ns,false);
    int done=0;

    int ret=m_decoder.decode(m_sync_frame,&m_lastframe,(unsigned char *)queue_bytes_in.Get(),m_lastframe.framesize,m_spltmp.Get(),m_lastframe.get_sample_count(),&done);

    if (ret == MP3_NEED_MORE) 
    {
      
      memset(m_spltmp.Get(),0,ns*sizeof(double)); // bit resevoir empty -- zero samples instead -- usually this will be after a seek anyway
//       old behavior (wrong): return 0; // try again in a bit. this should never happen since we parse our frame ourself anyway
    }

    if (ret == MP3_ERR)  // resync I guess, this shouldnt really happen much
    {
      m_lastframe.framesize=0;
      return 0;
    }

    queue_bytes_in.Advance(m_lastframe.framesize);
//    queue_bytes_in.Compact();

    queue_samples_out.Add(m_spltmp.Get(),ns*sizeof(double));

    m_lastframe.framesize=0;

  }

  return 0;

}