#ifndef _BASE_TCP_CLIENT_H
#define _BASE_TCP_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Queue.h"

class CBaseTCPClient
{
public:
  enum ETCPTransmitType
  {
    SEND,
    RECV,
    DISCONNECT
  };

  struct STCPTransmit
  {
    ETCPTransmitType type;
    uint8_t *data;
    size_t dataLen;
    char srcIP[INET_ADDRSTRLEN];
    uint16_t srcPort;
  };

private:
  int fd;
  char serverIP[INET_ADDRSTRLEN];
  uint16_t serverPort;
  int (*pCallback)(STCPTransmit *);
  CQueue<STCPTransmit> sendQue;
  CQueue<STCPTransmit> recvQue;
  pthread_t recvDataTID, handleSendQueTID, handleRecvQueTID;

  int SetKeepAlive(int idle, int intvl, int cnt);

  static void *HandleSendQue(void *pParam);
  size_t SendToServer(STCPTransmit *sendInfo);

  static void *HandleRecvQue(void *pParam);
  int ExecuteCallback(STCPTransmit *recvInfo);

  static void *RecvData(void *pParam);
  size_t RecvFromServer();

public:
  CBaseTCPClient();
  ~CBaseTCPClient();

  int GetSocketID();
  int Disconnect();
  int ConnectToServer(const char *ip, uint16_t port, int (*pArgCallback)(STCPTransmit *));
  bool IsTCPConnected();
  int Send(uint8_t *data, size_t dataLen);
};

#endif