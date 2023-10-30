#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#include "../../WDL/swell/swell-dlggen.h"
#endif

#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#include "../../WDL/wdlcstring.h"

#include "../reaper_plugin.h"


#include "mp3dec.h"

#define MIN_SIZE_FOR_FILECACHE (12*1024*1024)

#define WDL_WIN32_UTF8_NO_UI_IMPL
#define WDL_WIN32_UTF8_IMPL static
#include "../../WDL/win32_utf8.c"
#include "../../WDL/wdlstring.h"
#include "../../WDL/ptrlist.h"
#include "../../WDL/mutex.h"
extern void (*update_disk_counters)(int read, int write);
#include "../../WDL/filewrite.h"

#include "mp3_index.h"

extern void (*GetPeakFileNameEx2)(const char *fn, char *buf, int bufmax, bool forWrite, const char *extension);

static size_t FILE_WRITE_INT_LE(unsigned int value, WDL_FileWrite *hf)
{
  unsigned char buf[4]={
    (unsigned char) (value&0xff),
    (unsigned char) (value>>8)&0xff,
    (unsigned char) (value>>16)&0xff,
    (unsigned char) (value>>24)&0xff};
  return hf->Write(buf,sizeof(buf));
}

WDL_Mutex mp3_index::indexMutex;
int mp3_index::_sortfunc(const void *a, const void *b)
{
  mp3_index *ta = *(mp3_index **)a;
  mp3_index *tb = *(mp3_index **)b;

  return stricmp(ta->m_fn.Get(),tb->m_fn.Get());
}


WDL_PtrList<mp3_index> mp3_index::g_indexes;
mp3_index *mp3_index::indexFromFilename(const char *fn, WDL_FileRead *fr)
{
  WDL_MutexLock lock(&indexMutex);
  mp3_index tmp(fn);
  mp3_index *t=&tmp;
  mp3_index **_tmp=&t;

  if (g_indexes.GetSize() && (_tmp = (mp3_index **)bsearch(_tmp,g_indexes.GetList(),g_indexes.GetSize(),sizeof(void *),_sortfunc)) && *_tmp)
  {
    t = *_tmp;
    t->m_refcnt++;
    return t;
  }
  else // build and add
  {
    t = new mp3_index(fn);
    // check for cached frame list
    if ((fr && fr->GetSize()<MIN_SIZE_FOR_FILECACHE) ||
          t->ReadFrameListFromCache())
    {

      WDL_FileRead *fpsrc = fr ? fr : new WDL_FileRead(fn,0,65536,4);

      t->BuildFrameList(fpsrc, NULL);

      if (fpsrc != fr) delete fpsrc;
    }
    t->m_refcnt++;

    g_indexes.Add(t);
    qsort(g_indexes.GetList(),g_indexes.GetSize(),sizeof(void*),_sortfunc);

    return t;
  }
}

bool mp3_index::quickMetadataRead(const char *fn, WDL_FileRead *fr, mp3_metadata *metadata)
{
  if (!metadata) return false;
  mp3_index tmp(fn);

  WDL_FileRead *fpsrc = fr ? fr : new WDL_FileRead(fn,0,65536,4);
  tmp.BuildFrameList(fpsrc,metadata);

  if (fpsrc != fr) delete fpsrc;
  return metadata->len > 0.0;
}


int mp3_index::ReadFrameListFromCache() // 0 if found
{
  char cfn[2048];
  if (GetPeakFileNameEx2)
  {
    GetPeakFileNameEx2(m_fn.Get(),cfn,sizeof(cfn)-32,false,".reapindex");
  }
  else
  {
    lstrcpyn(cfn, m_fn.Get(), sizeof(cfn)-32);
    strcat(cfn, ".reapindex");
  }
  struct stat st={0}; 
  if (statUTF8(m_fn.Get(),&st)) return 1;

  delete m_framefile;
  m_framefile=new WDL_FileRead(cfn,0,512,1); 
  if (!m_framefile->IsOpen())
  {
    delete m_framefile;
    m_framefile=0;
    return 1;
  }

  int rv=-1;
  unsigned char buf[16];
  m_framestart=0;
  if (m_framefile->Read(buf,16) == 16 && !memcmp(buf,"RIDX",4))
  {
    unsigned int ft = buf[4] | (buf[5]<<8) | (buf[6]<<16) | (buf[7]<<24);
    unsigned int fs = buf[8] | (buf[9]<<8) | (buf[10]<<16) | (buf[11]<<24);
    unsigned int ni = buf[12] | (buf[13]<<8) | (buf[14]<<16) | (buf[15]<<24);
    INT64 l=m_framefile->GetSize();
    if (ni && ni*4+16+8 <= l && fs == (unsigned int)st.st_size && (abs((int) (ft-st.st_mtime)) < 5 || abs((int)(ft-st.st_mtime-3600)) < 5 || abs((int)(ft-st.st_mtime)+3600) < 5))
    {

      m_framefile->SetPosition(ni*4 + 16);
      if (m_framefile->Read(buf,8)==8)
      {
        m_start_eatsamples = buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
        m_end_eatsamples = buf[4] | (buf[5]<<8) | (buf[6]<<16) | (buf[7]<<24);
      }

      rv=0;
      m_numframes=ni;
      m_framestart=16;
    }
  }

  if (!m_framestart||m_numframes<2||rv)
  {
    delete m_framefile;
    m_framefile=0;
  }

  return rv;
}   
    
