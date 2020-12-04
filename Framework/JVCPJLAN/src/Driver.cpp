#include "../include/Driver.h"

CDriver *CDriver::pDriverStatic = NULL;

CDriver::CDriver(int argDriIndex, int argDevIndex, int argDevNum,
                 int argMaxDriNum, int argMaxDevNum)
{
    // 驱动信息
    pDriverInfo = new DriversInfo;
    SHMFIFOBase shmDri;
    shmDri.shmfifo_init(SHRDRIVERS, argMaxDriNum, sizeof(DriversInfo), false);
    shmDri.shmfifo_simple_read(pDriverInfo, argDriIndex);
    shmDri.shmfifo_destroy(false);
    // 设备信息
    int bufNum = argDevNum;
    DevsInfo *pBuf = NULL;
    if (bufNum > 0)
    {
        pBuf = new DevsInfo[bufNum];
        SHMFIFOBase shmDev;
        shmDev.shmfifo_init(SHRDEVICE, argMaxDevNum, sizeof(DevsInfo), false);
        for (int bufIndex = 0; bufIndex < bufNum; bufIndex++)
            shmDev.shmfifo_simple_read(pBuf + bufIndex, (argDevIndex + bufIndex) % argMaxDevNum);
        shmDev.shmfifo_destroy(false);
    }
    int devNum = bufNum;
    for (int bufIndex = 0; bufIndex < bufNum; bufIndex++)
        if (0 == strcasecmp(pBuf[bufIndex].DevType, "649T1323456527") ||
            0 == strcasecmp(pBuf[bufIndex].DevType, "5601589977682"))
            devNum--;
    if (0 == devNum)
    {
        pDevicesInfo = new DevsInfo[1];
        pDevicesInfo->devsNum = 0;
    }
    else
    {
        pDevicesInfo = new DevsInfo[devNum];
        int writeIndex = 0;
        for (int bufIndex = 0; bufIndex < bufNum; bufIndex++)
            if (0 == strcasecmp(pBuf[bufIndex].DevType, "649T1323456527") ||
                0 == strcasecmp(pBuf[bufIndex].DevType, "5601589977682"))
                continue;
            else
            {
                memcpy(pDevicesInfo + writeIndex, pBuf + bufIndex, sizeof(DevsInfo));
                pDevicesInfo[writeIndex].devsNum = devNum;
                writeIndex++;
            }
    }
    if (pBuf)
    {
        delete[] pBuf;
        pBuf = NULL;
    }
    // 通信属性
    pCommParam = new SDriCommParam;
    StrNCpySafe(pCommParam->IP, pDriverInfo->Options.IP, INET_ADDRSTRLEN);
    StrNCpySafe(pCommParam->mac, "", STRSIZE_MAC);
    pCommParam->port = PORT_DEFAULT;
    pCommParam->timeout = 300;
    pCommParam->minActTime = 300;
    pCommParam->interval = 300;
    pCommParam->socketID = 0;
    // 设备状态
    if (0 == devNum)
    {
        pDevicesState = new SDevStateParam[1];
        pDevicesState->devNum = 0;
    }
    else
    {
        pDevicesState = new SDevStateParam[devNum];
        for (int devIndex = 0; devIndex < devNum; devIndex++)
        {
            pDevicesState[devIndex].devNum = devNum;
            pDevicesState[devIndex].isOnline = true;
            memset(pDevicesState[devIndex].strOnline, 0, STRSIZE_EPVALUE);
            pDevicesState[devIndex].isFault = false;
            memset(pDevicesState[devIndex].strFault, 0, STRSIZE_EPVALUE);
            for (int epIndex = 0; epIndex < MAX_EPID; epIndex++)
            {
                pDevicesState[devIndex].epInfo[epIndex].epID = epIndex + 1;
                pDevicesState[devIndex].epInfo[epIndex].epNum = MAX_EPID;
                memset(pDevicesState[devIndex].epInfo[epIndex].repLast, 0, STRSIZE_EPVALUE);
            }
        }
    }
    // 运行标志
    isRunning = true;
    // 发送连接标志
    sendDoneFlag = false;
    connectState = tcpDisconnected;
    sendRecvState = EXECFALSE;
    checkConnectFlag = recvCheck;
    standByTime = 0;
    repeatCheckTime = 0;
    connCheckCnt = 0;

    // 数据存储
    sendInfo = new uint8_t[32];
    // 共享内存初始化
    shmW.shmfifo_init(SHMREPORTKEY, MAXSHMSIZE, sizeof(DevsData), false);
    shmCfgW.shmfifo_init(SHMCFGREPKEY, MAXSHMCFGSIZE, sizeof(CfgDevsData), false);
    // 同步索引初始化                ???
    StateSyncIndex = SYNC_SYSSWITCH;
    syncState = SYNC_EP01;
    // 静态对象指针
    pDriverStatic = this;
    // 线程号初始化
    sendHandleThreadID = 0;
    recvHandleThreadID = 0;
    getOrderThreadID = 0;
    getConfigOrderThreadID = 0;
}

