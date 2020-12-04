#ifndef _BASE_UPD_H
#define _BASE_UPD_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Queue.h"

class CBaseUDP
{
public:
  enum EUDPTransmitType
  {
    SEND,
    RECV
  };

  struct SUDPTransmit
  {
    EUDPTransmitType type;
    uint8_t *data;
    size_t dataLen;
    char srcIP[INET_ADDRSTRLEN];
    uint16_t srcPort;
    char destIP[INET_ADDRSTRLEN];
    uint16_t destPort;
  };

private:
  int fd;
  int (*pCallback)(SUDPTransmit *);
  CQueue<SUDPTransmit> sendQue;
  CQueue<SUDPTransmit> recvQue;
  pthread_t recvDataTID, handleSendQueTID, handleRecvQueTID;

  int StopUDPRecv();

  static void *HandleSendQue(void *pParam);
  size_t SendTo(SUDPTransmit *sendInfo);

  static void *HandleRecvQue(void *pParam);
  int ExecuteCallback(SUDPTransmit *recvInfo);

  static void *RecvData(void *pParam);
  size_t RecvFrom();

public:
  CBaseUDP();
  ~CBaseUDP();

  int Init(int (*pArgCallback)(SUDPTransmit *), uint16_t port = 0);
  int SetMulticast(const char *addr);
  int Send(const char *ip, uint16_t port, uint8_t *data, size_t dataLen);
};
#endif