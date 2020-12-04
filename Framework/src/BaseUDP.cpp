#include "../include/BaseUDP.h"

CBaseUDP::CBaseUDP()
{
    fd = -1;
    pCallback = NULL;
    recvDataTID = 0;

    pthread_attr_t tAttr;
    pthread_attr_init(&tAttr);
    pthread_attr_setdetachstate(&tAttr, PTHREAD_CREATE_DETACHED);
    if (0 != pthread_create(&handleSendQueTID, &tAttr, HandleSendQue, this))
        perror("CBaseUDP()::pthread_create()::HandleSendQue()");
    if (0 != pthread_create(&handleRecvQueTID, &tAttr, HandleRecvQue, this))
        perror("CBaseUDP()::pthread_create()::HandleRecvQue()");
    pthread_attr_destroy(&tAttr);
}

CBaseUDP::~CBaseUDP()
{
    StopUDPRecv();
    if (0 < handleSendQueTID)
    {
        pthread_cancel(handleSendQueTID);
        handleSendQueTID = 0;
    }
    if (0 < handleRecvQueTID)
    {
        pthread_cancel(handleRecvQueTID);
        handleRecvQueTID = 0;
    }
}

int CBaseUDP::StopUDPRecv()
{
    if (0 < recvDataTID)
    {
        pthread_cancel(recvDataTID);
        recvDataTID = 0;
    }
    if (3 <= fd)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        fd = -1;
    }

    return 0;
}

int CBaseUDP::Init(int (*pArgCallback)(SUDPTransmit *), uint16_t port)
{
    StopUDPRecv();

    pCallback = pArgCallback;

    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 3)
    {
        StopUDPRecv();
        perror("Init()::socket()");
        return -1;
    }
    // 设置超时
    timeval tv;
    tv.tv_sec = 4;  // s
    tv.tv_usec = 0; // us
    if (0 != setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(timeval)))
        perror("Init()::setsockopt()::SO_SNDTIMEO");
    if (0 != setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval)))
        perror("Init()::setsockopt()::SO_RCVTIMEO");
    // 设置端口复用
    int setVal = true;
    if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &setVal, sizeof(int)))
        perror("Init()::setsockopt()::SO_REUSEADDR");
    // 设置广播
    if (0 != setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &setVal, sizeof(int)))
        perror("Init()::setsockopt()::SO_BROADCAST");
    if (0 != port)
    {
        sockaddr_in localAddr;
        memset(&localAddr, 0x00, sizeof(sockaddr_in));
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = htons(port);
        if (-1 == bind(fd, (sockaddr *)&localAddr, sizeof(sockaddr_in)))
        {
            StopUDPRecv();
            perror("Init()::bind()");
            return -1;
        }
    }
    // 创建接收线程
    pthread_attr_t tAttr;
    pthread_attr_init(&tAttr);
    pthread_attr_setdetachstate(&tAttr, PTHREAD_CREATE_DETACHED);
    int ret = pthread_create(&recvDataTID, &tAttr, RecvData, this);
    pthread_attr_destroy(&tAttr);
    if (0 != ret)
    {
        StopUDPRecv();
        perror("Init()::pthread_create()::RecvData()");
        return -1;
    }

    return fd;
}

int CBaseUDP::Send(const char *ip, uint16_t port, uint8_t *data, size_t dataLen)
{
    SUDPTransmit sendInfo;
    sendInfo.type = EUDPTransmitType::SEND;
    sendInfo.data = new uint8_t[dataLen];
    memcpy(sendInfo.data, data, dataLen);
    sendInfo.dataLen = dataLen;
    strncpy(sendInfo.destIP, ip, INET_ADDRSTRLEN);
    sendInfo.destPort = port;
    sendQue.WriteToQueue(&sendInfo);

    return 0;
}

size_t CBaseUDP::SendTo(SUDPTransmit *sendInfo)
{
    if (fd < 3)
        return 0;

    sockaddr_in destAddr;
    memset(&destAddr, 0x00, sizeof(sockaddr_in));
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(sendInfo->destIP);
    destAddr.sin_port = htons(sendInfo->destPort);

    uint8_t *sendHead = sendInfo->data;
    size_t byteLeft = sendInfo->dataLen;
    ssize_t sendRet;
    while (0 < byteLeft)
    {
        sendRet = sendto(fd, sendHead, byteLeft, 0, (sockaddr *)&destAddr, sizeof(sockaddr));
        if (-1 == sendRet)
        {
            if (EAGAIN == errno)
                sleep(1);
            else
                return 0;
        }
        else
        {
            byteLeft -= sendRet;
            sendHead += sendRet;
        }
    }

    return sendInfo->dataLen;
}

void *CBaseUDP::HandleSendQue(void *pParam)
{
    CBaseUDP *pUDP = (CBaseUDP *)pParam;
    SUDPTransmit sendInfo;

    while (true)
    {
        if (pUDP->sendQue.ReadFromQueue(&sendInfo))
        {
            pUDP->SendTo(&sendInfo);

            delete[] sendInfo.data;
            sendInfo.data = NULL;
        }
        usleep(1 * 1000);
        pthread_testcancel();
    }

    return (void *)0;
}

int CBaseUDP::ExecuteCallback(SUDPTransmit *recvInfo)
{
    if (NULL == pCallback)
        return -1;

    return (*pCallback)(recvInfo);
}

void *CBaseUDP::HandleRecvQue(void *pParam)
{
    CBaseUDP *pUDP = (CBaseUDP *)pParam;
    SUDPTransmit recvInfo;

    while (true)
    {
        if (pUDP->recvQue.ReadFromQueue(&recvInfo))
        {

            pUDP->ExecuteCallback(&recvInfo);

            delete[] recvInfo.data;
            recvInfo.data = NULL;
        }
        usleep(1 * 1000);
        pthread_testcancel();
    }

    return (void *)0;
}

int CBaseUDP::SetMulticast(const char *addr)
{
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(addr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (0 != setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)))
    {
        perror("SetMulticast()::setsockopt()");
        return -1;
    }

    return 0;
}

size_t CBaseUDP::RecvFrom()
{
    if (fd < 3)
        return 0;

    uint8_t recvBuf[64 * 1024];
    ssize_t recvLen;
    sockaddr_in srcAddr;
    socklen_t srcAddrLen = sizeof(sockaddr_in);

    recvLen = recvfrom(fd, recvBuf, 64 * 1024, 0, (sockaddr *)&srcAddr, &srcAddrLen);
    if (-1 == recvLen || 0 == recvLen)
        return 0;

    SUDPTransmit recvInfo;
    recvInfo.type = EUDPTransmitType::RECV;
    recvInfo.data = new uint8_t[recvLen];
    memcpy(recvInfo.data, recvBuf, recvLen);
    recvInfo.dataLen = recvLen;
    strncpy(recvInfo.srcIP, inet_ntoa(srcAddr.sin_addr), INET_ADDRSTRLEN);
    recvInfo.srcPort = ntohs(srcAddr.sin_port);
    recvQue.WriteToQueue(&recvInfo);

    return recvInfo.dataLen;
}

void *CBaseUDP::RecvData(void *pParam)
{
    CBaseUDP *pUDP = (CBaseUDP *)pParam;

    while (true)
    {
        pUDP->RecvFrom();
        usleep(1 * 1000);
        pthread_testcancel();
    }

    return (void *)0;
}