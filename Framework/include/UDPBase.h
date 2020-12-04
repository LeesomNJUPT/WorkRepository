#ifndef _UPD_BASE_H
#define _UPD_BASE_H

#include "Common.h"
#include "global.h"
// #include <arpa/inet.h>
#include <errno.h>
// #include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define MAXLIST 128
static const char *BROADCAST = "255.255.255.255"; // "255.255.255.255"

class UDPBase
{
public:
  UDPBase();
  virtual ~UDPBase();

  int Initialize(const char *NetEquipment, int UDPPort, UDPCB nHandle = 0);
  bool PushToWriteList(char *mIP, int nPort, unsigned char *SendData, int Len);

private:
  int server_sockfd;
  char strNetEquipment[32];
  bool bThreadRunning;
  UDPCB funHandle;

  Common Comm;
  EquipmentInfo *pNetEquipment;
  pthread_t pRecvThreadId, pProcessListThreadId;
  pthread_attr_t RecvThreadAttr, ProcessListThreadAttr;
  // static pthread_mutex_t udp_mutex;
  pthread_mutex_t udp_mutex;

  int SendUDPData(int Server_FD, char *mIP, int nPort, unsigned char *mData, int nLen);
  int SendUDPBroadCast(int Server_FD, int nPort, unsigned char *mData, int nLen);
  bool StopUDPServer(int nSocket_fd);

  static void *RecvUDPThread(void *lpvParam);
  static void *ProcessUDPList(void *lpvParam);

  int executeCallback(UDPCB p, TransferData *data, int type);

  bool AddToList(pTransferData pINFO);
  bool ReadFromList(pTransferData pINFO);
  bool DeleteList();

  std::vector<TransferData *> udpdatalist;
};
#endif