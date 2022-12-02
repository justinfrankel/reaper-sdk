#include "osc.h"
#include "../../WDL/ptrlist.h"
#include "../../WDL/wdlcstring.h"
#include "../../WDL/jnetlib/jnetlib.h"

#include "../reaper_plugin.h"

#ifdef _WIN32
#include <process.h>

#pragma comment (lib,"wsock32.lib")
#else
#include "../../WDL/swell/swell.h"
#endif

#include "../../WDL/setthreadname.h"

WDL_PtrList<OscHandler> s_osc_handlers;

static HANDLE s_thread=0;
static bool s_threadquit=false;


#define OSC_DEBUG_INPUT 0
#define OSC_DEBUG_OUTPUT 0


extern void (*CSurf_OnOscControlMessage2)(const char*, const float*, const char *arg_str);

static unsigned WINAPI OscThreadProc(LPVOID p)
{
  WDL_SetThreadName("reaper/osc");
  JNL::open_socketlib();

  int sockcnt=0;
  WDL_Queue sendq;
  char hdr[16] = { 0 };
  strcpy(hdr, "#bundle");
  hdr[12]=1; // timetag=immediate

  int i;
  for (i=0; i < s_osc_handlers.GetSize(); ++i)
  {
    OscHandler* osc=s_osc_handlers.Get(i);
    osc->m_recvsock = INVALID_SOCKET;
    osc->m_sendsock = INVALID_SOCKET;

    if (osc->m_recv_enable && osc->m_recvaddr.sin_port>0)
    {
      osc->m_recvsock=socket(AF_INET, SOCK_DGRAM, 0);
      if (osc->m_recvsock != INVALID_SOCKET)
      {
        SET_SOCK_DEFAULTS(osc->m_recvsock);
        int on=1;
        setsockopt(osc->m_recvsock, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof(on));
        if (!bind(osc->m_recvsock, (struct sockaddr*)&osc->m_recvaddr, sizeof(struct sockaddr))) 
        {
          SET_SOCK_BLOCK(osc->m_recvsock, false);
          ++sockcnt;
        }
        else
        {
          shutdown(osc->m_recvsock, SHUT_RDWR);
          closesocket(osc->m_recvsock);
          osc->m_recvsock=INVALID_SOCKET;
        }
      }
    }

    if (osc->m_send_enable && osc->m_sendaddr.sin_port>0)
    {
      osc->m_sendsock=socket(AF_INET, SOCK_DGRAM, 0);
      if (osc->m_sendsock != INVALID_SOCKET)
      {
        int on=1;
        SET_SOCK_DEFAULTS(osc->m_sendsock);
        setsockopt(osc->m_sendsock, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof(on));
        ++sockcnt;
      }
    }
  }

  if (sockcnt)
  {
    while (!s_threadquit)
    {
      char buf[MAX_PACKET_SIZE];
      bool hadmsg=false;

      for (i=0; i < s_osc_handlers.GetSize(); ++i)
      {
        OscHandler* osc=s_osc_handlers.Get(i);

        if (osc->m_recvsock != INVALID_SOCKET ||
            (osc->m_recv_enable && 
             osc->m_recvaddr.sin_port==0 &&
             osc->m_sendsock != INVALID_SOCKET)
            )
        {
          buf[0]=0;

          int len;
          if (osc->m_recvsock != INVALID_SOCKET)
          {
            socklen_t socklen = sizeof(osc->m_last_recv_addr);
            len = recvfrom(osc->m_recvsock, buf, sizeof(buf), 0, (struct sockaddr*)&osc->m_last_recv_addr, &socklen);
          }
          else
          {
            SET_SOCK_BLOCK(osc->m_sendsock, false);
            len = recvfrom(osc->m_sendsock, buf, sizeof(buf), 0, 0, 0);
            SET_SOCK_BLOCK(osc->m_sendsock, true);
          }
          if (len > 0)
          { 
            // unpacking bundles becomes trivial
            // if we store the packet length as big-endian
            int tlen=len;
            REAPER_MAKEBEINTMEM((char*)&tlen);

            osc->m_mutex.Enter();
            osc->m_recvq.Add(&tlen, sizeof(int));
            osc->m_recvq.Add(buf, len);
            osc->m_mutex.Leave();

            if (osc->m_recv_enable&4) // just listening
            {
              int j;
              for (j=i+1; j < s_osc_handlers.GetSize(); ++j)
              {
                OscHandler* osc2=s_osc_handlers.Get(j);
                if (osc2->m_recv_enable && 
                    !memcmp(&osc2->m_recvaddr, &osc->m_recvaddr, sizeof(struct sockaddr_in)))
                {
                  osc2->m_mutex.Enter();
                  osc2->m_recvq.Add(&tlen, sizeof(int));
                  osc2->m_recvq.Add(buf, len);
                  osc2->m_mutex.Leave();
                }
              }
            }

            hadmsg=true;
          }
        }

        if ((osc->m_sendsock != INVALID_SOCKET ||
             (osc->m_recvsock != INVALID_SOCKET && 
              osc->m_send_enable &&
              osc->m_last_recv_addr.sin_port>0 &&
              osc->m_sendaddr.sin_port == 0)
             ) && 
             osc->m_sendq.Available())
        {    
          sendq.Add(hdr, 16);

          osc->m_mutex.Enter();
          sendq.Add(osc->m_sendq.Get(), osc->m_sendq.Available());
          osc->m_sendq.Clear();
          osc->m_mutex.Leave();

          char* packetstart=(char*)sendq.Get();
          int packetlen=16;
          bool hasbundle=false;
          sendq.Advance(16);

          if (osc->m_sendsock == INVALID_SOCKET)
          {
            SET_SOCK_BLOCK(osc->m_recvsock, true);
          }

          const int maxpacket = wdl_min(MAX_PACKET_SIZE,osc->m_maxpacketsz);
          while (sendq.Available() >= sizeof(int))
          {
            int len=*(int*)sendq.Get(); // not advancing
            REAPER_MAKEBEINTMEM((char*)&len);

            if (len < 1 || len > MAX_OSC_MSG_LEN || len > sendq.Available()) break;             
            
            if (packetlen > 16 && packetlen+sizeof(int)+len > maxpacket)
            {
              // packet is full
              if (!hasbundle)
              {
                packetstart += 20;
                packetlen -= 20;
              }

              if (s_threadquit) break;
              if (osc->m_sendsock != INVALID_SOCKET)
              {
                sendto(osc->m_sendsock, packetstart, packetlen, 0, (struct sockaddr*)&osc->m_sendaddr, sizeof(osc->m_sendaddr));
              }
              else if (osc->m_last_recv_addr.sin_port>0)
              {
                sendto(osc->m_recvsock, packetstart, packetlen, 0, (struct sockaddr*)&osc->m_last_recv_addr, sizeof(osc->m_last_recv_addr));
              }
              if (osc->m_sendsleep) Sleep(osc->m_sendsleep);

              packetstart=(char*)sendq.Get()-16; // safe since we padded the queue start
              memcpy(packetstart, hdr, 16);
              packetlen=16;
              hasbundle=false;
            }
         
            if (packetlen > 16) hasbundle=true;
            sendq.Advance(sizeof(int)+len);
            packetlen += sizeof(int)+len;
          }

          if (packetlen > 16)
          {
            if (!hasbundle)
            {
              packetstart += 20;
              packetlen -= 20;
            }

            if (s_threadquit) break;
            if (osc->m_sendsock != INVALID_SOCKET)
            {
              sendto(osc->m_sendsock, packetstart, packetlen, 0, (struct sockaddr*)&osc->m_sendaddr, sizeof(osc->m_sendaddr));
            }
            else if (osc->m_last_recv_addr.sin_port>0)
            {
              sendto(osc->m_recvsock, packetstart, packetlen, 0, (struct sockaddr*)&osc->m_last_recv_addr, sizeof(osc->m_last_recv_addr));
            }
            if (osc->m_sendsleep) Sleep(osc->m_sendsleep);
          }

          if (osc->m_sendsock == INVALID_SOCKET)
          {
            SET_SOCK_BLOCK(osc->m_recvsock, false);
          }

          sendq.Clear();
          hadmsg=true;
        }
      }

      static int sleep_amt;
      if (!hadmsg) 
      {
        if (sleep_amt < 8192) sleep_amt ++;
        Sleep(6 + sleep_amt/512);
      }
      else
      {
        sleep_amt = 0;
      }
    }
  }

  // s_threadquit:
  for (i=0; i < s_osc_handlers.GetSize(); ++i)
  {
    OscHandler* osc=s_osc_handlers.Get(i);
    if (osc->m_recvsock != INVALID_SOCKET)
    {
      shutdown(osc->m_recvsock, SHUT_RDWR);
      closesocket(osc->m_recvsock);
      osc->m_recvsock = INVALID_SOCKET;
    }
    if (osc->m_sendsock != INVALID_SOCKET)
    {
      shutdown(osc->m_sendsock, SHUT_RDWR);
      closesocket(osc->m_sendsock);
      osc->m_sendsock = INVALID_SOCKET;
    }
  }

  JNL::close_socketlib();
  return 0;
}