CDriver::~CDriver()
{
    if (0 != sendHandleThreadID)
        pthread_join(sendHandleThreadID, NULL);
    if (0 != recvHandleThreadID)
        pthread_join(recvHandleThreadID, NULL);
    if (0 != getOrderThreadID)
        pthread_join(getOrderThreadID, NULL);
    if (0 != getConfigOrderThreadID)
        pthread_join(getConfigOrderThreadID, NULL);

    delete pDriverInfo;
    pDriverInfo = NULL;
    delete[] pDevicesInfo;
    pDevicesInfo = NULL;
    delete pCommParam;
    pCommParam = NULL;
    delete[] pDevicesState;
    pDevicesState = NULL;
    delete[] sendInfo;
    sendInfo = NULL;
    shmW.shmfifo_destroy(false);
    shmCfgW.shmfifo_destroy(false);
}

int ChtoHex(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return ch - 48;
    else if ((ch >= 'a') && (ch <= 'f'))
        return ch - 87;
    else if ((ch >= 'A') && (ch <= 'F'))
        return ch - 55;
    else
        return -1;
}

uint8_t CDriver::CmpAsString(char *lastRep, char *buf)
{
    uint8_t ret;
    if (0 == strcasecmp(lastRep, buf))
        ret = CMP_UNDIFFER;
    else if (0 == strcasecmp(lastRep, ""))
        ret = CMP_INITSYNC;
    else
        ret = CMP_DIFFER;
    StrNCpySafe(lastRep, buf, STRSIZE_EPVALUE);

    return ret;
}

uint8_t CDriver::CmpAsDouble(char *lastRep, char *buf, double range)
{
    uint8_t ret;
    if (0 == strcasecmp(lastRep, ""))
    {
        ret = CMP_INITSYNC;
        StrNCpySafe(lastRep, buf, STRSIZE_EPVALUE);
    }
    else if (fabs(atof(lastRep) - atof(buf)) < range)
        ret = CMP_UNDIFFER;
    else
    {
        ret = CMP_DIFFER;
        StrNCpySafe(lastRep, buf, STRSIZE_EPVALUE);
    }

    return ret;
}

uint8_t CDriver::DataCmp(char *lastRep, char *buf, double range)
{
    if (0 == range)
        return CmpAsString(lastRep, buf);
    else
        return CmpAsDouble(lastRep, buf, range);
}

int CDriver::GetEPIndex(int devIndex, int epID)
{
    if ((1 <= epID) && (epID <= MAX_EPID))
        return (epID - 1);

    return -1;
}

int CDriver::GetDevIndex(char *devID)
{
    for (int devIndex = 0; devIndex < pDevicesInfo->devsNum; devIndex++)
        if (SpellIsSame(devID, pDevicesInfo[devIndex].DevId))
            return devIndex;

    return -1;
}

bool CDriver::GetOrderStart()
{
    int ret = pthread_create(&getOrderThreadID, NULL, GetOrder, (void *)this);
    if (ret != 0)
    {
        perror(DRINAME "::GetOrderStart()");
        return false;
    }

    return true;
}

int CDriver::ReqConv(int devIndex, int epID, char *epValue, SDevOrder *reqOrder)
{
    string strHex;
    strHex = "218901";
    switch (epID)
    {
    // 开关机
    case 1:
    {
        if (SpellIsSame(epValue, "true"))
            strHex += "505731";
        else if (SpellIsSame(epValue, "false"))
            strHex += "505730";
        else
            return -1;
    }
    break;
    // 输入源选择
    case 3:
    {
        strHex += "4950";
        switch (atoi(epValue))
        {
        case 1:
            strHex += "36";
            break;
        case 2:
            strHex += "37";
            break;
        default:
            return -1;
            break;
        }
    }
    break;
    // 按键
    case 17:
    {
        strHex += "52433733";
        switch (atoi(epValue))
        {
        // MENU
        case 1:
            strHex += "3245";
            break;
        // OK
        case 2:
            strHex += "3246";
            break;
        // BACK
        case 3:
            strHex += "3033";
            break;
        // 上
        case 4:
            strHex += "3031";
            break;
        // 下
        case 5:
            strHex += "3032";
            break;
        // 右
        case 6:
            strHex += "3336";
            break;
        // 左
        case 7:
            strHex += "3334";
            break;
        // GAMMA
        case 8:
            strHex += "4535";
            break;
        // HIDE
        case 9:
            strHex += "3144";
            break;
        // INFO
        case 10:
            strHex += "3734";
            break;
        // LENS
        case 11:
            strHex += "3330";
            break;
        // PICTURE
        case 12:
            strHex += "4534";
            break;
        // COLOR
        case 13:
            strHex += "3838";
            break;
        // CMD
        case 14:
            strHex += "3841";
            break;
        // MPC
        case 15:
            strHex += "4530";
            break;
        // SETTING MEMORY
        case 16:
            strHex += "4334";
            break;
        // ADVANCED MENU
        case 17:
            strHex += "3733";
            break;
        default:
            return -1;
            break;
        }
    }
    break;//                                   0        1                           0
    default:
        return -1;
        break;
    }

    reqOrder->dataLen = StrToHex(reqOrder->data, BYTESIZE_TRANSFERDATA, strHex.c_str());

    return reqOrder->dataLen;//                                   0        1                           0
}

