#ifndef _BASE_SHM_FIFO_H
#define _BASE_SHM_FIFO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/time.h>

#include "BaseTool.h"

class CBaseSMFIFO
{
public:
  struct SMQTransmit
  {
    long type;
    char text[32];
  };

  CBaseSMFIFO();
  ~CBaseSMFIFO();

  int SMInit(int smkey, int blkGt, int blkSz, bool createEnable);
  int SMWriteDevData(const void *buf, int writeIndex);
  int SMReadDevData(void *buf, int readIndex);
  int SMWriteCfgData(const void *buf, int writeIndex);
  int SMReadCfgData(void *buf, int readIndex);
  int SMWrite(const void *buf, int writeIndex);
  int SMRead(void *buf, int readIndex);
  void SMDetach(bool destroyEnable);

  int MQInit(int mqkey, bool createEnable);
  int MQSend(const void *msg, size_t msgSz, bool wait);
  int MQRecv(void *msg, size_t msgSz, long msgType, bool wait);
  void MQDetach();

private:
  struct SSMHead
  {
    int rdIdx;
    int wrIdx;
    int ahead;
    int blkGt;
    int blkSz;
  };

  struct SSMInfo
  {
    SSMHead *pHead;
    void *pPayload;
    int mqId;
    int smId;
    int semMutex;
  };

  union USemVal {
    int value;
  };

  SSMInfo smInfo;

  void SemLock(int semId);
  void SemUnlock(int semId);
};

#endif