static void StartOscThread()
{
  unsigned id=0;
  s_thread=(HANDLE)_beginthreadex(0, 0, OscThreadProc, 0, 0, &id);
}

static void KillOscThread()
{
  if (s_thread)
  {
    s_threadquit=true;
    WaitForSingleObject(s_thread, INFINITE);
    CloseHandle(s_thread);
  }
  s_thread=0;
  s_threadquit=false;
}


bool OscInit(OscHandler* osc)
{
  KillOscThread();
  if (s_osc_handlers.Find(osc) < 0)
  {
    // insert at the top, so new listeners will block existing handlers
    s_osc_handlers.Insert(0, osc);
  }

  StartOscThread();
  return true;
}

void OscQuit(OscHandler* osc)
{
  if (osc)
  {
    int i=s_osc_handlers.Find(osc);
    if (i >= 0) 
    {
      KillOscThread();
      s_osc_handlers.Delete(i);
      if (s_osc_handlers.GetSize())
      {
        StartOscThread();
      }
    }
  }
  else
  {
    KillOscThread();
    s_osc_handlers.Empty();
  }
}


int OscGetInput(OscHandler* osc)
{
  int msgcnt=0;

  if (osc->m_recvq.Available() >= sizeof(int))
  {
    static WDL_Queue s_q;
    osc->m_mutex.Enter();
    int sz=osc->m_recvq.Available();
    s_q.Add(osc->m_recvq.Get(), sz);
    osc->m_recvq.Clear();
    osc->m_mutex.Leave();

    while (s_q.Available() >= sizeof(int))
    {
      int len=*(int*)s_q.Get(sizeof(int));
      REAPER_MAKEBEINTMEM((char*)&len);

      if (len <= 0 || len > MAX_PACKET_SIZE || len > s_q.Available()) break;

      if (s_q.Available() > 20 && !strcmp((char*)s_q.Get(), "#bundle"))
      {
        s_q.Advance(16); // past "#bundle" and timestamp
        len=*(int*)s_q.Get(sizeof(int));
        REAPER_MAKEBEINTMEM((char*)&len);

        if (len <= 0 || len > s_q.Available()) break;
      }
      if (len > MAX_OSC_MSG_LEN) break;

      OscMessageRead rmsg((char*)s_q.Get(len), len);

#if OSC_DEBUG_INPUT
      char dump[MAX_OSC_MSG_LEN*2];
      rmsg.DebugDump("recv: ", dump, sizeof(dump));
#ifdef _WIN32
      lstrcatn(dump, "\n",sizeof(dump));
      OutputDebugString(dump);
#else
      fprintf(stderr, "%s\n", dump);
#endif
#endif

      const char* msg=rmsg.GetMessage();
      const float* f=rmsg.PopFloatArg(true);
      const char* sarg=rmsg.PopStringArg(true);

      osc->m_handler(osc->m_obj, &rmsg);
      if (osc->m_recv_enable&2)
      {
        CSurf_OnOscControlMessage2(msg, f, sarg);
      }

      ++msgcnt;
    }    
    s_q.Clear();    
  }

  return msgcnt;
}