int CDriver::BuildReqOrder(DevsData *devData)
{
    SDevOrder reqOrder = {0};
    int devIndex = GetDevIndex(devData->DevId);
    if (-1 == devIndex)
    {
        printf(DRINAME "::BuildReqOrder(): Invalid device index.\n");
        return -1;
    }
    int epID = atoi(devData->epData.EpId);
    if (-1 == GetEPIndex(devIndex, epID))
    {
        printf(DRINAME "::BuildReqOrder(): Invalid ep index.\n");
        return -1;
    }
    int dataLen = ReqConv(devIndex, epID, devData->epData.EpValue, &reqOrder);
    if (0 == dataLen)
    {
        printf(DRINAME "::BuildReqOrder(): Invalid data.\n");
        return -1;
    }

    reqOrder.devIndex = devIndex;
    reqOrder.devAct = 0x00;
    StrNCpySafe(reqOrder.orderID, devData->uuid, STRSIZE_ORDERID);
    if (devData->Repeat <= 1)
    {
        reqOrder.repeat = 3;
    }
    else
    {
        reqOrder.repeat = devData->Repeat;
    }

    reqOrder.timeStamp = devData->data_timestamp;
    reqOrder.timeout = devData->Timeout;
    reqOrder.state = WAIT;
    sendList.WriteToQueue(&reqOrder);

    return dataLen;
}

void *CDriver::GetOrder(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;

    SHMFIFOBase shmDevDataR;
    int msgType = pDriver->pDriverInfo->Id;
    mymsg *msgR = new mymsg;
    int readIndex = MAXSHMSIZE;
    DevsData *devDataR = new DevsData;

    shmDevDataR.msgque_init(SHMREQUESTKEY, false);
    shmDevDataR.shmfifo_init(SHMREQUESTKEY, MAXSHMSIZE, sizeof(DevsData), false);
    while (pDriver->isRunning)
    {
        usleep(INTERVAL_GETORDER * 1000);
        // 读消息队列
        memset(msgR, 0, sizeof(mymsg));
        if (-1 == shmDevDataR.msgque_recv(msgR, MSG_SZ, msgType, false))
        {
            if (ENOMSG != errno)
                perror(DRINAME "::GetOrder()::msgque_recv()");
            continue;
        }
        readIndex = atoi(msgR->mtext);
        // 读共享内存
        memset(devDataR, 0, sizeof(DevsData));
        if (-1 == shmDevDataR.shmfifo_read(devDataR, readIndex))
        {
            printf(DRINAME "::GetOrder(): Share memory read error.\n");
            continue;
        }
        if (-1 == pDriver->BuildReqOrder(devDataR))
        {
            printf(DRINAME "::GetOrder(): Build request order error.\n");
            continue;
        }
    }
    delete msgR;
    msgR = NULL;
    delete devDataR;
    devDataR = NULL;

    shmDevDataR.shmfifo_destroy(false);

    return (void *)0;
}

bool CDriver::HandleStart()
{
    int ret;
    ret = pthread_create(&recvHandleThreadID, NULL, RecvHandle, (void *)this);
    if (ret != 0)
        perror(DRINAME "::HandleStart()::RecvHandle()");

    ret = pthread_create(&sendHandleThreadID, NULL, SendHandle, (void *)this);
    if (ret != 0)
        perror(DRINAME "::HandleStart()::SendHandle()");

    return true;
}

void CDriver::BuildBaseRepData(DevsData *devData, int devIndex)
{
    StrNCpySafe(devData->DevId, pDevicesInfo[devIndex].DevId, sizeof(devData->DevId));
    StrNCpySafe(devData->cDevId, pDevicesInfo[devIndex].cDevId, sizeof(devData->cDevId));
    StrNCpySafe(devData->devType, pDevicesInfo[devIndex].DevType, sizeof(devData->devType));
    devData->epNum = 1;
    devData->data_timestamp = GetTimeStamp();
}

