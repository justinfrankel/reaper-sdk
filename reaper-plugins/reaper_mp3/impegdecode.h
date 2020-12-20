#ifndef _IMPEGDECODE_H_
#define _IMPEGDECODE_H_

class IMPEGDecoder
{
public:
  virtual ~IMPEGDecoder() { }
  
  virtual bool WantInput()=0;
  virtual void AddInput(const void *buf, int buflen)=0;

  virtual double *GetOutput(int *avail)=0; // avail returns number of doubles avail
  virtual void OutputAdvance(int num)=0;  // num = number of doubles

  virtual void Reset(bool full)=0;
  virtual int SyncState()=0; // > 0 if synched
  virtual int Run()=0; // returns -1 on non-recoverable error
  virtual int GetSampleRate()=0;
  virtual int GetNumChannels()=0;
};

#endif