void OscSendOutput(OscHandler* osc, OscMessageWrite* wmsg)
{
  if (osc->m_sendsock != INVALID_SOCKET ||
      (osc->m_recvsock != INVALID_SOCKET && 
              osc->m_send_enable &&
              osc->m_sendaddr.sin_port == 0 &&
              osc->m_last_recv_addr.sin_port>0)
     )
  {

#if OSC_DEBUG_OUTPUT
      char dump[MAX_OSC_MSG_LEN*2];
      wmsg->DebugDump("send: ", dump, sizeof(dump));
#ifdef _WIN32
      strcat(dump, "\n");
      OutputDebugString(dump);
#else
      fprintf(stderr, "%s\n", dump);
#endif
#endif

    int len=0;
    const char* msg=wmsg->GetBuffer(&len);
    int tlen=len;
    REAPER_MAKEBEINTMEM((char*)&tlen);
    osc->m_mutex.Enter();
    osc->m_sendq.Add(&tlen, sizeof(int));
    osc->m_sendq.Add(msg, len);
    osc->m_mutex.Leave();
  }
}


OscHandler* OscAddLocalListener(OscHandlerFunc handler, void* obj, int port) 
{
  OscHandler* osc=new OscHandler;
  osc->m_recv_enable=1|4;
  osc->m_recvsock=INVALID_SOCKET;
  osc->m_recvaddr.sin_family=AF_INET;
  osc->m_recvaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  osc->m_recvaddr.sin_port=htons(port);
  osc->m_obj=obj;
  osc->m_handler=handler;

  if (!OscInit(osc))
  {
    delete osc;
    osc=0;
  }
  return osc;
}

void OscRemoveLocalListener(OscHandler* osc)
{
  OscQuit(osc);  
}



void GetLocalIP(char* buf, int buflen)
{   
  buf[0]=0;
  SOCKET sock=socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
  if (sock == INVALID_SOCKET)
  {
    static bool socketlib_init;
    if (!socketlib_init) 
    {
      JNL::open_socketlib();
      socketlib_init=true;
    }
    sock=socket(AF_INET, SOCK_DGRAM, 0);
  }
#endif
  if (sock != INVALID_SOCKET)
  {
    struct sockaddr_in sin = { 0 };
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=inet_addr("8.8.8.8"); // google dns heh
    sin.sin_port=htons(53);
    SET_SOCK_DEFAULTS(sock);
    if (connect(sock, (const sockaddr*)&sin, sizeof(sin)) >= 0)
    {
      struct sockaddr_in name = { 0 };
      socklen_t len=sizeof(name);
      if (getsockname(sock, (sockaddr*)&name, &len) >= 0)
      {
        char* p=inet_ntoa(name.sin_addr);
        if (p) lstrcpyn(buf, p, buflen);
      }
    }
    shutdown(sock, SHUT_RDWR);
    closesocket(sock);
  }
}