void CDriver::BuildWholeRepData(DevsData *devData, int devIndex, int epID, char *epValue, double range)
{
    if (range >= 0)
    {
        uint8_t ret = DataCmp(pDevicesState[devIndex].epInfo[epID - 1].repLast, epValue, range);
        if (CMP_UNDIFFER == ret)
            return;
        if (CMP_INITSYNC == ret)
            StrNCpySafe(devData->dataType, REP_SYNC, sizeof(devData->dataType));
        else if (CMP_DIFFER == ret)
            StrNCpySafe(devData->dataType, REP_CHANGE, sizeof(devData->dataType));
        else
            return;
    }
    else
        StrNCpySafe(devData->dataType, REP_CHANGE, sizeof(devData->dataType));

    sprintf(devData->epData.EpId, "%d", epID);
    StrNCpySafe(devData->epData.EpValue, epValue, EPVALUESIZE);
    BuildBaseRepData(devData, devIndex);
}

void CDriver::WriteRepData(int devIndex, int epID, char *epValue, double range)
{
    DevsData devData = {0};
    BuildWholeRepData(&devData, devIndex, epID, epValue, range);
    if (1 == devData.epNum)
        shmW.shmfifo_write(&devData, -1);
}

bool CDriver::AckCmp(uint8_t *sendInfo, uint8_t *recvInfo, size_t recvInfoLen)
{
    for (size_t i = 1; i < recvInfoLen - 1; i++)
    {
        if (sendInfo[i] != recvInfo[i])
        {
            memset(sendInfo, 0, sizeof(uint8_t) * 32);
            return false;
        }
    }

    return true;
}

int CDriver::RecvParse(SRecvBuffer *recvBuffer)
{
    uint8_t *strState = new uint8_t[recvBuffer->dataLen];

    if (strState == NULL)
    {
        printf(DRINAME "::strState is NULL!\n");
        return 0;
    }

    if (recvBuffer->dataLen <= 0)
        return 0;

    memcpy(strState, recvBuffer->data, recvBuffer->dataLen);
    char epValue[STRSIZE_EPVALUE] = {0};

    if (strState[0] == 0x40) // response
    {
        // power
        if (strState[3] == 0x50)
        {
            if (strState[5] == 0x31)
                StrNCpySafe(epValue, "true", STRSIZE_EPVALUE);
            else
                StrNCpySafe(epValue, "false", STRSIZE_EPVALUE);
            WriteRepData(0, 1, epValue, 0);
        }
        // input
        if (strState[3] == 0x49)
        {
            if (strState[5] == 0x36)
                StrNCpySafe(epValue, "1", STRSIZE_EPVALUE);
            else
                StrNCpySafe(epValue, "2", STRSIZE_EPVALUE);
            WriteRepData(0, 3, epValue, 0);
        }
    }
    else if (strState[0] == 0x06) // ACK
    {
        if (AckCmp(sendInfo, recvBuffer->data, recvBuffer->dataLen))
        {
            memset(sendInfo, 0, sizeof(uint8_t) * 32);
            sendRecvState = EXECSUCCESS;
        }

        if (strState[3] == 0x00 && strState[4] == 0x00) // 待机查询接收数据解析
        {
            sendRecvState = EXECSUCCESS;
            connCheckCnt = 0;
        }
    }
    else if (SpellIsSame((char *)recvBuffer->data, "PJ_OK") &&
             waitPJ_OK == connectState) // PJ_OK
        connectState = recvPJ_OK;
    else if (SpellIsSame((char *)recvBuffer->data, "PJACK") &&
             sentPJ_REQ == connectState) // PJACK
    {
        connectState = recvPJ_ACK;
        FlashDevState(0, STATE_NORMAL);
        WriteDevState(0);
    }

    else if (SpellIsSame((char *)recvBuffer->data, "PJNAK") || SpellIsSame((char *)recvBuffer->data, "PJ_NG"))
        connectState = tcpDisconnected;

    delete[] strState;
    return 0;
}

int CDriver::RecvCallback(TransferData *data, int type)
{
    switch (type)
    {
    case NOTIFY:
    {
        SRecvBuffer recvBuffer = {0};
        if (data->param3 > (SIZE_RECVDATA - 1))
            recvBuffer.dataLen = SIZE_RECVDATA - 1;
        else
            recvBuffer.dataLen = data->param3;
        memcpy(recvBuffer.data, data->Data, recvBuffer.dataLen);
        pDriverStatic->recvList.WriteToQueue(&recvBuffer);
    }
    break;
    case NETWORKSTATUS:
    {
    }
    break;
    default:
        break;
    }

    return 0;
}

