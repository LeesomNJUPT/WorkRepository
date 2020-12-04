#include "../include/Driver.h"

CDriver *CDriver::pDriverStatic = NULL;

CDriver::CDriver(int argDriIndex, int argDevIndex, int argDevNum,
                 int argMaxDriNum, int argMaxDevNum)
{
    // 静态对象指针
    pDriverStatic = this;

    // 驱动信息
    pDriverInfo = new DriversInfo;
    SHMFIFOBase shmDriR;
    shmDriR.shmfifo_init(SHRDRIVERS, argMaxDriNum, sizeof(DriversInfo), false);
    shmDriR.shmfifo_simple_read(pDriverInfo, argDriIndex);
    shmDriR.shmfifo_destroy(false);

    // 设备信息
    int rawDevNum = argDevNum;
    if (rawDevNum <= 0)
        rawDevNum = 0;
    int devNum = rawDevNum;
    DevsInfo *rawDev = NULL;
    if (rawDevNum > 0)
    {
        rawDev = new DevsInfo[rawDevNum];
        SHMFIFOBase shmDevR;
        shmDevR.shmfifo_init(SHRDEVICE, argMaxDevNum, sizeof(DevsInfo), false);
        for (int rawIndex = 0; rawIndex < rawDevNum; rawIndex++)
        {
            shmDevR.shmfifo_simple_read(rawDev + rawIndex, (argDevIndex + rawIndex) % argMaxDevNum);
            if (SpellIsSame(rawDev[rawIndex].DevType, DEVTYPE_RS485) || SpellIsSame(rawDev[rawIndex].DevType, DEVTYPE_RS232))
                devNum--;
        }
        shmDevR.shmfifo_destroy(false);
    }

    if (0 == devNum)
    {
        pDevicesInfo = new DevsInfo[1];
        pDevicesInfo->devsNum = 0;
    }
    else
    {
        pDevicesInfo = new DevsInfo[devNum];
        int writeIndex = 0;
        for (int rawIndex = 0; rawIndex < rawDevNum; rawIndex++)
            if (SpellIsSame(rawDev[rawIndex].DevType, DEVTYPE_RS485) || SpellIsSame(rawDev[rawIndex].DevType, DEVTYPE_RS232))
                continue;
            else
            {
                memcpy(pDevicesInfo + writeIndex, rawDev + rawIndex, sizeof(DevsInfo));
                pDevicesInfo[writeIndex].devsNum = devNum;
                writeIndex++;
            }
    }
    if (rawDev)
    {
        delete[] rawDev;
        rawDev = NULL;
    }
    // 通信属性
    pCommParam = new SDriCommParam;
    StrNCpySafe(pCommParam->ip, pDriverInfo->Options.IP, INET_ADDRSTRLEN);
    pCommParam->port = PORT_DEFAULT;
    memset(pCommParam->mac, 0x00, STRSIZE_MAC);
    pCommParam->connectedTs = 0;
    memset(pCommParam->connectStatus, 0x00, STRSIZE_STATUS);
    // 设备状态
    if (0 == devNum)
    {
        pDevicesStatus = new SDevStatusParam[1];
        pDevicesStatus->devNum = 0;
    }
    else
    {
        pDevicesStatus = new SDevStatusParam[devNum];
        for (int devIndex = 0; devIndex < devNum; devIndex++)
        {
            pDevicesStatus[devIndex].devNum = devNum;
            pDevicesStatus[devIndex].devType = DevTypeConv(pDevicesInfo[devIndex].DevType);
            pDevicesStatus[devIndex].isOnline = true;
            memset(pDevicesStatus[devIndex].strOnline, 0x00, STRSIZE_STATUS);
            pDevicesStatus[devIndex].isFault = false;
            memset(pDevicesStatus[devIndex].strFault, 0x00, STRSIZE_STATUS);
            DevEpInfoInit(devIndex);
            pDevicesStatus[devIndex].syncToc = 0;
        }
    }
    // 同步状态
    switch (pDevicesStatus[0].devType)
    {
    case T748:
        syncState = SYNC_EP01;
        break;
    default:
        syncState = SYNC_EP01;
        break;
    }
    // 共享内存初始化
    shmW.shmfifo_init(SHMREPORTKEY, MAXSHMSIZE, sizeof(DevsData), false);
    shmCfgW.shmfifo_init(SHMCFGREPKEY, MAXSHMCFGSIZE, sizeof(CfgDevsData), false);
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
    for (int devIndex = 0; devIndex < pDevicesStatus->devNum; devIndex++)
    {
        delete[] pDevicesStatus[devIndex].epInfo;
        pDevicesStatus[devIndex].epInfo = NULL;
    }
    delete[] pDevicesStatus;
    pDevicesStatus = NULL;
    shmW.shmfifo_destroy(false);
    shmCfgW.shmfifo_destroy(false);
}

