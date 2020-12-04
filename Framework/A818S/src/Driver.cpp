#include "../include/Driver.h"

CDriver *CDriver::pDriverStatic = NULL;

CDriver::CDriver(int argDriIndex, int argDevIndex, int argDevNum,
                 int argMaxDriNum, int argMaxDevNum)
{
    searchId = -1;
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
    if (0 < rawDevNum)
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
            pDevicesStatus[devIndex].devId = atoi(pDevicesInfo[devIndex].Ids);
            pDevicesStatus[devIndex].devType = DevTypeConv(pDevicesInfo[devIndex].DevType);
            pDevicesStatus[devIndex].isOnline = true;
            memset(pDevicesStatus[devIndex].strOnline, 0x00, STRSIZE_STATUS);
            pDevicesStatus[devIndex].isFault = false;
            memset(pDevicesStatus[devIndex].strFault, 0x00, STRSIZE_STATUS);
            DevEpInfoInit(devIndex);
            pDevicesStatus[devIndex].syncToc = 0;
        }
        // 同步
        syncIndex = 0;
        switch (pDevicesStatus[0].devType)
        {
        case A818S:
            syncState = SYNC_EP02;
            break;
        default:
            syncState = SYNC_EP02;
            break;
        }
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

    if (DEVTYPE_A818S == devTypeBuf)
        ret = A818S;
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
    case A818S:
    {
        uint8_t epIdBuf[] = {2};
        epNum = sizeof(epIdBuf) / sizeof(uint8_t);
        epId = new uint8_t[epNum];
        memcpy(epId, epIdBuf, sizeof(epIdBuf));
    }
    break;
    default:
        epNum = 0;
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

