#ifndef _OSC_H_
#define _OSC_H_

#include <string.h>
#include <stdlib.h>
#include "../../WDL/queue.h"
#include "../../WDL/mutex.h"
#include "../../WDL/jnetlib/netinc.h"
#include "../../WDL/jnetlib/util.h"


#define DEF_RECVPORT 8000
#define DEF_SENDPORT 9000

#define DEF_MAXPACKETSZ 1024
#define MAX_PACKET_SIZE 32768

#define DEF_SENDSLEEP 10
#define MAX_SENDSLEEP 100

#define MAX_OSC_MSG_LEN 1024


class OscMessageRead;
class OscMessageWrite;


typedef bool (*OscHandlerFunc)(void* obj, OscMessageRead* rmsg);


struct OscHandler
{
  OscHandler()
  {
    m_recv_enable=0;
    m_recvsock=INVALID_SOCKET;
    memset(&m_recvaddr, 0, sizeof(m_recvaddr));

    m_send_enable=0;
    m_sendsock=INVALID_SOCKET;
    memset(&m_sendaddr, 0, sizeof(m_sendaddr));
    memset(&m_last_recv_addr,0,sizeof(m_last_recv_addr));

    m_maxpacketsz=DEF_MAXPACKETSZ;
    m_sendsleep=DEF_SENDSLEEP;

    m_obj=0;
    m_handler=0;
  }

  int m_recv_enable; // &1=receive from socket, &2=send messages to reaper kbd system, &4=just listening, thanks
  SOCKET m_recvsock;
  struct sockaddr_in m_recvaddr;
  WDL_Queue m_recvq;

  int m_send_enable; // &1=send to socket
  SOCKET m_sendsock;
  struct sockaddr_in m_sendaddr, m_last_recv_addr;
  WDL_Queue m_sendq;

  int m_maxpacketsz;
  int m_sendsleep;

  WDL_Mutex m_mutex;

  void* m_obj;
  OscHandlerFunc m_handler;
};


bool OscInit(OscHandler* osc);
void OscQuit(OscHandler* osc=0); // 0 for quit all


int OscGetInput(OscHandler* osc);
void OscSendOutput(OscHandler* osc, OscMessageWrite* wmsg);



OscHandler* OscAddLocalListener(OscHandlerFunc handler, void* obj, int port); 
void OscRemoveLocalListener(OscHandler* osc);



class OscMessageRead
{
public:

  OscMessageRead(char* buf, int len); // writes over buf

  const char* GetMessage() const; // get the entire message string, no args
  int GetNumArgs() const;

  const char* PopWord();

  const void *GetIndexedArg(int idx, char *typeOut) const;

  const int* PopIntArg(bool peek, bool peekIfLast=false);
  const float* PopFloatArg(bool peek, bool peekIfLast=false);
  const char* PopStringArg(bool peek, bool peekIfLast=false);

  void DebugDump(const char* label, char* dump, int dumplen);

private:

  char* m_msg_end;
  char* m_type_end;
  char* m_arg_end;

  char* m_msg_ptr;
  char* m_type_ptr;
  char* m_arg_ptr;

  bool m_msgok;
};


class OscMessageWrite
{
public:

  OscMessageWrite();

  bool PushWord(const char* word);
  bool PushInt(int val); // push an int onto the message (not an int arg)

  bool PushIntArg(int val);
  bool PushFloatArg(float val);
  bool PushStringArg(const char* val);

  const char* GetBuffer(int* len);
  
  void DebugDump(const char* label, char* dump, int dumplen);

private:
  
  char m_msg[MAX_OSC_MSG_LEN];
  char m_types[MAX_OSC_MSG_LEN];
  char m_args[MAX_OSC_MSG_LEN];

  char* m_msg_ptr;
  char* m_type_ptr;
  char* m_arg_ptr;
};


// helper
void GetLocalIP(char* buf, int buflen);


#endif // _OSC_H_