void CDriver::WriteDevState(int devIndex)
{
    DevsData devData;
    memset(&devData, 0, sizeof(DevsData));
    char strState[STRSIZE_EPVALUE] = {0};

    StrNCpySafe(devData.DevId, pDevicesInfo[devIndex].DevId, sizeof(devData.DevId));
    StrNCpySafe(devData.cDevId, pDevicesInfo[devIndex].cDevId, sizeof(devData.cDevId));
    StrNCpySafe(devData.devType, pDevicesInfo[devIndex].DevType, sizeof(devData.devType));
    StrNCpySafe(devData.dataType, REP_STATUS, sizeof(devData.dataType));
    devData.epNum = 1;
    StrNCpySafe(devData.epData.EpId, REP_STATUS, sizeof(devData.epData.EpId));
    // 网络状态
    if (pDevicesState[devIndex].isOnline)
        StrNCpySafe(strState, DEVICE_ONLINE, STRSIZE_EPVALUE);
    else
        StrNCpySafe(strState, DEVICE_OFFLINE, STRSIZE_EPVALUE);
    if (CmpAsString(pDevicesState[devIndex].strOnline, strState))
    {
        StrNCpySafe(devData.epData.EpValue, strState, EPVALUESIZE);
        devData.data_timestamp = GetTimeStamp();
        shmW.shmfifo_write(&devData, -1);
    }
    // 异常状态
    if (pDevicesState[devIndex].isFault)
        StrNCpySafe(strState, DEVICE_FAULT, STRSIZE_EPVALUE);
    else
        StrNCpySafe(strState, DEVICE_USUAL, STRSIZE_EPVALUE);
    if (CmpAsString(pDevicesState[devIndex].strFault, strState))
    {
        StrNCpySafe(devData.epData.EpValue, strState, EPVALUESIZE);
        devData.data_timestamp = GetTimeStamp();
        shmW.shmfifo_write(&devData, -1);
    }
}

int CDriver::RepDevState()
{
    uint64_t waitStartTime = 0;
    // IP为空 循环等待
    while (SpellIsSame(pCommParam->IP, ""))
    {
        sleep(1);
    }
    // 上报设备状态
    if (false == tcp.isTCPConnected() ||
        noRecvedPJ_OK == connectState ||
        sentPJ_REQ == connectState ||
        tcpDisconnected == connectState)
    {
        connectState = tcpDisconnected;
        if (0 != pCommParam->socketID)
        {
            FlashDevState(0, STATE_OFFLINE);
            WriteDevState(0);
        }
        tcp.Disconnect();
        pCommParam->socketID = tcp.ConnectToServer(pCommParam->IP, pCommParam->port, RecvCallback);
        if (pCommParam->socketID < 3)
        {
            FlashDevState(0, STATE_OFFLINE);
            WriteDevState(0);
            sleep(4);
            return 0;
        }
        else
        {
            char adapterName[32] = {0};
            if (0 == GetAdapterName(pCommParam->IP, adapterName))
                GetPeerMAC(pCommParam->socketID, pCommParam->mac, adapterName);
            connectState = tcpConnected;
            waitStartTime = GetTimeStamp();
            connectState = waitPJ_OK;
        }
    }
    // 等待PJ_OK
    while (waitPJ_OK == connectState)
    {
        if (GetTimeStamp() > (waitStartTime + (5 * 1000))) // 5s超时等待
        {
            connectState = noRecvedPJ_OK;
            break;
        }
        usleep(1000);
    }
    // 等待recvPJ_Ack
    if (recvPJ_OK == connectState)
    {
        if (SendPJ_REQ())
        {
            waitStartTime = GetTimeStamp();
            while (connectState == sentPJ_REQ)
            {
                if (GetTimeStamp() > (waitStartTime + (5 * 1000)))
                    break;

                usleep(1000);
            }
        }
    }

    return 0;
}

int CDriver::SyncConv(SDevOrder &syncOrder)
{
    string strHex;

    strHex += "3F8901";

    switch (syncState)
    {
    case SYNC_EP01:
    {
        strHex += "5057";
        syncState = SYNC_EP03;
        break;
    }
    case SYNC_EP03:
    {
        strHex += "4950";
        syncState = SYNC_EP01;
        break;
    }
    default:
        syncState = SYNC_EP03;
        strHex += "5057";
        break;
    }
    syncOrder.devAct = 0x00;
    syncOrder.dataLen = StrToHex(syncOrder.data, BYTESIZE_TRANSFERDATA, strHex.c_str());
}

int CDriver::BuildSyncOrder()
{
    if (0 == pDevicesState->devNum)
    {
        usleep(pCommParam->interval * 1000);
        return 0;
    }
    SDevOrder syncOrder = {0};
    if (-1 == SyncConv(syncOrder))
        return -1;

    syncOrder.devIndex = StateSyncIndex;
    syncOrder.state = WAIT;
    ExecuteOrder(&syncOrder);

    return 0;
}

uint16_t CRC16(uint8_t *buf, size_t bufLen)
{
    uint16_t crc16 = 0xFFFF;
    int moveCount = 0;

    for (int i = 0; i < bufLen; i++)
    {
        crc16 = (((crc16 & 0x00FF) ^ buf[i]) & 0x00FF) + (crc16 & 0xFF00);
        for (moveCount = 0; moveCount < 8; moveCount++)
            if (crc16 & 0x0001)
            {
                crc16 = crc16 >> 1;
                crc16 = crc16 ^ 0xA001;
            }
            else
                crc16 = crc16 >> 1;
    }

    return crc16;
}

