/*
 reaper_raw
 Simple PCM_source example plug-in
 Copyright (C) 2006-2008 Cockos Incorporated

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.



  This project makes a simple 16 bit stereo 44khz .RAW file reader.

*/


#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#endif

#include <stdio.h>
#include <math.h>

#include "resource.h"

#define REAPERAPI_IMPLEMENT
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_PeakBuild_Create
#define REAPERAPI_WANT_format_timestr
#define REAPERAPI_WANT_update_disk_counters
#define REAPERAPI_WANT_PCM_Source_CreateFromSimple
#define REAPERAPI_WANT_GetPreferredDiskWriteMode
#define REAPERAPI_WANT_get_ini_file
#define REAPER_PLUGIN_FUNCTIONS_IMPL_LOADFUNC
#include "../reaper_plugin.h"
#include "reaper_plugin_functions.h"

#include "../../WDL/fileread.h"

REAPER_PLUGIN_HINSTANCE g_hInst;


#define SOURCE_TYPE "RAW"

#define RAW_BPS 16 // 16 bit
#define RAW_NCH 2
#define RAW_SRATE 44100


class RAW_SimpleMediaDecoder : public ISimpleMediaDecoder
{
public:
  RAW_SimpleMediaDecoder()
  {
    m_filename=0;
    m_isopened=0;
    m_nch=0;
    m_bps=0;
    m_srate=0.0;
    m_length=0;

    m_fh=0;
    // todo: initialize any basic decoder stuff (i.e. set pointers to 0 etc)
  }
  ~RAW_SimpleMediaDecoder()
  {
    Close(true);
    free(m_filename);
  }

  ISimpleMediaDecoder *Duplicate()
  {
    RAW_SimpleMediaDecoder *r=new RAW_SimpleMediaDecoder;
    free(r->m_filename);
    r->m_filename = m_filename ? strdup(m_filename) : NULL;
    return r;
  }

  void Open(const char *filename, int diskreadmode, int diskreadbs, int diskreadnb)
  {
    Close(filename && strcmp(filename,m_filename?m_filename:""));
    if (filename) 
    {
      free(m_filename);
      m_filename=strdup(filename);
    }

    m_isopened=true;

    m_length=0;
    m_bps=0;
    m_nch=0;
    m_srate=0;

    m_fh = new WDL_FileRead(m_filename ? m_filename : "",diskreadmode,diskreadbs,diskreadnb);
    if (!m_fh->IsOpen())
    {
      delete m_fh;
      m_fh=0;
    }
    else
    {
      // create your decoder here. 
      // If the decoder already exists, check to make sure
      // that the file timestamp matches.
      m_bps=RAW_BPS;
      m_nch=RAW_NCH;
      m_srate=RAW_SRATE;

      int blockAlign=m_nch*(m_bps/8);
      if (blockAlign<1)blockAlign=1;
      m_length = m_fh->GetSize()/blockAlign;
    }

  }

  void Close(bool fullClose)
  {
    if (fullClose)
    {
      // delete any decoder data

    }
    delete m_fh;
    m_fh=0;
    m_isopened=false;
  }

  const char *GetFileName() { return m_filename?m_filename:""; }
  const char *GetType() { return SOURCE_TYPE; }

  void GetInfoString(char *buf, int buflen, char *title, int titlelen)
  {
    lstrcpyn(title,"RAW File Properties",titlelen);

    if (IsOpen())
    {
      // todo: add any decoder specific info
      char temp[4096],lengthbuf[128];
      format_timestr((double) m_length / (double)m_srate,lengthbuf,sizeof(lengthbuf));
      sprintf(temp,"Length: %s:\r\n"
                   "Samplerate: %.0f\r\n"
                   "Channels: %d\r\n"
                   "Bits/sample: %d\r\n",  
            lengthbuf,m_srate,m_nch,m_bps);
        
      lstrcpyn(buf,temp,buflen);
    }
    else if (!m_isopened) lstrcpyn(buf,"Media offline",buflen);
    else if (m_fh) lstrcpyn(buf,"Error initializing decoder",buflen);
    else lstrcpyn(buf,"Error opening file",buflen);
  }

