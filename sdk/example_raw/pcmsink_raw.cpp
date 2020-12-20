/*
 reaper_raw
 Simple PCM_sink example plug-in
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



  This file implements a raw sink, for generating .raw files.
*/


#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#include "../../WDL/swell/swell-dlggen.h"
#endif

#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#include "resource.h"

#include "../reaper_plugin.h"
#include "reaper_plugin_functions.h"

#include "../../WDL/wdlstring.h"
#include "../../WDL/filewrite.h"

#define SINK_FOURCC REAPER_FOURCC('r','a','w',' ')

extern HINSTANCE g_hInst;

static void doubletomem16(double in, unsigned char *outp)
{
  short out;
  if ((in)<0.0) { if ((in) <= -1.0) (out) = -32768; else (out) = (short) ((int)(((in) * 32768.0)-0.5)); }
	else { if ((in) >= (32766.5/32768.0)) (out) = 32767; else (out) = (short) (int)((in) * 32768.0 + 0.5); }
  outp[0] = out&0xff;
  outp[1] = out>>8;
}

class PCM_sink_raw : public PCM_sink
{
  public:
    static HWND showConfig(void *cfgdata, int cfgdata_l, HWND parent);

    PCM_sink_raw(const char *fn, void *cfgdata, int cfgdata_l, 
                 int nch, int srate, bool buildpeaks)
    {
      m_peakbuild=0;
      m_bps = 16;
      if (cfgdata_l >= 8 && *((int *)cfgdata) == SINK_FOURCC)
      {
        m_bps=REAPER_MAKELEINT(((int *)(((unsigned char *)cfgdata)+4))[0]);
      }
      m_nch=nch>1?2:1;
      m_srate=srate;
      m_lensamples=0;
      m_fn.Set(fn);

      int wm=1,nb[2]={16,128},bs=65536;
      if (GetPreferredDiskWriteMode) GetPreferredDiskWriteMode(&wm,nb,&bs);
      m_file=new WDL_FileWrite(fn,wm,bs,nb[0],nb[1]);
      if (m_file && !m_file->IsOpen())
      {
        delete m_file; 
        m_file=0;
      }

      if (buildpeaks && m_file)
      {
        m_peakbuild=PeakBuild_Create(NULL,fn,m_srate,m_nch);
      }
    }

    bool IsOpen()
    {
      if (!m_file) return false;
      return 1;
    }

    ~PCM_sink_raw()
    {
      delete m_file;
      m_file=0;
      delete m_peakbuild;
      m_peakbuild=0;
    }

    const char *GetFileName() { return m_fn.Get(); }
    int GetNumChannels() { return m_nch; } // return number of channels
    double GetLength() { return m_lensamples / (double) m_srate; } // length in seconds, so far
    INT64 GetFileSize()
    {
      return m_file ? m_file->GetSize() : 0;
    }

    void GetOutputInfoString(char *buf, int buflen)
    {
      char tmp[512];
      sprintf(tmp,"RAW %d bit %dHz %dch",m_bps,m_srate,m_nch);
      lstrcpyn(buf,tmp,buflen);
    }

    bool WantMIDI() { return false; }

    void WriteMIDI(MIDI_eventlist *events, int len, double samplerate) 
    { 
      // this would only be called anyway if we respond 1 to WantMIDI()
    }
    void WriteDoubles(double **samples, int len, int nch, int offset, int spacing)
    {
      if(!m_file) return;

      if (m_peakbuild)
        m_peakbuild->ProcessSamples(samples,len,nch,offset,spacing);

      unsigned char *bout=m_tmpbuf.Resize(len*m_nch*(m_bps/8),false);
      // write samples to disk
      int ch;
      for (ch = 0; ch < m_nch; ch ++) // write the format's numchannels
      {
        double *in=ch < nch ? samples[ch] : samples[nch-1];  //if the input chancount is less, make sure we feed it valid inputs for the higher channels

        int x;

        if (ch < nch || (ch==1 && nch==1))
          for (x = 0; x < len; x ++) doubletomem16(in[x],bout+(x*m_nch+ch)*(m_bps/8));
        else
          for (x = 0; x < len; x ++) memset(bout+(x*m_nch+ch)*(m_bps/8),0,2);
      }

      m_file->Write(bout,m_tmpbuf.GetSize());
      if (update_disk_counters)
        update_disk_counters(0,m_tmpbuf.GetSize());

      m_lensamples+=len;

    } 
    int GetLastSecondPeaks(int sz, double *buf)
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