int CDriver::ExecuteOrder(SDevOrder *order)
{
    uint8_t sendBuf[SIZE_SENDDATA] = {0};
    size_t sendLen = 0;

    memcpy(sendBuf + sendLen, order->data, order->dataLen);
    sendLen += order->dataLen;
    sendBuf[sendLen++] = 0x0A;

    tcp.Send(sendBuf, sendLen);
    usleep(pCommParam->interval * 1000);

    // 超时等待
    uint64_t ackStartTime = 0;
    ackStartTime = GetTimeStamp();
    while (true)
    {
        if (EXECSUCCESS == sendRecvState)
        {
            order->state = DONE;
            sendRecvState = EXECFALSE;
            break;
        }
        if (GetTimeStamp() > ackStartTime + 1000)
            break;
        usleep(1000);
    }

    return 0;
}

int CDriver::WriteOrderRsp(SDevOrder *order)
{
    int devIndex = order->devIndex;
    DevsData devData = {0};
    StrNCpySafe(devData.DevId, pDevicesInfo[devIndex].DevId, sizeof(devData.DevId));
    StrNCpySafe(devData.cDevId, pDevicesInfo[devIndex].cDevId, sizeof(devData.cDevId));
    StrNCpySafe(devData.devType, pDevicesInfo[devIndex].DevType, sizeof(devData.devType));
    StrNCpySafe(devData.dataType, REP_REQ, sizeof(devData.dataType));
    StrNCpySafe(devData.uuid, order->orderID, sizeof(devData.uuid));
    devData.epNum = 0;
    devData.data_timestamp = GetTimeStamp();
    int ret = shmW.shmfifo_write(&devData, -1);
    if (-1 == ret)
    {
        printf(DRINAME "::WriteOrderRsp(): Share memory write error.\n");
        return -1;
    }

    return 0;
}

int CDriver::FlashDevState(int devIndex, int state)
{
    switch (state)
    {
    case STATE_OFFLINE:
    {
        pDevicesState[devIndex].isOnline = false;
        pDevicesState[devIndex].isFault = false;
    }
    break;
    case STATE_ERROR:
    {
        pDevicesState[devIndex].isOnline = true;
        pDevicesState[devIndex].isFault = true;
    }
    break;
    case STATE_NORMAL:
    {
        pDevicesState[devIndex].isOnline = true;
        pDevicesState[devIndex].isFault = false;
    }
    break;
    default:
        break;
    }

    return 0;
}

bool CDriver::SendPJ_REQ()
{
    char req[10] = "PJREQ";
    connectState = sentPJ_REQ;
    if (tcp.Send((uint8_t *)req, strlen(req)))
        return true;

    return false;
}

void *CDriver::SendHandle(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;
    SDevOrder *reqOrder = new SDevOrder;

    pDriver->RepDevState();

    while (pDriver->isRunning)
    {
        if (recvPJ_ACK == pDriver->connectState)
        {
            memset(reqOrder, 0, sizeof(SDevOrder));
            // 读取命令队列
            if (pDriver->sendList.ReadFromQueue(reqOrder))
            {
                // 全局变量获取发送信息
                memcpy(pDriver->sendInfo, reqOrder->data, sizeof(reqOrder->data));
                if (0x00 == reqOrder->devAct)
                {
                    int repeatTime = -1;
                    while (true)
                    {
                        pDriver->ExecuteOrder(reqOrder);
                        repeatTime++;
                        // 命令执行响应
                        if (0 == repeatTime &&
                            -1 == pDriver->WriteOrderRsp(reqOrder))
                            printf(DRINAME "::BuildOrderRsp(): Report order response error.\n");
                        if (DONE == reqOrder->state ||
                            repeatTime >= reqOrder->repeat ||
                            GetTimeStamp() > (reqOrder->timeStamp + reqOrder->timeout))
                            break;
                        else
                            continue;
                    }
                }
            }
            else
                pDriver->BuildSyncOrder();

            if (SpellIsSame(pDriver->pDevicesState[0].epInfo[0].repLast, "false")) // 待机状态
            {
                // if (pDriver->standByTimeOver)
                // {
                //     pDriver->standByTime = GetTimeStamp();
                //     pDriver->standByTimeOver = false;
                // }
                if (GetTimeStamp() > pDriver->standByTime + 3000)
                {
                    pDriver->connCheckCnt++;
                    string strHex = "21890100";
                    reqOrder->dataLen = StrToHex(reqOrder->data, BYTESIZE_TRANSFERDATA, strHex.c_str());
                    reqOrder->state = WAIT;
                    reqOrder->repeat = 5;
                    reqOrder->timeout = 3 * 1000;
                    reqOrder->timeStamp = GetTimeStamp();
                    reqOrder->devAct = 0x00;
                    pDriver->ExecuteOrder(reqOrder);
                    // pDriver->standByTimeOver = true;
                    pDriver->standByTime = GetTimeStamp();
                }
            }
            else
            {
                pDriver->connCheckCnt = 0;
            }
        }
        pDriver->RepDevState();

        if (pDriver->connCheckCnt > 5)
        {
            pDriver->connCheckCnt = 0;
            pDriver->connectState = tcpDisconnected;
            pDriver->FlashDevState(0, STATE_OFFLINE);
            pDriver->WriteDevState(0);
        }

        usleep(1000);
    }

    delete reqOrder;
    reqOrder = NULL;

    return (void *)0;
}