EDevType CDriver::DevTypeConv(const char *devType)
{
    string devTypeBuf = devType;
    EDevType ret = UNKNOW;

    if (DEVTYPE_T748 == devTypeBuf)
        ret = T748;
    else
        ret = UNKNOW;

    return ret;
}

int CDriver::DevEpInfoInit(int devIndex)
{
    uint8_t epNum = 0;
    uint8_t *epId = NULL;
    switch (pDevicesStatus[devIndex].devType)
    {
    case T748:
    {
        uint8_t epIdBuf[] = {1, 3, 4, 5, 7};
        epNum = sizeof(epIdBuf) / sizeof(uint8_t);
        epId = new uint8_t[epNum];
        memcpy(epId, epIdBuf, sizeof(epIdBuf));
    }
    break;
    default:
    {
        epNum = 0;
    }
    break;
    }

    if (0 == epNum)
    {
        pDevicesStatus[devIndex].epInfo = new SDevEpInfo[1];
        pDevicesStatus[devIndex].epInfo->epNum = 0;
    }
    else
    {
        pDevicesStatus[devIndex].epInfo = new SDevEpInfo[epNum];
        for (uint8_t epIndex = 0; epIndex < epNum; epIndex++)
        {
            pDevicesStatus[devIndex].epInfo[epIndex].epId = epId[epIndex];
            pDevicesStatus[devIndex].epInfo[epIndex].epNum = epNum;
            memset(pDevicesStatus[devIndex].epInfo[epIndex].repLast, 0x00, STRSIZE_EPVALUE);
        }
    }
    delete[] epId;
    epId = NULL;

    return 0;
}

int CDriver::GetEpIndex(int devIndex, uint8_t epID)
{
    for (int epIndex = 0; epIndex < pDevicesStatus[devIndex].epInfo->epNum; epIndex++)
        if (epID == pDevicesStatus[devIndex].epInfo[epIndex].epId)
            return epIndex;

    return -1;
}

int CDriver::GetDevIndex(const char *devID)
{
    for (int devIndex = 0; devIndex < pDevicesInfo->devsNum; devIndex++)
        if (SpellIsSame(pDevicesInfo[devIndex].DevId, devID))
            return devIndex;

    return -1;
}

bool CDriver::GetOrderStart()
{
    int ret = pthread_create(&getOrderThreadID, NULL, GetOrder, this);
    if (0 != ret)
    {
        perror(DRINAME "::GetOrderStart()");
        return false;
    }

    return true;
}

//                            devIndex, epID, devData->epData.EpValue, &ctrlOrder
int CDriver::CtrlConv(int devIndex, int epID, const char *epValue, SDevOrder *ctrlOrder)
{
    string strBuf;
    switch (pDevicesStatus[devIndex].devType)
    {
    case T748:
    {
        switch (epID)
        {
        case 1:
        {
            strBuf += "Main.Power=";
            if (SpellIsSame(epValue, "true"))
                strBuf += "On";
            else if (SpellIsSame(epValue, "false"))
                strBuf += "Off";
            else
                return -1;
        }
        break;
        case 3:
        {
            strBuf += "Main.Mute=";
            if (SpellIsSame(epValue, "true"))
                strBuf += "On";
            else if (SpellIsSame(epValue, "false"))
                strBuf += "Off";
            else
                return -1;
        }
        break;
        case 4:
        {
            strBuf += "Main.Volume+";
        }
        break;
        case 5:
        {
            strBuf += "Main.Volume-";
        }
        break;
        case 7:
        {
            strBuf += "Main.Source=";
            switch (atoi(epValue))
            {
            case 1:
                strBuf += "1";
                break;
            case 2:
                strBuf += "2";
                break;
            case 3:
                strBuf += "3";
                break;
            case 4:
                strBuf += "4";
                break;
            case 5:
                strBuf += "5";
                break;
            case 6:
                strBuf += "6";
                break;
            case 7:
                strBuf += "7";
                break;
            case 8:
                strBuf += "8";
                break;
            case 9:
                strBuf += "9";
                break;
            case 10:
                strBuf += "10";
                break;
            default:
                return -1;
                break;
            }
        }
        break;
        default:
            return -1;
            break;
        }
    }
    break;
    default:
        return -1;
        break;
    }

    ctrlOrder->data = new string;
    (*ctrlOrder->data) = strBuf;

    return 0;
}