  bool IsOpen()
  {
    return m_fh && m_bps && m_nch && m_srate>0; // todo: check to make sure decoder is initialized properly
  }

  int GetNumChannels() { return m_nch; }
  int GetBitsPerSample() { return m_bps; }
  double GetSampleRate() { return m_srate; }
  INT64 GetLength() { return m_length; }
  INT64 GetPosition() { return m_lastpos; }

  void SetPosition(INT64 pos)
  {
    if (m_fh)
    {
      // todo: if decoder, seek decoder (rather than file)
      m_fh->SetPosition((m_lastpos=pos) * (m_bps/8) * m_nch);
    }
  }

  int ReadSamples(double *buf, int length)
  {
    int rd=0;

    if (m_fh)
    {
      int blockAlign=m_nch*(m_bps/8);
      if (blockAlign<1)blockAlign=1; // should never happen but we hate div0's

      // see if we're at eof
      if (m_lastpos+length > m_length)
        length = m_length - m_lastpos;

      if (length > 0)
      {
        unsigned char *rdbuf=m_diskreadbuf.Resize(length*blockAlign,false);
        rd = m_fh->Read(rdbuf,length*blockAlign) / blockAlign;

        if (rd>0)
        {
          if (update_disk_counters) update_disk_counters(rd*blockAlign,0);

          if (m_bps==16) // we only support 16 bit
          {
            int x,sz=rd*m_nch;
            for (x = 0; x < sz; x ++)
            {
              buf[x]=((short)(rdbuf[0] | ((int)rdbuf[1]<<8)))/32768.0;
              rdbuf += 2;
            }
          }
          else memset(buf,0,rd*m_nch*sizeof(double));      
        }
      }
    }

    m_lastpos+=rd;

    return rd;
  }
private:

  WDL_FileRead *m_fh;
  
  char *m_filename;
  int m_isopened;

  WDL_TypedBuf<unsigned char> m_diskreadbuf;

  int m_nch, m_bps;
  double m_srate;
  INT64 m_lastpos;
  INT64 m_length; // length in sample-frames
};







PCM_source *CreateFromType(const char *type, int priority)
{
  if (priority > 4) // let other plug-ins override "RAW" if they want
  {
    if (!strcmp(type,SOURCE_TYPE))
      return PCM_Source_CreateFromSimple(new RAW_SimpleMediaDecoder,NULL);
  }

  return NULL;
}

PCM_source *CreateFromFile(const char *filename, int priority)
{
  int lfn=strlen(filename);
  if (priority > 4 && lfn>4 && !stricmp(filename+lfn-4,".raw"))
  {
    PCM_source *w=PCM_Source_CreateFromSimple(new RAW_SimpleMediaDecoder,filename);
    if (w->IsAvailable() || priority >= 7) return w;
    delete w;
  }
  return NULL;
}

  // this is used for UI only, not so muc
const char *EnumFileExtensions(int i, const char **descptr) // call increasing i until returns a string, if descptr's output is NULL, use last description
{
  if (i == 0)
  {
    if (descptr) *descptr = "RAW files";
    return "RAW";
  }
  if (descptr) *descptr=NULL;
  return NULL;
}




pcmsrc_register_t myRegStruct={CreateFromType,CreateFromFile,EnumFileExtensions};



extern pcmsink_register_t mySinkRegStruct; // from pcmsink_raw.cpp


extern "C"
{

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec)
{
  g_hInst=hInstance;
  if (rec)
  {
    if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || !REAPERAPI_LoadAPI(rec->GetFunc))
      return 0;

    if (!rec->Register || 
        !rec->Register("pcmsrc",&myRegStruct) || 
        !rec->Register("pcmsink",&mySinkRegStruct))
      return 0;


    // our plugin registered, return success

    return 1;
  }
  else
  {
    return 0;
  }
}

};   