void mp3_index::BuildFrameList(WDL_FileRead *fpsrc, mp3_metadata *quick_length_check)
{
  m_start_eatsamples=0;
  m_end_eatsamples=0;

  if (!fpsrc->IsOpen()) return;

  WDL_FileWrite *fpo = NULL;

  if (quick_length_check)
  {
    quick_length_check->len = 0.0;
    quick_length_check->srate = quick_length_check->nch = 0;
  }
  else
  {
    char cfn[2048];
    if (GetPeakFileNameEx2)
    {
      GetPeakFileNameEx2(m_fn.Get(), cfn, sizeof(cfn)-32, true, ".reapindex");
    }
    else
    {
      lstrcpyn(cfn, m_fn.Get(), sizeof(cfn)-32);
      strcat(cfn, ".reapindex");
    }

    if (fpsrc->GetSize() >= MIN_SIZE_FOR_FILECACHE)
    {
      fpo = new WDL_FileWrite(cfn);
      if (!fpo->IsOpen())
      {
        delete fpo;
        fpo=NULL;
      }
    }
  }

  if (fpo)
  {
    struct stat st={0}; 
    statUTF8(m_fn.Get(),&st);

    fpo->Write("RIDX",4);
    FILE_WRITE_INT_LE(st.st_mtime,fpo);
    FILE_WRITE_INT_LE(st.st_size,fpo);
    FILE_WRITE_INT_LE(0,fpo);
  }

  m_frameposmemcache.Clear();

  fpsrc->SetPosition(0);
  WDL_Queue m_filebuf;
  // build frame offset list
  struct frame fr={0,};
  unsigned int lasthdr=0;
  unsigned int byte_pos=0;
  int ni=0;
  int ateof=0;
  int firstblock=1;
  bool firstframe=true;
  while (!ateof || m_filebuf.Available()>32)
  {
    if (m_filebuf.Available() < 32768 && !ateof)
    {
      m_filebuf.Compact();
      char buf[32768];
      int l=fpsrc->Read(buf,sizeof(buf));
      if (!l) ateof=1;

      m_filebuf.Add(buf,l);
      if (firstblock)
      {
        firstblock=0;
		    if (l>10 && !memcmp(buf,"ID3",3) && buf[3]!=-1 && buf[4]!=-1 && buf[6]>=0&& buf[7]>=0&& buf[8]>=0&& buf[9]>=0)
    		{
				  byte_pos=10 + (((int)buf[6])<<21);
				  byte_pos+=((int)buf[7])<<14;
				  byte_pos+=((int)buf[8])<<7;
				  byte_pos+=((int)buf[9]);
          if (buf[3]==4 && (buf[5]&0x10)) byte_pos += 10; // skip id3v2.4 footer
          m_filebuf.Clear();
          fpsrc->SetPosition(byte_pos);
          continue;
        }

      }
    }

    if (m_filebuf.Available()<32) break; 


    unsigned char *in_ptr = (unsigned char *)m_filebuf.Get();
    unsigned int this_header=(in_ptr[0]<<24)|(in_ptr[1]<<16)|(in_ptr[2]<<8)|in_ptr[3];
    unsigned char *this_header_ptr = in_ptr;

    if (!lasthdr)
    {
      if (decode_header(&fr,this_header) && m_filebuf.Available() >= 8+fr.framesize)
      {
        in_ptr += 4 + fr.framesize;

        unsigned int next_header=(in_ptr[0]<<24)|(in_ptr[1]<<16)|(in_ptr[2]<<8)|in_ptr[3];

        if (!mp3_decoder::CompareHeader(this_header,next_header) &&
            decode_header(&fr,next_header))
              lasthdr=this_header;
      }
    }

    if (lasthdr && !mp3_decoder::CompareHeader(lasthdr,this_header) && 
        decode_header(&fr,this_header) && m_filebuf.Available() >= 4+fr.framesize)
    {
      if (firstframe)
      {
        // probably better defaults to use, on layer2 etc too?
        unsigned int tag_frame_cnt = 0;
        int frame_len_samples = 1152;
        if (fr.lay == 3)
        {
          m_start_eatsamples = 0;

          unsigned char *rdbuf = this_header_ptr;

          rdbuf+=4;//skip hdr

          if (!fr.lsf) rdbuf += fr.mode==3 ? 17 : 32;
          else rdbuf+=fr.mode==3 ? 9 : 17;

          if (fr.lsf) frame_len_samples = 576;

          // check for Xing/lame info tag. 

          m_encodingtag=-1; // unknown

          if (!memcmp(rdbuf,"Xing",4) || !memcmp(rdbuf,"Info",4))
          {
            if (!memcmp(rdbuf, "Info", 4)) m_encodingtag=2; // CBR

            int flags =(rdbuf[4]<<24)|(rdbuf[5]<<16)|(rdbuf[6]<<8)|rdbuf[7];
            rdbuf+=8;
            if (flags & 1)  // frames
            {
              if (quick_length_check)
              {
                tag_frame_cnt = 1 + ((rdbuf[0]<<24)|(rdbuf[1]<<16)|(rdbuf[2]<<8)|rdbuf[3]);
              }
              rdbuf+=4;
            }
            if (flags & 2) { rdbuf+=4; } // bytes
            if (flags & 4) { rdbuf+=100; } // toc
            if (flags & 8) { rdbuf+=4; } // vbrscale
            
            // http://gabriel.mp3-tech.org/mp3infotag.html
            if (m_encodingtag < 0 && rdbuf[5] == '.')  // eg. LAME3.88a
            {
              unsigned char c = rdbuf[9]&0xF;
              if (c == 1 || c == 8) m_encodingtag=2;  // CBR
              else if (c == 2 || c == 9) m_encodingtag=1; // ABR
              else if (c >= 2 && c <= 6) m_encodingtag=0; // VBR
            }

            // todo: if has toc, and not writing index, set frame list to alternate format, etc

            rdbuf+=21;

            m_start_eatsamples = (rdbuf[0] << 4) + ((rdbuf[1] >> 4)&0xf);
            m_end_eatsamples = (int)rdbuf[2] + (((int)rdbuf[1] & 0x0F) << 8);

            if ((unsigned int)m_end_eatsamples > 3200) m_end_eatsamples=0;
            if ((unsigned int)m_start_eatsamples > 3200) m_end_eatsamples=m_start_eatsamples=0; // invalid start = reset to defaults                      
          }
          else 
          {
            // no tag -- default handling?
            if (!fr.lsf) m_start_eatsamples-=576;         
            
          }

          m_start_eatsamples+=529+frame_len_samples;
          m_end_eatsamples -= 529;
          if (m_end_eatsamples<0)m_end_eatsamples=0;
        }
        else
        {
          m_start_eatsamples += 482; // approx layer 2 latency, geh. lame actually skips 240 samples less, but this seems mo betta for twolame encoded files at least
        }
  
        if (quick_length_check)
        {
          if (!tag_frame_cnt && WDL_NORMALLY(fr.framesize))
            tag_frame_cnt = (unsigned int) ((fpsrc->GetSize() - byte_pos) / (fr.framesize+4));
          quick_length_check->len = (frame_len_samples * (double)tag_frame_cnt - m_start_eatsamples - m_end_eatsamples) / (double)fr.get_sample_rate();
          quick_length_check->srate = fr.get_sample_rate();
          quick_length_check->nch = fr.get_channels();
          return;
        }

        firstframe=false;
      }

      if (!fpo) m_frameposmemcache.Add(&byte_pos,1);
      else
      {
        FILE_WRITE_INT_LE(byte_pos,fpo);
      }

      ni++;
      byte_pos+=4+fr.framesize;
      if (4+fr.framesize > m_filebuf.Available()) 
      {
        break;
      }

      m_filebuf.Advance(4+fr.framesize);
      lasthdr=this_header;
    }
    else
    {
      byte_pos++;
      m_filebuf.Advance(1); // ugh
    }
  }  
  
  if (fpo)
  {
    FILE_WRITE_INT_LE(m_start_eatsamples,fpo);
    FILE_WRITE_INT_LE(m_end_eatsamples,fpo);

    fpo->SetPosition(12);
    FILE_WRITE_INT_LE(ni,fpo);
    delete fpo;

    ReadFrameListFromCache();
  }
  else
  {
    m_numframes=ni;
  }

}