int CDriver::BuildCtrlOrder(DevsData *devData)
{
    int devIndex = GetDevIndex(devData->DevId);
    if (-1 == devIndex)
    {
        printf(DRINAME "::BuildCtrlOrder(): Invalid device index.\n");
        return -1;
    }
    uint8_t epID = (uint8_t)atoi(devData->epData.EpId);
    if (-1 == GetEpIndex(devIndex, epID))
    {
        printf(DRINAME "::BuildCtrlOrder(): Invalid ep index.\n");
        return -1;
    }
    SDevOrder ctrlOrder = {0};
    if (-1 == CtrlConv(devIndex, epID, devData->epData.EpValue, &ctrlOrder))
    {
        printf(DRINAME "::BuildCtrlOrder(): Invalid data.\n");
        return -1;
    }

    ctrlOrder.devIndex = devIndex;
    ctrlOrder.orderType = CTRL;

    StrNCpySafe(ctrlOrder.orderId, devData->uuid, STRSIZE_UUID);
    ctrlOrder.repeat = devData->Repeat;
    ctrlOrder.orderTs = devData->data_timestamp;
    ctrlOrder.orderTo = devData->Timeout;

    ctrlOrder.state = WAIT;

    sendList.WriteToQueue(&ctrlOrder);

    return 0;
}

void *CDriver::GetOrder(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;

    SHMFIFOBase shmR;
    int msgType = pDriver->pDriverInfo->Id;
    mymsg *msg = new mymsg;
    int readIndex = MAXSHMSIZE;
    DevsData *devData = new DevsData;

    shmR.msgque_init(SHMREQUESTKEY, false);
    shmR.shmfifo_init(SHMREQUESTKEY, MAXSHMSIZE, sizeof(DevsData), false);
    while (true)
    {
        usleep(INTERVAL_READSHM);
        // 读消息队列
        memset(msg, 0x00, sizeof(mymsg));
        if (-1 == shmR.msgque_recv(msg, MSG_SZ, msgType, false))
        {
            if (ENOMSG != errno)
                perror(DRINAME "::GetOrder()::msgque_recv()");
            continue;
        }
        readIndex = atoi(msg->mtext);
        // 读共享内存
        memset(devData, 0x00, sizeof(DevsData));
        if (-1 == shmR.shmfifo_read(devData, readIndex))
        {
            printf(DRINAME "::GetOrder(): Share memory read error.\n");
            continue;
        }
        if (-1 == pDriver->BuildCtrlOrder(devData))
        {
            printf(DRINAME "::GetOrder(): Build ctrl order error.\n");
            continue;
        }
    }

    delete msg;
    msg = NULL;
    delete devData;
    devData = NULL;

    shmR.shmfifo_destroy(false);

    return (void *)0;
}

bool CDriver::HandleStart()
{
    int ret[2];

    ret[0] = pthread_create(&recvHandleThreadID, NULL, RecvHandle, this);
    if (0 != ret[0])
        perror(DRINAME "::HandleStart()::RecvHandle()");

    ret[1] = pthread_create(&sendHandleThreadID, NULL, SendHandle, this);
    if (0 != ret[1])
        perror(DRINAME "::HandleStart()::SendHandle()");

    if (0 == ret[0] && 0 == ret[1])
        return true;
    else
        return false;
}

void CDriver::BuildBaseEPData(DevsData *devData, int devIndex)
{
    StrNCpySafe(devData->DevId, pDevicesInfo[devIndex].DevId, sizeof(devData->DevId));
    StrNCpySafe(devData->cDevId, pDevicesInfo[devIndex].cDevId, sizeof(devData->cDevId));
    StrNCpySafe(devData->devType, pDevicesInfo[devIndex].DevType, sizeof(devData->devType));
    devData->epNum = 1;
    devData->Timeout = 5 * 1000;
    devData->data_timestamp = GetTimeStamp();
}