int CDriver::CtrlConv(int devIndex, int epID, const char *epValue, SDevOrder *ctrlOrder)
{
    string strHex;

    switch (pDevicesStatus[devIndex].devType)
    {
    case A818S:
        switch (epID)
        {
        case 2:
        {
            string strChannelId;
            int channelId;
            string channelState;
            Json::Reader reader;
            Json::Value jsEpValue;
            if (!reader.parse(epValue, jsEpValue, false))
                return -1;
            // 获取通道号
            Json::Value::Members member = jsEpValue.getMemberNames();
            if (1 != member.size())
                return -1;
            strChannelId = *member.begin();
            stringstream recvChannelId(strChannelId);
            recvChannelId >> channelId;
            // 判断通道号正确性
            if (channelId < 0 || 8 < channelId)
                return -1;
            // 获取通道状态
            channelState = jsEpValue[strChannelId].asString();
            // 写设备ID
            char formatIds[3] = {0};
            snprintf(formatIds, sizeof(formatIds), "%02X", atoi(pDevicesInfo[devIndex].Ids));
            strHex += formatIds;
            // 全开/全关
            if (0 == channelId)
            {
                if ("true" == channelState)
                    strHex += "2903FFFF01";
                else if ("false" == channelState)
                    strHex += "2903FF0002";
                else
                    return -1;
                break;
            }
            // 控制单个通道
            strHex += "2802";
            // 写通道号
            snprintf(formatIds, sizeof(formatIds), "%02X", channelId);
            strHex += formatIds;
            // 判断通道状态并写通道状态
            if (channelState == "true")
                strHex += "01";
            else if (channelState == "false")
                strHex += "00";
            else
                return -1;
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

    ctrlOrder->dataLen = StrToHex(ctrlOrder->data, BYTESIZE_TRANSFERDATA, strHex.c_str());
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
    if (0 <= range)
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
    // 检验数据格式及其长度 头尾 长度6
    if (0xAE == recvBuf->data[0] && 0xBB == recvBuf->data[1] && 0xFF == recvBuf->data[2] &&
        0xAC == recvBuf->data[5] && 6 == recvBuf->dataLen)
    {
        // 获取设备索引和通道状态
        uint8_t recvId = recvBuf->data[3];
        uint8_t channelState = recvBuf->data[4];
        // 设备上线
        int devIndex;
        for (devIndex = 0; devIndex < pDevicesStatus->devNum; devIndex++)
        {
            if (atoi(pDevicesInfo[devIndex].Ids) == recvId)
            {
                pDevicesStatus[devIndex].syncToc = 0;
                RefreshDevStatus(devIndex, STATE_ONLINE);
                WriteDevStatus(devIndex);
                break;
            }
        }
        if (pDevicesStatus->devNum == devIndex)
        {
            searchId = recvId;
            return -1;
        }
        switch (pDevicesStatus[devIndex].devType)
        {
        case A818S:
        {
            // 遍历每个通道(按位判断7-0)
            char chId[16];
            Json::Value jsEpData;
            // 全开/全关判断
            if (0x00 == channelState)
                jsEpData["0"] = "false";
            else
                jsEpData["0"] = "true";
            for (int bitPos = 0; bitPos < 8; bitPos++)
            {
                snprintf(chId, sizeof(chId), "%d", (bitPos + 1));
                if (1 == ((channelState >> bitPos) & 0x01)) //  判断单通道状态
                    jsEpData[chId] = "true";
                else
                    jsEpData[chId] = "false";
            }
            Json::FastWriter fw;
            string strEpData = fw.write(jsEpData);
            // 序列化
            StringEscape(strEpData);
            StringEscape(strEpData);
            // 写功能数据
            WriteEpData(devIndex, 2, strEpData.c_str(), 0);
        }
        break;
        default:
            return -1;
            break;
        }
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
    else if (0 < pDevicesStatus->devNum)
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
    if (NULL == order->data)
    {
        order->state = DONE;
        return -1;
    }
    uint8_t sendData[BYTESIZE_TRANSFERDATA] = {0};
    size_t sendLen = 0;
    if (SEARCH == order->orderType || A818S == pDevicesStatus[order->devIndex].devType)
    {
        sendData[sendLen++] = 0xAE;
        sendData[sendLen++] = 0xA8;
        memcpy(sendData + sendLen * sizeof(uint8_t), order->data, order->dataLen);
        sendLen += order->dataLen;
        sendData[sendLen++] = 0xAC;
    }
    else
        return -1;

#ifdef DEBUG
    printf(DRINAME "::TCPSend %lu bytes:", sendLen);
    for (int dataIndex = 0; dataIndex < sendLen; dataIndex++)
        printf(" %02X", sendData[dataIndex]);
    printf(".\n");
#endif
    tcp.Send(sendData, sendLen);
    usleep(300 * 1000);

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

int CDriver::SyncConv(SDevOrder &syncOrder)
{
    int ret = 0;
    string strHex;
    bool syncOver = false;

    switch (pDevicesStatus[syncIndex].devType)
    {
    case A818S:
    {
        switch (syncState)
        {
        case SYNC_EP02:
        {
            char idBuf[3];
            snprintf(idBuf, sizeof(idBuf), "%02X", pDevicesStatus[syncIndex].devId);
            strHex += idBuf;
            strHex += "2B";
            syncOver = true;
        }
        break;
        default:
        {
            ret = -1;
            syncOver = true;
        }
        break;
        }
    }
    break;
    default:
    {
        ret = -1;
        syncOver = true;
    }
    break;
    }

    if (0 == ret)
    {
        syncOrder.devIndex = syncIndex;
        syncOrder.dataLen = StrToHex(syncOrder.data, BYTESIZE_TRANSFERDATA, strHex.c_str());
    }
    if (syncOver)
    {
        syncIndex = (syncIndex + 1) % pDevicesStatus->devNum;
        switch (syncIndex)
        {
        case A818S:
            syncState = SYNC_EP02;
            break;
        default:
            syncState = SYNC_EP02;
            break;
        }
    }

    return ret;
}

int CDriver::BuildSyncOrder()
{
    if (pDevicesStatus->devNum <= 0)
        return -1;

    SDevOrder syncOrder = {0};
    if (-1 == SyncConv(syncOrder))
        return -1;
    syncOrder.orderType = SYNC;
    syncOrder.state = WAIT;
    // 同步超时
    if (pDevicesStatus[syncOrder.devIndex].syncToc < 8)
        pDevicesStatus[syncOrder.devIndex].syncToc++;
    else
    {
        RefreshDevStatus(syncOrder.devIndex, STATE_OFFLINE);
        WriteDevStatus(syncOrder.devIndex);
    }
    ExecuteOrder(&syncOrder);

    return 0;
}

int CDriver::DriverConfigLink()
{
    char adapterName[IFNAMSIZ] = {0};                     //  适配器名字
    if (0 == GetAdapterName(pCommParam->ip, adapterName)) //  获取适配器名
    {
        CMLGWConfig rs485(adapterName, PORT_BASE + pDriverInfo->Id, pCommParam->ip);
        if (0 == pDevicesStatus->devNum)
        {
            if (rs485.ConfigUART(9600, DATABIT_8, STOPBIT_1, CHK_NONE))
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
            StrNCpySafe(pCommParam->mac, pDriverInfo->Name, STRSIZE_MAC);
        }
        else
            rs485.GetMAC(pCommParam->mac);
        return 0;
    }
    else
        return -1;
}

void *CDriver::SendHandle(void *pParam)
{
    CDriver *pDriver = (CDriver *)pParam;
    SDevOrder *ctrlOrder = new SDevOrder;

    pDriver->DriverConfigLink();
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
                    /* if (0 == repeatExe && -1 == pDriver->WriteCtrlRsp(ctrlOrder))
                        printf(DRINAME "::WriteCtrlRsp(): Report control response error.\n"); */
                    if (DONE == ctrlOrder->state ||
                        ctrlOrder->repeat <= repeatExe ||
                        (ctrlOrder->orderTs + ctrlOrder->orderTo) < GetTimeStamp())
                        break;
                    else
                        continue;
                }
            }
            break;
            default:
            {
                pDriver->ExecuteOrder(ctrlOrder);
            }
            break;
            }
        }
        else // 队列中无指令
        {
            pDriver->BuildSyncOrder();
        }
        pDriver->CheckNetworkStatus();
        usleep(INTERVAL_VOID);
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
    if (SpellIsSame(cfgData->dataType, CFG_manualSearch)) // 判断配置数据 的类型
    {
        char id[STRSIZE_ID] = {0};           // 设备ID
        char devType[STRSIZE_DEVTYPE] = {0}; //  设备类型
        for (int paramIndex = 0; paramIndex < cfgData->paramNum; paramIndex++)
        {
            if (SpellIsSame(cfgData->paramData[paramIndex].Id, "devType"))
                StrNCpySafe(devType, cfgData->paramData[paramIndex].Value, STRSIZE_DEVTYPE);
            else if (SpellIsSame(cfgData->paramData[paramIndex].Id, "id"))
                StrNCpySafe(id, cfgData->paramData[paramIndex].Value, STRSIZE_ID);
        }
        if (SpellIsSame(devType, "") || UNKNOW == DevTypeConv(devType)) // 设备类型若为空或不存在则退出
            return -1;
        // 生成搜索指令
        SDevOrder *searchOrder = new SDevOrder;
        searchOrder->devIndex = -1;
        searchOrder->orderType = SEARCH;
        searchOrder->dataLen = 2;
        searchOrder->data[0] = atoi(id);
        searchOrder->data[1] = 0x2B;
        searchOrder->state = WAIT;
        sendList.WriteToQueue(searchOrder);
        delete searchOrder;
        searchOrder = NULL;
        // 等待搜索结果
        sleep(8);
        if (atoi(id) == searchId &&
            false == SpellIsSame(pCommParam->mac, ""))
        {
            // 数据上报
            CfgDevsData repDevData = {0};
            StrNCpySafe(repDevData.uuid, cfgData->uuid, sizeof(repDevData.uuid));
            StrNCpySafe(repDevData.pId, cfgData->DevId, sizeof(repDevData.pId));
            StrNCpySafe(repDevData.pType, cfgData->devType, sizeof(repDevData.pType));
            StrNCpySafe(repDevData.devType, devType, sizeof(repDevData.devType));
            StrNCpySafe(repDevData.dataType, CFG_searchRep, sizeof(repDevData.dataType));
            StrNCpySafe(repDevData.DriverName, cfgData->DriverName, sizeof(repDevData.DriverName));
            repDevData.lastLevel = 1;
            repDevData.total = 1;
            repDevData.index = 1;
            repDevData.Timeout = 5 * 1000;
            repDevData.paramNum = 2;
            StrNCpySafe(repDevData.paramData[0].Id, "mac", sizeof(repDevData.paramData[0].Id));
            StrNCpySafe(repDevData.paramData[0].Value, pCommParam->mac, sizeof(repDevData.paramData[0].Value));
            StrNCpySafe(repDevData.paramData[1].Id, "address", sizeof(repDevData.paramData[1].Id));
            StrNCpySafe(repDevData.paramData[1].Value, id, sizeof(repDevData.paramData[1].Value));
            repDevData.data_timestamp = GetTimeStamp();
            shmCfgW.shmfifo_cfg_write(&repDevData, -1);
            usleep(INTERVAL_WRITESHM);
        }
        searchId = -1;
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