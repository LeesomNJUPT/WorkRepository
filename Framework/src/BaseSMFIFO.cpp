#include "../include/BaseSMFIFO.h"

CBaseSMFIFO::CBaseSMFIFO()
{
}

CBaseSMFIFO::~CBaseSMFIFO()
{
}

void CBaseSMFIFO::SemLock(int semId)
{
    sembuf opBuf[] = {{0, -1, SEM_UNDO}};
    semop(semId, opBuf, 1);
}

void CBaseSMFIFO::SemUnlock(int semId)
{
    sembuf opBuf[] = {{0, 1, SEM_UNDO}};
    semop(semId, opBuf, 1);
}

int CBaseSMFIFO::SMInit(int smkey, int blkGt, int blkSz, bool createEnable)
{
    size_t sizeGt = blkGt * blkSz + sizeof(SSMHead);
    smInfo.smId = shmget(smkey, sizeGt, 0);

    if (-1 == smInfo.smId)
    {
        if (false == createEnable)
        {
            perror("SMInit()");
            return -1;
        }
        smInfo.smId = shmget(smkey, sizeGt, IPC_CREAT | 0666);
        if (-1 == smInfo.smId)
        {
            perror("SMInit()");
            return -1;
        }
        smInfo.pHead = (SSMHead *)shmat(smInfo.smId, NULL, 0);
        memset(smInfo.pHead, 0, sizeGt);
        smInfo.pHead->rdIdx = 0;
        smInfo.pHead->wrIdx = 0;
        smInfo.pHead->ahead = 0;
        smInfo.pHead->blkGt = blkGt;
        smInfo.pHead->blkSz = blkSz;

        smInfo.pPayload = (void *)smInfo.pHead + sizeof(SSMHead);

        smInfo.semMutex = semget(smkey, 1, IPC_CREAT | 0666);

        USemVal sv;
        sv.value = 1;
        semctl(smInfo.semMutex, 0, SETVAL, sv);
    }
    else
    {
        smInfo.pHead = (SSMHead *)shmat(smInfo.smId, NULL, 0);
        smInfo.pPayload = (void *)smInfo.pHead + sizeof(SSMHead);
        smInfo.semMutex = semget(smkey, 0, 0);
    }
    return 0;
}

int CBaseSMFIFO::SMWriteDevData(const void *buf, int writeIndex)
{
    SemLock(smInfo.semMutex);
    int doneIndex = -1;
    void *writePos = NULL;
    // 寻址
    if (writeIndex < 0)
    {
        doneIndex = smInfo.pHead->wrIdx;
    }
    else if (writeIndex < smInfo.pHead->blkGt)
    {
        doneIndex = writeIndex;
    }
    else
    {
        doneIndex = -1;
        SemUnlock(smInfo.semMutex);
        return doneIndex;
    }
    writePos = smInfo.pPayload + doneIndex * smInfo.pHead->blkSz;
    NSBaseTool::SDevData *devData = (NSBaseTool::SDevData *)writePos;
    // 写操作
    if (0 == devData->timestamp)
    {
        memcpy(writePos, buf, smInfo.pHead->blkSz);
    }
    else
    {
        uint64_t realtime;
        timeval ts;
        gettimeofday(&ts, NULL);
        realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
        if ((devData->timestamp + devData->timeout) < realtime)
        {
            memcpy(writePos, buf, smInfo.pHead->blkSz);
        }
        else
        {
            doneIndex = -1;
        }
    }
    // 索引变化
    if (writeIndex < 0)
    {
        smInfo.pHead->wrIdx = (smInfo.pHead->wrIdx + 1) % (smInfo.pHead->blkGt);
        if (0 == smInfo.pHead->wrIdx)
        {
            smInfo.pHead->ahead++;
        }
    }

    SemUnlock(smInfo.semMutex);
    return doneIndex;
}