void *CDriver::RecvHandle(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;
    SRecvBuffer *recvBuffer = new SRecvBuffer;

    while (pDriver->isRunning)
    {
        memset(recvBuffer, 0, sizeof(SRecvBuffer));
        // 读取接收队列
        if (pDriver->recvList.ReadFromQueue(recvBuffer))
        {
            pDriver->RecvParse(recvBuffer);
        }
        usleep(INTERVAL_VOID * 1000);
    }

    delete recvBuffer;
    recvBuffer = NULL;

    return (void *)0;
}

int CDriver::HandleConfigOrder(CfgDevsData *configData)
{
    if (SpellIsSame(configData->dataType, CFG_manualSearch))
    {
        uint64_t beginTS = GetTimeStamp();
        // 获取网关IP
        for (int paramIndex = 0; paramIndex < configData->paramNum; paramIndex++)
            if (SpellIsSame(configData->paramData[paramIndex].Id, "ip"))
            {
                StrNCpySafe(pCommParam->IP, configData->paramData[paramIndex].Value, INET_ADDRSTRLEN);
                break;
            }
        // 获取网关MAC
        CfgDevsData ctrlData = {0};
        while (GetTimeStamp() < (beginTS + DURATION_MANUAL_SEARCH))
        {
            sleep(1);
            if (false == SpellIsSame(pCommParam->mac, ""))
            {
                // 上报controller
                StrNCpySafe(ctrlData.uuid, configData->uuid, sizeof(ctrlData.uuid));
                StrNCpySafe(ctrlData.devType, DEVTYPE_JVCPJLAN, sizeof(ctrlData.devType));
                StrNCpySafe(ctrlData.dataType, CFG_controllerRep, sizeof(ctrlData.dataType));
                StrNCpySafe(ctrlData.DriverName, configData->DriverName, sizeof(ctrlData.DriverName));
                ctrlData.lastLevel = 0;
                ctrlData.total = 1;
                ctrlData.index = 1;
                ctrlData.Timeout = 5000;
                ctrlData.data_timestamp = GetTimeStamp();
                ctrlData.paramNum = 2;
                StrNCpySafe(ctrlData.paramData[0].Id, "ip", sizeof(ctrlData.paramData[0].Id));
                StrNCpySafe(ctrlData.paramData[0].Value, pCommParam->IP, sizeof(ctrlData.paramData[0].Value));
                StrNCpySafe(ctrlData.paramData[1].Id, "mac", sizeof(ctrlData.paramData[1].Id));
                StrNCpySafe(ctrlData.paramData[1].Value, pCommParam->mac, sizeof(ctrlData.paramData[1].Value));
                shmCfgW.shmfifo_cfg_write(&ctrlData, -1);
                //  上报网关设备
                CfgDevsData repDevData = {0};
                StrNCpySafe(repDevData.uuid, configData->uuid, sizeof(repDevData.uuid));
                StrNCpySafe(repDevData.pId, configData->DevId, sizeof(repDevData.pId));
                StrNCpySafe(repDevData.pType, configData->devType, sizeof(repDevData.pType));
                StrNCpySafe(repDevData.devType, DEVTYPE_JVCPJLAN, sizeof(repDevData.devType));
                StrNCpySafe(repDevData.dataType, CFG_searchRep, sizeof(repDevData.dataType));
                StrNCpySafe(repDevData.DriverName, pCommParam->mac, sizeof(repDevData.DriverName));
                repDevData.lastLevel = 1;
                repDevData.total = 1;
                repDevData.index = 1;
                repDevData.Timeout = 5000;
                repDevData.data_timestamp = GetTimeStamp();
                repDevData.paramNum = 3;
                StrNCpySafe(repDevData.paramData[0].Id, "mac", sizeof(repDevData.paramData[0].Id));
                StrNCpySafe(repDevData.paramData[0].Value, pCommParam->mac, sizeof(repDevData.paramData[0].Value));
                StrNCpySafe(repDevData.paramData[1].Id, "oldCtrl", sizeof(repDevData.paramData[1].Id));
                StrNCpySafe(repDevData.paramData[1].Value, configData->DriverName, sizeof(repDevData.paramData[1].Value));
                StrNCpySafe(repDevData.paramData[2].Id, "showId", sizeof(repDevData.paramData[2].Id));
                StrNCpySafe(repDevData.paramData[2].Value, pCommParam->IP, sizeof(repDevData.paramData[2].Value));
                shmCfgW.shmfifo_cfg_write(&repDevData, -1);

                return 0;
            }
        }
        // 未搜索到controller
        StrNCpySafe(ctrlData.uuid, configData->uuid, sizeof(ctrlData.uuid));
        StrNCpySafe(ctrlData.dataType, CFG_controllerRep, sizeof(ctrlData.dataType));
        StrNCpySafe(ctrlData.DriverName, configData->DriverName, sizeof(ctrlData.DriverName));
        ctrlData.lastLevel = 1;
        ctrlData.total = 1;
        ctrlData.index = 1;
        ctrlData.Timeout = 5000;
        ctrlData.data_timestamp = GetTimeStamp();
        ctrlData.paramNum = 2;
        StrNCpySafe(ctrlData.paramData[0].Id, "ip", sizeof(ctrlData.paramData[0].Id));
        StrNCpySafe(ctrlData.paramData[0].Value, pCommParam->IP, sizeof(ctrlData.paramData[0].Value));
        StrNCpySafe(ctrlData.paramData[1].Id, "optType", sizeof(ctrlData.paramData[1].Id));
        StrNCpySafe(ctrlData.paramData[1].Value, "1", sizeof(ctrlData.paramData[1].Value));
        shmCfgW.shmfifo_cfg_write(&ctrlData, -1);
    }

    return 0;
}

