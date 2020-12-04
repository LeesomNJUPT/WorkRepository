/*
 * @Author: mikey.zhaopeng 
 * @Date: 2020-09-24 13:47:24 
 * @Last Modified by: mikey.zhaopeng
 * @Last Modified time: 2020-09-24 14:17:34
 */
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
    shmDriR.shmfifo_simple_read(pDriverInfo, argDriIndex);     // 这里读共享内存是什么意思？
    shmDriR.shmfifo_destroy(false);
    // 设备信息
    int rawDevNum = argDevNum;        // raw含义: read and write
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
                                      //  void *buf,       int read_index
            shmDevR.shmfifo_simple_read(rawDev + rawIndex, (argDevIndex + rawIndex) % argMaxDevNum);     //  ???
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
    pCommParam->port = PORT_DEFAULT;            //默认端口号8100？？？
    memset(pCommParam->mac, 0x00, STRSIZE_MAC);
    pCommParam->connectedTS = 0;
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
            DevEPInfoInit(devIndex);
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

    if (DEVTYPE_SERIES == devTypeBuf)
        ret = SERIES;
    else
        ret = UNKNOW;

    return ret;
}

int CDriver::DevEPInfoInit(int devIndex)
{
    uint8_t epNum = 0;
    uint8_t *epID = NULL;
    switch (pDevicesStatus[devIndex].devType)
    {
        case SERIES:
        {
            uint8_t epIDBuf[] = {1, 3, 4, 5, 7};
            epNum = sizeof(epIDBuf) / sizeof(uint8_t);
            epID = new uint8_t[epNum];
            memcpy(epID, epIDBuf, sizeof(epIDBuf));
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
        pDevicesStatus[devIndex].epInfo = new SDevEPInfo[1];
        pDevicesStatus[devIndex].epInfo->epNum = 0;
    }
    else
    {
        pDevicesStatus[devIndex].epInfo = new SDevEPInfo[epNum];
        for (uint8_t epIndex = 0; epIndex < epNum; epIndex++)
        {
            pDevicesStatus[devIndex].epInfo[epIndex].epID = epID[epIndex];
            pDevicesStatus[devIndex].epInfo[epIndex].epNum = epNum;
            memset(pDevicesStatus[devIndex].epInfo[epIndex].repLast, 0x00, STRSIZE_EPVALUE);
        }
    }

    return 0;
}

int CDriver::GetEPIndex(int devIndex, uint8_t epID)
{
    for (int epIndex = 0; epIndex < pDevicesStatus[devIndex].epInfo->epNum; epIndex++)
        if (epID == pDevicesStatus[devIndex].epInfo[epIndex].epID)    
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

int CDriver::CtrlConv(int devIndex, int epID, const char *epValue, SDevOrder *ctrlOrder)  // 字节流控制命令转换
{
    string strHex;
    switch (pDevicesStatus[devIndex].devType)
    {
        case SERIES:
        {
            switch (epID)
            {
                case 1:
                {
                    if (SpellIsSame(epValue, "true"))
                        strHex += "74030621013B";
                    else if (SpellIsSame(epValue, "false"))
                        strHex += "74030621003A";
                    else
                        return -1;
                }
                break;
                case 3:
                {
                    if (SpellIsSame(epValue, "true"))
                        strHex += "590306080107";
                    else if (SpellIsSame(epValue, "false"))
                        strHex += "5A0306080007";
                    else
                        return -1;
                }
                break;
                case 4:
                {
                    strHex += "74030620013A";
                }
                break;
                case 5:
                {
                    strHex += "740306200039";
                }
                break;
                case 7:
                {
                    switch (atoi(epValue))
                    {
                    case 1:
                        strHex += "5B0306030508";
                        break;
                    case 2:
                        strHex += "5C030603060A";
                        break;
                    case 3:
                        strHex += "5D030600370C";
                        break;
                    case 4:
                        strHex += "5E030603080E";
                        break;
                    case 5:
                        strHex += "610306030009";
                        break;
                    case 6:
                        strHex += "600306030109";
                        break;
                    case 7:
                        strHex += "570306030302";
                        break;
                    case 8:
                        strHex += "580306030404";
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

    // fake status
    switch (epID)
    {
    case 1:
    case 3:
    case 7:
        WriteEPData(devIndex, epID, epValue, 0);
        break;
    default:
        break;
    }
    ctrlOrder->dataLen = StrToHex(ctrlOrder->data, BYTESIZE_TRANSFERDATA, strHex.c_str());

    return 0;
}

int CDriver::BuildCtrlOrder(DevsData *devData)
{
    int devIndex = GetDevIndex(devData->DevId);            // 获取设备ID
    if (-1 == devIndex)
    {
        printf(DRINAME "::BuildCtrlOrder(): Invalid device index.\n");
        return -1;
    }
    uint8_t epID = (uint8_t)atoi(devData->epData.EpId);     
    if (-1 == GetEPIndex(devIndex, epID))                   
    {
        printf(DRINAME "::BuildCtrlOrder(): Invalid ep index.\n");
        return -1;
    }
    SDevOrder ctrlOrder = {0};
    if (-1 == CtrlConv(devIndex, epID, devData->epData.EpValue, &ctrlOrder))   // 控制命令转化
    {
        printf(DRINAME "::BuildCtrlOrder(): Invalid data.\n");
        return -1;
    }

    ctrlOrder.devIndex = devIndex;
    ctrlOrder.orderType = CTRL;

    StrNCpySafe(ctrlOrder.orderID, devData->uuid, STRSIZE_UUID);
    ctrlOrder.repeat = devData->Repeat;
    ctrlOrder.orderTS = devData->data_timestamp;
    ctrlOrder.orderTO = devData->Timeout;

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
    int readIndex = MAXSHMSIZE;  //1000
    DevsData *devData = new DevsData;

    shmR.msgque_init(SHMREQUESTKEY, false);      // 加上false的含义？？？都传进去false了，里面还判断干啥
    shmR.shmfifo_init(SHMREQUESTKEY, MAXSHMSIZE, sizeof(DevsData), false);
    while (true)
    {
        usleep(INTERVAL_READSHM);    //10 * 1000 us  轮询，留出时间片
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

void CDriver::BuildWholeEPData(DevsData *devData, int devIndex, uint8_t epID, const char *epValue, double range)
{
    if (range >= 0)
    {
        EStrRewrite ret = StrRewrite(pDevicesStatus[devIndex].epInfo[GetEPIndex(devIndex, epID)].repLast, epValue, range, STRSIZE_EPVALUE);
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

void CDriver::WriteEPData(int devIndex, uint8_t epID, const char *epValue, double range)
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

    return 0;
}

int CDriver::RecvCallback(TransferData *data, int type)
{
    switch (type)
    {
    case NOTIFY:
    {
        SRecvBuffer recvBuf = {0};
        if (data->param3 > BYTESIZE_TRANSFERDATA)
            recvBuf.dataLen = BYTESIZE_TRANSFERDATA;
        else
            recvBuf.dataLen = data->param3;
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

//写驱动状态
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
    if (pDevicesStatus[devIndex].isFault)
        StrNCpySafe(strStatus, DEVICE_FAULT, STRSIZE_STATUS);
    else
        StrNCpySafe(strStatus, DEVICE_USUAL, STRSIZE_STATUS);
    if (SAME != StrRewrite(pDevicesStatus[devIndex].strFault, strStatus, 0, STRSIZE_STATUS))
    {
        StrNCpySafe(devData.epData.EpValue, strStatus, EPVALUESIZE);
        devData.data_timestamp = GetTimeStamp();
        shmW.shmfifo_write(&devData, -1);
        usleep(INTERVAL_WRITESHM);
    }

    return 0;
}


//   检查网络状态
int CDriver::CheckNetworkStatus()
{
    if (false == tcp.isTCPConnected())     // 检查TCP连接状态
    {
        if (false == SpellIsSame(pCommParam->connectStatus, ""))
            WriteDriStatus(GATEWAY_OFFLINE);              //错误，写驱动信息进共享内存 ???
        tcp.Disconnect();
        int ret = tcp.ConnectToServer(pCommParam->ip, pCommParam->port, RecvCallback);
        if (ret < 3)
        {
            WriteDriStatus(GATEWAY_OFFLINE);
            sleep(4);
        }
        else
        {
            WriteDriStatus(GATEWAY_ONLINE);
            pCommParam->connectedTS = GetTimeStamp();
            for (int devIndex = 0; devIndex < pDevicesStatus->devNum; devIndex++)
            {
                memset(pDevicesStatus[devIndex].strOnline, 0x00, STRSIZE_STATUS);
                memset(pDevicesStatus[devIndex].strFault, 0x00, STRSIZE_STATUS);
                WriteDevStatus(devIndex);
            }
        }
    }
    else if (pDevicesStatus->devNum > 0)   //检查连接到设备状态
    {
        bool devAllOff = true;
        for (int devIndex = 0; devIndex < pDevicesStatus->devNum; devIndex++)
            if (pDevicesStatus[devIndex].isOnline)
            {
                devAllOff = false;
                break;
            }
        if (devAllOff && GetTimeStamp() > (pCommParam->connectedTS + DURATION_CONNECTED_VALIDITY))
        {
            tcp.Disconnect();
            int ret = tcp.ConnectToServer(pCommParam->ip, pCommParam->port, RecvCallback);
            if (ret < 3)
                WriteDriStatus(GATEWAY_OFFLINE);
            else
            {
                WriteDriStatus(GATEWAY_ONLINE);
                pCommParam->connectedTS = GetTimeStamp();  
            }
        }
    }

    return 0;
}

int CDriver::ExecuteOrder(SDevOrder *order)
{
    uint8_t sendData[BYTESIZE_TRANSFERDATA] = {0};
    size_t sendLen = 0;

    switch (pDevicesStatus[order->devIndex].devType)
    {
    case SERIES:                                                 
    {
        sendData[sendLen++] = 0xFA;                                    //0
        sendData[sendLen++] = 0xA2;                                    //1
        memcpy(sendData + sendLen, order->data, order->dataLen);      
        sendLen += order->dataLen;
        sendData[sendLen++] = 0xF5;
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

    order->state = COMPLETE;

    return 0;
}

int CDriver::WriteCtrlRsp(SDevOrder *order)
{
    int devIndex = order->devIndex;
    DevsData *devData = new DevsData;
    memset(devData, 0x00, sizeof(DevsData));
    StrNCpySafe(devData->DevId, pDevicesInfo[devIndex].DevId, sizeof(devData->DevId));
    StrNCpySafe(devData->uuid, order->orderID, sizeof(devData->uuid));
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
    case ONLINE:
    {
        pDevicesStatus[devIndex].isOnline = true;
        pDevicesStatus[devIndex].isFault = false;
    }
    break;
    case OFFLINE:
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


// 发送处理
void *CDriver::SendHandle(void *pParam)         
{
    CDriver *pDriver = (CDriver *)pParam;       
    SDevOrder *ctrlOrder = new SDevOrder;       

    pDriver->CheckNetworkStatus();    // 检查网络状态,怎么具体分析？

    while (true)
    {
        memset(ctrlOrder, 0x00, sizeof(SDevOrder));
        if (pDriver->sendList.ReadFromQueue(ctrlOrder))    //通过队列查询，若无指令返回false，若有指令，通过指针返回Ctrlorder？？？
        {
            switch (ctrlOrder->orderType)
            {
                case CTRL:
                {
                    int repeatExe = -1;
                    while (true)
                    {
                        pDriver->ExecuteOrder(ctrlOrder);      // 设备操作命令执行 执行完成state == COMPLETE  里面有问题？？？
                        repeatExe++;                           // 含义？？？
                        if (0 == repeatExe && -1 == pDriver->WriteCtrlRsp(ctrlOrder))    //检测响应？
                            printf(DRINAME "::WriteCtrlRsp(): Report control response error.\n");
                        if (COMPLETE == ctrlOrder->state || repeatExe >= ctrlOrder->repeat ||
                            GetTimeStamp() > (ctrlOrder->orderTS + ctrlOrder->orderTO))
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
        }
        else
        {
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
            pDriver->RecvParse(recvBuf);                        

        usleep(INTERVAL_VOID);    
    }

    delete recvBuf;
    recvBuf = NULL;

    return (void *)0;
}

int CDriver::HandleCfgData(CfgDevsData *cfgData)
{
    if (SpellIsSame(cfgData->dataType, CFG_autoSearch))    
    {
        char devType[STRSIZE_DEVTYPE] = {0};
        for (int paramIndex = 0; paramIndex < cfgData->paramNum; paramIndex++)
            if (SpellIsSame(cfgData->paramData[paramIndex].Id, "devType"))
            {
                StrNCpySafe(devType, cfgData->paramData[paramIndex].Value, STRSIZE_DEVTYPE);    
                break;
            }
        if (SpellIsSame(devType, "") || UNKNOW == DevTypeConv(devType))     
            return -1;

        char adapterName[IFNAMSIZ] = {0};
        if (0 == GetAdapterName(pCommParam->ip, adapterName))
        {
    
            CMLGWConfig rs485(adapterName, PORT_BASE + pDriverInfo->Id, pCommParam->ip);   
            if (0 == pDevicesStatus->devNum)
            {
                if (rs485.ConfigUART(38400, DATABIT_8, STOPBIT_1, CHK_NONE))      
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
        }
        else
            return -1;

        if (false == SpellIsSame(pCommParam->mac, ""))
        {
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