int CBaseSMFIFO::SMReadDevData(void *buf, int readIndex)
{
    SemLock(smInfo.semMutex);
    int doneIndex = -1;
    void *readPos = NULL;
    // 顺序读时读索引不能超前于写索引
    if (readIndex < 0)
    {
        if ((smInfo.pHead->wrIdx + smInfo.pHead->ahead * smInfo.pHead->blkGt) <= smInfo.pHead->rdIdx)
        {
            doneIndex = -1;
            SemUnlock(smInfo.semMutex);
            return doneIndex;
        }
    }
    // 寻址
    if (readIndex < 0)
    {
        doneIndex = smInfo.pHead->rdIdx;
    }
    else if (readIndex < smInfo.pHead->blkGt)
    {
        doneIndex = readIndex;
    }
    else
    {
        doneIndex = -1;
        SemUnlock(smInfo.semMutex);
        return doneIndex;
    }
    readPos = smInfo.pPayload + doneIndex * smInfo.pHead->blkSz;
    NSBaseTool::SDevData *devData = (NSBaseTool::SDevData *)readPos;
    // 读操作
    if (0 != devData->timestamp)
    {
        if (readIndex < 0)
        {
            memcpy(buf, readPos, smInfo.pHead->blkSz);
        }
        else
        {
            uint64_t realtime;
            timeval ts;
            gettimeofday(&ts, NULL);
            realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
            if (realtime < (devData->timestamp + devData->timeout))
            {
                memcpy(buf, readPos, smInfo.pHead->blkSz);
            }
            else
            {
                doneIndex = -1;
            }
        }
        devData->timestamp = 0;
    }
    else
    {
        doneIndex = -1;
    }
    // 索引变化
    if (readIndex < 0)
    {
        smInfo.pHead->rdIdx = (smInfo.pHead->rdIdx + 1) % (smInfo.pHead->blkGt);
        if (0 == smInfo.pHead->rdIdx)
        {
            smInfo.pHead->ahead--;
        }
    }

    SemUnlock(smInfo.semMutex);
    return doneIndex;
}

int CBaseSMFIFO::SMWriteCfgData(const void *buf, int writeIndex)
{
    SemLock(smInfo.semMutex);
    int doneIndex = -1;
    void *writePos = NULL;
    // 寻址
    if (writeIndex < 0)
    {
        doneIndex = smInfo.pHead->wrIdx;
    }
    else if (writeIndex < smInfo.pHead->blkGt)
    {
        doneIndex = writeIndex;
    }
    else
    {
        doneIndex = -1;
        SemUnlock(smInfo.semMutex);
        return doneIndex;
    }
    writePos = smInfo.pPayload + doneIndex * smInfo.pHead->blkSz;
    NSBaseTool::SCfgData *cfgData = (NSBaseTool::SCfgData *)writePos;
    // 写操作
    if (0 == cfgData->timestamp)
    {
        memcpy(writePos, buf, smInfo.pHead->blkSz);
    }
    else
    {
        uint64_t realtime;
        timeval ts;
        gettimeofday(&ts, NULL);
        realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
        if ((cfgData->timestamp + cfgData->timeout) < realtime)
        {
            memcpy(writePos, buf, smInfo.pHead->blkSz);
        }
        else
        {
            doneIndex = -1;
        }
    }
    // 索引变化
    if (writeIndex < 0)
    {
        smInfo.pHead->wrIdx = (smInfo.pHead->wrIdx + 1) % (smInfo.pHead->blkGt);
        if (0 == smInfo.pHead->wrIdx)
        {
            smInfo.pHead->ahead++;
        }
    }

    SemUnlock(smInfo.semMutex);
    return doneIndex;
}

int CBaseSMFIFO::SMReadCfgData(void *buf, int readIndex)
{
    SemLock(smInfo.semMutex);
    int doneIndex = -1;
    void *readPos = NULL;
    // 顺序读时读索引不能超前于写索引
    if (readIndex < 0)
    {
        if ((smInfo.pHead->wrIdx + smInfo.pHead->ahead * smInfo.pHead->blkGt) <= smInfo.pHead->rdIdx)
        {
            doneIndex = -1;
            SemUnlock(smInfo.semMutex);
            return doneIndex;
        }
    }
    // 寻址
    if (readIndex < 0)
    {
        doneIndex = smInfo.pHead->rdIdx;
    }
    else if (readIndex < smInfo.pHead->blkGt)
    {
        doneIndex = readIndex;
    }
    else
    {
        doneIndex = -1;
        SemUnlock(smInfo.semMutex);
        return doneIndex;
    }
    readPos = smInfo.pPayload + doneIndex * smInfo.pHead->blkSz;
    NSBaseTool::SCfgData *cfgData = (NSBaseTool::SCfgData *)readPos;
    // 读操作
    if (0 != cfgData->timestamp)
    {
        if (readIndex < 0)
        {
            memcpy(buf, readPos, smInfo.pHead->blkSz);
        }
        else
        {
            uint64_t realtime;
            timeval ts;
            gettimeofday(&ts, NULL);
            realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
            if (realtime < (cfgData->timestamp + cfgData->timeout))
            {
                memcpy(buf, readPos, smInfo.pHead->blkSz);
            }
            else
            {
                doneIndex = -1;
            }
        }
        cfgData->timestamp = 0;
    }
    else
    {
        doneIndex = -1;
    }
    // 索引变化
    if (readIndex < 0)
    {
        smInfo.pHead->rdIdx = (smInfo.pHead->rdIdx + 1) % (smInfo.pHead->blkGt);
        if (0 == smInfo.pHead->rdIdx)
        {
            smInfo.pHead->ahead--;
        }
    }

    SemUnlock(smInfo.semMutex);
    return doneIndex;
}