void *CDriver::GetConfigOrder(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;

    SHMFIFOBase shmCfgR;
    int msgType = pDriver->pDriverInfo->Id;
    mymsg *msgR = new mymsg;
    int readIndex = MAXSHMCFGSIZE;
    CfgDevsData *cfgDataR = new CfgDevsData;

    shmCfgR.msgque_init(SHMCFGREQKEY, false);
    shmCfgR.shmfifo_init(SHMCFGREQKEY, MAXSHMCFGSIZE, sizeof(CfgDevsData), false);
    while (pDriver->isRunning)
    {
        usleep(INTERVAL_GETORDER * 1000);
        // 读消息队列
        memset(msgR, 0, sizeof(mymsg));
        if (-1 == shmCfgR.msgque_recv(msgR, MSG_SZ, msgType, false))
        {
            if (ENOMSG != errno)
                perror(DRINAME "::GetConfigOrder()::msgque_recv()");
            continue;
        }
        readIndex = atoi(msgR->mtext);
        // 读共享内存
        memset(cfgDataR, 0, sizeof(CfgDevsData));
        if (-1 == shmCfgR.shmfifo_cfg_read(cfgDataR, readIndex))
        {
            printf(DRINAME "::GetConfigOrder(): Share memory read error.\n");
            continue;
        }
        pDriver->HandleConfigOrder(cfgDataR);
    }
    delete msgR;
    msgR = NULL;
    delete cfgDataR;
    cfgDataR = NULL;

    shmCfgR.shmfifo_destroy(false);

    return (void *)0;
}

bool CDriver::GetConfigOrderStart()
{
    int ret = pthread_create(&getConfigOrderThreadID, NULL, GetConfigOrder, (void *)this);
    if (ret != 0)
    {
        perror(DRINAME "::GetConfigOrderStart()");
        return false;
    }

    return true;
}

int CDriver::GetPeerMAC(int socket, char *macBuf, char *adapterName)
{
    int ret = -1;
    arpreq arPreq;
    sockaddr_in dstadd_in;
    socklen_t len = sizeof(sockaddr_in);

    memset(&arPreq, 0, sizeof(arpreq));
    memset(&dstadd_in, 0, len);
    if (getpeername(socket, (sockaddr *)&dstadd_in, &len) < 0)
        printf("ONKYOAVAMP::GetPeerMAC()::getpeername() error.\n");
    else
    {
        memcpy(&arPreq.arp_pa, &dstadd_in, len);
        strcpy(arPreq.arp_dev, adapterName);
        arPreq.arp_pa.sa_family = AF_INET;
        arPreq.arp_ha.sa_family = AF_UNSPEC;
        if (ioctl(socket, SIOCGARP, &arPreq) < 0)
            printf("ONKYOAVAMP::GetPeerMAC()::ioctl() SIOCGARP error.\n");
        else
        {
            uint8_t *ptr = (uint8_t *)arPreq.arp_ha.sa_data;
            ret = sprintf(macBuf,
                          "%02x%02x%02x%02x%02x%02x",
                          ptr[0],
                          ptr[1],
                          ptr[2],
                          ptr[3],
                          ptr[4],
                          ptr[5]);
        }
    }

    return ret;
}