void CDriver::BuildWholeEPData(DevsData *devData, int devIndex, uint8_t epID, const char *epValue, double range) //差异上报
{
    if (range >= 0)
    {
        EStrRewrite ret = StrRewrite(pDevicesStatus[devIndex].epInfo[GetEpIndex(devIndex, epID)].repLast, epValue, range, STRSIZE_EPVALUE);
        if (SAME == ret)
            return;
        if (INIT == ret)
            StrNCpySafe(devData->dataType, DEVDATATYPE_SYNC, sizeof(devData->dataType));
        else if (ALTER == ret)
            StrNCpySafe(devData->dataType, DEVDATATYPE_CHANGE, sizeof(devData->dataType));
        else
            return;
    }
    else
        StrNCpySafe(devData->dataType, DEVDATATYPE_CHANGE, sizeof(devData->dataType));

    snprintf(devData->epData.EpId, sizeof(devData->epData.EpId), "%u", epID);
    StrNCpySafe(devData->epData.EpValue, epValue, EPVALUESIZE);
    BuildBaseEPData(devData, devIndex);
}

void CDriver::WriteEpData(int devIndex, uint8_t epID, const char *epValue, double range)
{
    DevsData devData = {0};
    BuildWholeEPData(&devData, devIndex, epID, epValue, range);
    if (1 == devData.epNum)
    {
        shmW.shmfifo_write(&devData, -1);
        usleep(INTERVAL_WRITESHM);
    }
}

int CDriver::RecvParse(SRecvBuffer *recvBuf)
{
#ifdef DEBUG
    printf(DRINAME "::TCPRecv %lu bytes:", recvBuf->dataLen);
    for (int dataIndex = 0; dataIndex < recvBuf->dataLen; dataIndex++)
        printf(" %02X", recvBuf->data[dataIndex]);
    printf(".\n");
#endif

    if (pDevicesStatus->devNum <= 0)
        return -1;
    // 设备上线
    pDevicesStatus[0].syncToc = 0; //同步超时计数
    RefreshDevStatus(0, STATE_ONLINE);
    WriteDevStatus(0);
    switch (pDevicesStatus[0].devType)
    {
    case T748:
    {
        if (recvBuf->dataLen < 3)
            return -1;
        string strBuf;
        strBuf.assign((char *)(recvBuf->data + 1), recvBuf->dataLen - 2); 
        size_t eqPos;
        if (strBuf.npos == (eqPos = strBuf.find('=')))
            return -1;
        string key = strBuf.substr(0, eqPos);
        string value = strBuf.substr(eqPos + 1);
        if ("Main.Power" == key)
        {
            if ("On" == value)
                // devIndex,  epID, epValue, range
                WriteEpData(0, 1, "true", 0);
            else if ("Off" == value)
                WriteEpData(0, 1, "false", 0);
        }
        else if ("Main.Mute" == key)
        {
            if ("On" == value)
                WriteEpData(0, 3, "true", 0);
            else if ("Off" == value)
                WriteEpData(0, 3, "false", 0);
        }
        else if ("Main.Source" == key)
        {
            if ("1" == value)
                WriteEpData(0, 7, "1", 0);
            else if ("2" == value)
                WriteEpData(0, 7, "2", 0);
            else if ("3" == value)
                WriteEpData(0, 7, "3", 0);
            else if ("4" == value)
                WriteEpData(0, 7, "4", 0);
            else if ("5" == value)
                WriteEpData(0, 7, "5", 0);
            else if ("6" == value)
                WriteEpData(0, 7, "6", 0);
            else if ("7" == value)
                WriteEpData(0, 7, "7", 0);
            else if ("8" == value)
                WriteEpData(0, 7, "8", 0);
            else if ("9" == value)
                WriteEpData(0, 7, "9", 0);
            else if ("10" == value)
                WriteEpData(0, 7, "10", 0);
        }
    }
    break;
    default:
        return -1;
        break;
    }

    return 0;
}