 private:
    WDL_TypedBuf<unsigned char> m_tmpbuf;

    int m_nch,m_srate, m_bps, m_qual;
    INT64 m_lensamples;
    WDL_String m_fn;
    WDL_FileWrite *m_file;
    REAPER_PeakBuild_Interface *m_peakbuild;
};

static unsigned int GetFmt(const char **desc) 
{
  if (desc) *desc="RAW";
  return SINK_FOURCC;
}

static const char *GetExtension(const void *cfg, int cfg_l)
{
  if (cfg_l >= 4 && *((int *)cfg) == SINK_FOURCC) return "raw";
  return NULL;
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

int SinkGetConfigSize() { return 8; }

void SinkInitDialog(HWND hwndDlg, void *cfgdata, int cfgdata_l)
{
  
  
  int bps = 16;
  if (cfgdata_l < 8 || *((int *)cfgdata) != SINK_FOURCC)
    cfgdata_l=LoadDefaultConfig(&cfgdata,"raw encoder defaults");
  
  if (cfgdata_l>=8 && ((int*)cfgdata)[0] == SINK_FOURCC)
  {
    bps= REAPER_MAKELEINT(((int *)(((unsigned char*)cfgdata)+4))[0]);
  }

  // todo: show conifguration

}

void SinkSaveState(HWND hwndDlg, void *_data)
{
  int bps = 16;

  // todo: get state from dialog
  
  ((int *)_data)[0] = SINK_FOURCC;
  ((int *)(((unsigned char *)_data)+4))[0]=REAPER_MAKELEINT(bps);
}

void SaveDefaultConfig(HWND hwndDlg)
{
  char data[1024];
  SinkSaveState(hwndDlg,data);
  int l=SinkGetConfigSize();
  const char *desc="raw encoder defaults";
  const char *fn=get_ini_file();
  char buf[64]; 
  sprintf(buf,"%d",l);
  WritePrivateProfileString(desc,"default_size",buf,fn);
  WritePrivateProfileStruct(desc,"default",data,l,fn);
}

BOOL WINAPI wavecfgDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      {
        void *cfgdata=((void **)lParam)[0];
        int cfgdata_l=(int)((void**)lParam)[1];
        SinkInitDialog(hwndDlg,cfgdata,cfgdata_l);
        

      }
    return 0;
    case WM_USER+1024:
      {
        if (wParam) *((int *)wParam)=SinkGetConfigSize();
        if (lParam)
        {
          SinkSaveState(hwndDlg,(void*)lParam);
          

        }
      }
    return 0;
    case WM_DESTROY:
      {
        SaveDefaultConfig(hwndDlg);
      }
    return 0;

  }
  return 0;
}

static HWND ShowConfig(const void *cfg, int cfg_l, HWND parent)
{
  if (cfg_l >= 4 && *((int *)cfg) == SINK_FOURCC) 
  {
    const void *x[2]={cfg,(void *)cfg_l};
    return CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_RAWSINK_CFG),parent,wavecfgDlgProc,(LPARAM)x);
  }
  return 0;
}

#ifndef _WIN32 // this is for mac only

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_RAWSINK_CFG,SWELL_DLG_WS_CHILD|SWELL_DLG_WS_FLIPPED|SWELL_DLG_WS_NOAUTOSIZE,"",243,45,1.8)

BEGIN
END

SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_RAWSINK_CFG)

#endif

static PCM_sink *CreateSink(const char *filename, void *cfg, int cfg_l, int nch, int srate, bool buildpeaks)
{
  if (cfg_l >= 4 && *((int *)cfg) == SINK_FOURCC) 
  {
    PCM_sink_raw *v=new PCM_sink_raw(filename,cfg,cfg_l,nch,srate,buildpeaks);
    if (v->IsOpen()) return v;
    delete v;
  }
  return 0;
}

pcmsink_register_t mySinkRegStruct={GetFmt,GetExtension,ShowConfig,CreateSink};