int CBaseSMFIFO::SMWrite(const void *buf, int write_index)
{
    SemLock(smInfo.semMutex);
    int doneIndex = -1;
    void *writePos = NULL;
    // 寻址
    if (write_index < 0)
    {
        doneIndex = smInfo.pHead->wrIdx;
    }
    else if (write_index < smInfo.pHead->blkGt)
    {
        doneIndex = write_index;
    }
    else
    {
        doneIndex = -1;
        SemUnlock(smInfo.semMutex);
        return doneIndex;
    }
    writePos = smInfo.pPayload + doneIndex * smInfo.pHead->blkSz;
    // 写操作
    memcpy(writePos, buf, smInfo.pHead->blkSz);
    // 索引变化
    if (write_index < 0)
    {
        smInfo.pHead->wrIdx = (smInfo.pHead->wrIdx + 1) % (smInfo.pHead->blkGt);
        if (0 == smInfo.pHead->wrIdx)
        {
            smInfo.pHead->ahead++;
        }
    }
    SemUnlock(smInfo.semMutex);
    return doneIndex;
}

int CBaseSMFIFO::SMRead(void *buf, int readIndex)
{
    SemLock(smInfo.semMutex);
    int doneIndex = -1;
    void *readPos = NULL;
    // 顺序读时读索引不能超前于写索引
    if (readIndex < 0)
    {
        if ((smInfo.pHead->wrIdx + smInfo.pHead->ahead * smInfo.pHead->blkGt) <= smInfo.pHead->rdIdx)
        {
            doneIndex = -1;
            SemUnlock(smInfo.semMutex);
            return doneIndex;
        }
    }
    // 寻址
    if (readIndex < 0)
    {
        doneIndex = smInfo.pHead->rdIdx;
    }
    else if (readIndex < smInfo.pHead->blkGt)
    {
        doneIndex = readIndex;
    }
    else
    {
        doneIndex = -1;
        SemUnlock(smInfo.semMutex);
        return doneIndex;
    }
    readPos = smInfo.pPayload + doneIndex * smInfo.pHead->blkSz;
    // 读操作
    memcpy(buf, readPos, smInfo.pHead->blkSz);
    // 索引变化
    if (readIndex < 0)
    {
        smInfo.pHead->rdIdx = (smInfo.pHead->rdIdx + 1) % (smInfo.pHead->blkGt);
        if (0 == smInfo.pHead->rdIdx)
        {
            smInfo.pHead->ahead--;
        }
    }
    SemUnlock(smInfo.semMutex);
    return doneIndex;
}

void CBaseSMFIFO::SMDetach(bool destroyEnable)
{
    shmdt(smInfo.pHead);
    // 销毁
    if (destroyEnable)
    {
        shmctl(smInfo.smId, IPC_RMID, 0);
        semctl(smInfo.semMutex, 0, IPC_RMID);
    }
}

int CBaseSMFIFO::MQInit(int mqkey, bool createEnable)
{
    smInfo.mqId = msgget(mqkey, 0);

    if (-1 == smInfo.mqId)
    {
        if (false == createEnable)
        {
            perror("MQInit()");
            return -1;
        }
        smInfo.mqId = msgget(mqkey, IPC_CREAT | 0666);
        if (-1 == smInfo.mqId)
        {
            perror("MQInit()");
            return -1;
        }
    }

    return 0;
}

int CBaseSMFIFO::MQSend(const void *msg, size_t msgSz, bool wait)
{
    int ret;
    if (wait)
    {
        ret = msgsnd(smInfo.mqId, msg, msgSz, 0);
    }
    else
    {
        ret = msgsnd(smInfo.mqId, msg, msgSz, IPC_NOWAIT);
    }
    return ret;
}

int CBaseSMFIFO::MQRecv(void *msg, size_t msgSz, long msgType, bool wait)
{
    int ret;
    if (wait)
    {
        ret = msgrcv(smInfo.mqId, msg, msgSz, msgType, 0);
    }
    else
    {
        ret = msgrcv(smInfo.mqId, msg, msgSz, msgType, IPC_NOWAIT);
    }
    return ret;
}

void CBaseSMFIFO::MQDetach()
{
    msgctl(smInfo.mqId, IPC_RMID, NULL);
}