int CDriver::RecvCallback(TransferData *data, int type)
{
    switch (type)
    {
    case NOTIFY:
    {
        SRecvBuffer recvBuf = {0};
        recvBuf.dataLen = data->param3;
        recvBuf.data = new uint8_t[recvBuf.dataLen];
        memcpy(recvBuf.data, data->Data, recvBuf.dataLen);
        pDriverStatic->recvList.WriteToQueue(&recvBuf);
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

int CDriver::WriteDriStatus(const char *driStatus)
{
    if (SAME == StrRewrite(pCommParam->connectStatus, driStatus, 0, STRSIZE_STATUS))
        return -1;

    DevsData devData = {0};
    StrNCpySafe(devData.DevId, pDriverInfo->Name, sizeof(devData.DevId));
    StrNCpySafe(devData.devType, pDriverInfo->Type, sizeof(devData.devType));
    StrNCpySafe(devData.dataType, DEVDATATYPE_STATUS, sizeof(devData.dataType));
    devData.Timeout = 5 * 1000;
    devData.epNum = 1;
    StrNCpySafe(devData.epData.EpId, DEVDATATYPE_STATUS, sizeof(devData.epData.EpId));
    StrNCpySafe(devData.epData.EpValue, driStatus, EPVALUESIZE);
    devData.data_timestamp = GetTimeStamp();
    shmW.shmfifo_write(&devData, -1);
    usleep(INTERVAL_WRITESHM);

    return 0;
}

int CDriver::WriteDevStatus(int devIndex)
{
    DevsData devData = {0};
    char strStatus[STRSIZE_STATUS] = {0};

    StrNCpySafe(devData.DevId, pDevicesInfo[devIndex].DevId, sizeof(devData.DevId));
    StrNCpySafe(devData.cDevId, pDevicesInfo[devIndex].cDevId, sizeof(devData.cDevId));
    StrNCpySafe(devData.devType, pDevicesInfo[devIndex].DevType, sizeof(devData.devType));
    StrNCpySafe(devData.dataType, DEVDATATYPE_STATUS, sizeof(devData.dataType));
    devData.Timeout = 5 * 1000;
    devData.epNum = 1;
    StrNCpySafe(devData.epData.EpId, DEVDATATYPE_STATUS, sizeof(devData.epData.EpId));
    // 在线状态
    if (pDevicesStatus[devIndex].isOnline)
        StrNCpySafe(strStatus, DEVICE_ONLINE, STRSIZE_STATUS);
    else
        StrNCpySafe(strStatus, DEVICE_OFFLINE, STRSIZE_STATUS);
    if (SAME != StrRewrite(pDevicesStatus[devIndex].strOnline, strStatus, 0, STRSIZE_STATUS))
    {
        StrNCpySafe(devData.epData.EpValue, strStatus, EPVALUESIZE);
        devData.data_timestamp = GetTimeStamp();
        shmW.shmfifo_write(&devData, -1);
        usleep(INTERVAL_WRITESHM);
    }
    // 异常状态
    /* if (pDevicesStatus[devIndex].isFault)
        StrNCpySafe(strStatus, DEVICE_FAULT, STRSIZE_STATUS);
    else
        StrNCpySafe(strStatus, DEVICE_USUAL, STRSIZE_STATUS);
    if (SAME != StrRewrite(pDevicesStatus[devIndex].strFault, strStatus, 0, STRSIZE_STATUS))
    {
        StrNCpySafe(devData.epData.EpValue, strStatus, EPVALUESIZE);
        devData.data_timestamp = GetTimeStamp();
        shmW.shmfifo_write(&devData, -1);
        usleep(INTERVAL_WRITESHM);
    } */

    return 0;
}

int CDriver::CheckNetworkStatus()
{
    if (false == tcp.isTCPConnected())
    {
        if (false == SpellIsSame(pCommParam->connectStatus, ""))
            WriteDriStatus(GATEWAY_OFFLINE);
        tcp.Disconnect();
        int ret = tcp.ConnectToServer(pCommParam->ip, pCommParam->port, RecvCallback); //  回调函数接收数据
        if (ret < 3)
        {
            WriteDriStatus(GATEWAY_OFFLINE);
            sleep(4);
        }
        else
        {
            WriteDriStatus(GATEWAY_ONLINE);
            pCommParam->connectedTs = GetTimeStamp();
            for (int devIndex = 0; devIndex < pDevicesStatus->devNum; devIndex++)
            {
                memset(pDevicesStatus[devIndex].strOnline, 0x00, STRSIZE_STATUS);
                memset(pDevicesStatus[devIndex].strFault, 0x00, STRSIZE_STATUS);
                WriteDevStatus(devIndex);
            }
        }
    }
    else if (pDevicesStatus->devNum > 0)
    {
        bool devAllOff = true;
        for (int devIndex = 0; devIndex < pDevicesStatus->devNum; devIndex++)
            if (pDevicesStatus[devIndex].isOnline)
            {
                devAllOff = false;
                break;
            }
        if (devAllOff && (pCommParam->connectedTs + DURATION_CONNECTED_VALIDITY) < GetTimeStamp())
        {
            tcp.Disconnect();
            int ret = tcp.ConnectToServer(pCommParam->ip, pCommParam->port, RecvCallback);
            if (ret < 3)
                WriteDriStatus(GATEWAY_OFFLINE);
            else
            {
                WriteDriStatus(GATEWAY_ONLINE);
                pCommParam->connectedTs = GetTimeStamp();
            }
        }
    }

    return 0;
}

int CDriver::ExecuteOrder(SDevOrder *order)
{
    if (NULL == order->data || 0 == (*order->data).length())
    {
        order->state = DONE;
        return -1;
    }
    uint8_t *sendData = NULL;
    size_t sendLen = 0;

    switch (pDevicesStatus[order->devIndex].devType)
    {
    case T748:
    {
        sendData = new uint8_t[(*order->data).length() + 2];
        sendData[sendLen++] = 0x0D;
        memcpy(sendData + sendLen, (*order->data).c_str(), (*order->data).length());
        sendLen += (*order->data).length();
        sendData[sendLen++] = 0x0D;
    }
    break;
    default:
        return -1;
        break;
    }

#ifdef DEBUG
    printf(DRINAME "::TCPSend %lu bytes:", sendLen);
    for (int dataIndex = 0; dataIndex < sendLen; dataIndex++)
        printf(" %02X", sendData[dataIndex]);
    printf(".\n");
#endif

    tcp.Send(sendData, sendLen);
    usleep(300 * 1000);

    delete[] sendData;
    sendData = NULL;

    order->state = DONE;
    return 0;
}

int CDriver::WriteCtrlRsp(SDevOrder *order)
{
    int devIndex = order->devIndex;
    DevsData *devData = new DevsData;
    memset(devData, 0x00, sizeof(DevsData));
    StrNCpySafe(devData->DevId, pDevicesInfo[devIndex].DevId, sizeof(devData->DevId));
    StrNCpySafe(devData->uuid, order->orderId, sizeof(devData->uuid));
    StrNCpySafe(devData->cDevId, pDevicesInfo[devIndex].cDevId, sizeof(devData->cDevId));
    StrNCpySafe(devData->devType, pDevicesInfo[devIndex].DevType, sizeof(devData->devType));
    StrNCpySafe(devData->dataType, DEVDATATYPE_REQ, sizeof(devData->dataType));
    devData->Timeout = 5 * 1000;
    devData->epNum = 0;
    devData->data_timestamp = GetTimeStamp();
    int ret = shmW.shmfifo_write(devData, -1);
    usleep(INTERVAL_WRITESHM);
    delete devData;
    devData = NULL;
    if (-1 == ret)
    {
        printf(DRINAME "::WriteCtrlRsp()::shmfifo_write().\n");
        return -1;
    }

    return 0;
}

int CDriver::RefreshDevStatus(int devIndex, EDevState state)
{
    switch (state)
    {
    case STATE_ONLINE:
    {
        pDevicesStatus[devIndex].isOnline = true;
        pDevicesStatus[devIndex].isFault = false;
    }
    break;
    case STATE_OFFLINE:
    {
        pDevicesStatus[devIndex].isOnline = false;
        pDevicesStatus[devIndex].isFault = false;
    }
    break;
    default:
        break;
    }

    return 0;
}

int CDriver::BuildSyncOrder()
{
    if (pDevicesStatus->devNum <= 0)
        return -1;

    string strBuf;
    switch (pDevicesStatus[0].devType)
    {
    case T748:
        switch (syncState)
        {
        case SYNC_EP01:
        {
            strBuf += "Main.Power?";
            syncState = SYNC_EP03;
        }
        break;
        case SYNC_EP03:
        {
            strBuf += "Main.Mute?";
            syncState = SYNC_EP07;
        }
        break;
        case SYNC_EP07:
        {
            strBuf += "Main.Source?";
            syncState = SYNC_EP01;
        }
        break;
        default:
            return -1;
            break;
        }
        break;
    default:
        return -1;
        break;
    }

    if (pDevicesStatus[0].syncToc < 8)
        pDevicesStatus[0].syncToc++;
    else
    {
        RefreshDevStatus(0, STATE_OFFLINE);
        WriteDevStatus(0);
    }

    SDevOrder syncOrder = {0};
    syncOrder.devIndex = 0;
    syncOrder.orderType = SYNC;
    syncOrder.data = new string;
    (*syncOrder.data) = strBuf;
    syncOrder.state = WAIT;

    ExecuteOrder(&syncOrder);

    delete syncOrder.data;
    syncOrder.data = NULL;

    return 0;
}

void *CDriver::SendHandle(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;
    SDevOrder *ctrlOrder = new SDevOrder;

    pDriver->CheckNetworkStatus();

    while (true)
    {
        memset(ctrlOrder, 0x00, sizeof(SDevOrder));
        if (pDriver->sendList.ReadFromQueue(ctrlOrder))
        {
            switch (ctrlOrder->orderType)
            {
            case CTRL:
            {
                int repeatExe = -1; //重复执行次数
                while (true)
                {
                    pDriver->ExecuteOrder(ctrlOrder);
                    repeatExe++;
                    // if (0 == repeatExe && -1 == pDriver->WriteCtrlRsp(ctrlOrder))
                    //     printf(DRINAME "::WriteCtrlRsp(): Report control response error.\n");
                    if (DONE == ctrlOrder->state ||
                        ctrlOrder->repeat <= repeatExe ||                          
                        (ctrlOrder->orderTs + ctrlOrder->orderTo) < GetTimeStamp()) //命令超时时间(ms) + 命令时间戳(ms)
                        break;
                    else
                        continue;
                }
            }
            break;
            default:
                pDriver->ExecuteOrder(ctrlOrder);
                break;
            }

            delete ctrlOrder->data;
            ctrlOrder->data = NULL;
        }
        else //  未从消息队列获取数据
        {
            pDriver->BuildSyncOrder();
        }

        pDriver->CheckNetworkStatus();
        usleep(INTERVAL_VOID); //10 * 1000
    }

    delete ctrlOrder;
    ctrlOrder = NULL;

    return (void *)0;
}

void *CDriver::RecvHandle(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;
    SRecvBuffer *recvBuf = new SRecvBuffer;

    while (true)
    {
        memset(recvBuf, 0x00, sizeof(SRecvBuffer));
        if (pDriver->recvList.ReadFromQueue(recvBuf))
        {
            pDriver->RecvParse(recvBuf);
            delete[] recvBuf->data;
            recvBuf->data = NULL;
        }

        usleep(INTERVAL_VOID);
    }

    delete recvBuf;
    recvBuf = NULL;

    return (void *)0;
}

int CDriver::HandleCfgData(CfgDevsData *cfgData)
{
    if (SpellIsSame(cfgData->dataType, CFG_autoSearch)) // 判断配置数据 的类型
    {
        char devType[STRSIZE_DEVTYPE] = {0};                                   //  设备类型
        for (int paramIndex = 0; paramIndex < cfgData->paramNum; paramIndex++) //  获取设备类型
            if (SpellIsSame(cfgData->paramData[paramIndex].Id, "devType"))     //  循环遍历获取devType
            {
                StrNCpySafe(devType, cfgData->paramData[paramIndex].Value, STRSIZE_DEVTYPE);
                break;
            }
        if (SpellIsSame(devType, "") || UNKNOW == DevTypeConv(devType)) // 设备类型若为空或不存在则退出
            return -1;

        char adapterName[IFNAMSIZ] = {0};                     //  适配器名字
        if (0 == GetAdapterName(pCommParam->ip, adapterName)) //  获取适配器名
        {
            CMLGWConfig rs485(adapterName, PORT_BASE + pDriverInfo->Id, pCommParam->ip);
            if (0 == pDevicesStatus->devNum)
            {
                if (rs485.ConfigUART(115200, DATABIT_8, STOPBIT_1, CHK_NONE))
                    printf(DRINAME "::ConfigUART() error!\n");
                if (rs485.ConfigSOCK(PRT_TCPS, pCommParam->ip, pCommParam->port))
                    printf(DRINAME "::ConfigSOCK() error!\n");
                if (rs485.ConfigReboot())
                    printf(DRINAME "::ConfigReboot() error!\n");
                sleep(4);
            }
            if (rs485.ConfigMAC())
            {
                printf(DRINAME "::ConfigMAC() error!\n");
                StrNCpySafe(pCommParam->mac, pDriverInfo->Name, STRSIZE_MAC); // ???
            }
            else
                rs485.GetMAC(pCommParam->mac);
        }
        else
            return -1;

        if (false == SpellIsSame(pCommParam->mac, ""))
        {
            /*
                char DevId[32];
                char uuid[128];
                char pId[32];
                char pType[16];
                char devType[16];
                char dataType[16];
                char DriverName[32];
                int lastLevel;
                int total;
                int index;
                long Timeout;
                int paramNum;   
                unsigned long data_timestamp;
                CfgParam paramData[16];
            */
            CfgDevsData repDevData = {0};
            StrNCpySafe(repDevData.uuid, cfgData->uuid, sizeof(repDevData.uuid));
            StrNCpySafe(repDevData.pId, cfgData->DevId, sizeof(repDevData.pId)); // ???
            StrNCpySafe(repDevData.pType, cfgData->devType, sizeof(repDevData.pType));
            StrNCpySafe(repDevData.devType, devType, sizeof(repDevData.devType));
            StrNCpySafe(repDevData.dataType, CFG_searchRep, sizeof(repDevData.dataType));
            StrNCpySafe(repDevData.DriverName, cfgData->DriverName, sizeof(repDevData.DriverName));
            repDevData.lastLevel = 1; //最后一层概念
            repDevData.total = 1;
            repDevData.index = 1; //  从1开始计数？
            repDevData.Timeout = 5 * 1000;
            repDevData.paramNum = 2;
            StrNCpySafe(repDevData.paramData[0].Id, "mac", sizeof(repDevData.paramData[0].Id));
            StrNCpySafe(repDevData.paramData[0].Value, pCommParam->mac, sizeof(repDevData.paramData[0].Value));
            StrNCpySafe(repDevData.paramData[1].Id, "address", sizeof(repDevData.paramData[1].Id));
            StrNCpySafe(repDevData.paramData[1].Value, "RS232", sizeof(repDevData.paramData[1].Value));
            repDevData.data_timestamp = GetTimeStamp();
            shmCfgW.shmfifo_cfg_write(&repDevData, -1);
            usleep(INTERVAL_WRITESHM);
        }
    }

    return 0;
}

void *CDriver::GetConfigOrder(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;

    SHMFIFOBase shmCfgR;
    int msgType = pDriver->pDriverInfo->Id;
    mymsg *msg = new mymsg;
    int readIndex = MAXSHMCFGSIZE;
    CfgDevsData *cfgData = new CfgDevsData;

    shmCfgR.msgque_init(SHMCFGREQKEY, false);
    shmCfgR.shmfifo_init(SHMCFGREQKEY, MAXSHMCFGSIZE, sizeof(CfgDevsData), false);
    while (true)
    {
        usleep(INTERVAL_READSHM);
        // 读消息队列
        memset(msg, 0x00, sizeof(mymsg));
        if (-1 == shmCfgR.msgque_recv(msg, MSG_SZ, msgType, false))
        {
            if (ENOMSG != errno)
                perror(DRINAME "::GetConfigOrder()::msgque_recv()");
            continue;
        }
        readIndex = atoi(msg->mtext);
        // 读共享内存
        memset(cfgData, 0x00, sizeof(CfgDevsData));
        if (-1 == shmCfgR.shmfifo_cfg_read(cfgData, readIndex))
        {
            printf(DRINAME "::GetConfigOrder(): Share memory read error.\n");
            continue;
        }
        pDriver->HandleCfgData(cfgData);
    }
    delete msg;
    msg = NULL;
    delete cfgData;
    cfgData = NULL;

    shmCfgR.shmfifo_destroy(false);

    return (void *)0;
}

bool CDriver::GetConfigOrderStart()
{
    int ret = pthread_create(&getConfigOrderThreadID, NULL, GetConfigOrder, this);
    if (ret != 0)
    {
        perror(DRINAME "::GetConfigOrderStart()");
        return false;
    }

    return true;
}