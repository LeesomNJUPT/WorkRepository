#include "../include/UDPBase.h"

// std::vector<TransferData *> UDPBase::udpdatalist;
// pthread_mutex_t UDPBase::udp_mutex;
// int UDPBase::server_sockfd;

UDPBase::UDPBase()
{
    pProcessListThreadId = 0;
    pRecvThreadId = 0;
    bThreadRunning = true;
    server_sockfd = -1;
    pNetEquipment = NULL;

    memset(&RecvThreadAttr, 0, sizeof(RecvThreadAttr));
    memset(&ProcessListThreadAttr, 0, sizeof(ProcessListThreadAttr));
    pthread_mutex_init(&udp_mutex, NULL);
}

UDPBase::~UDPBase()
{
    bThreadRunning = false;
    if (pRecvThreadId > 0)
    {
        pthread_cancel(pRecvThreadId);
        pthread_join(pRecvThreadId, NULL);
        pthread_attr_destroy(&RecvThreadAttr);
        pRecvThreadId = NULL;
        // LogTrace.TraceInfo("thread finished, :%s!\n", (char *)thread_res);
    }

    if (pProcessListThreadId > 0)
    {
        pthread_cancel(pProcessListThreadId);
        pthread_join(pProcessListThreadId, NULL);
        pthread_attr_destroy(&ProcessListThreadAttr);
        pProcessListThreadId = NULL;
        // LogTrace.TraceInfo("thread finished, :%s!\n", (char *)thread_res);
    }

    DeleteList();
    if (server_sockfd > -1)
        StopUDPServer(server_sockfd);

    if (pNetEquipment)
    {
        delete pNetEquipment;
        pNetEquipment = NULL;
    }

    pthread_mutex_destroy(&udp_mutex);
}

bool UDPBase::StopUDPServer(int nSocket_fd)
{
    bThreadRunning = false;

    if (pRecvThreadId > 0)
    {
        pthread_cancel(pRecvThreadId);
        pRecvThreadId = NULL;
        pthread_attr_destroy(&RecvThreadAttr);
    }

    if (pProcessListThreadId > 0)
    {
        pthread_cancel(pProcessListThreadId);
        pProcessListThreadId = NULL;
        pthread_attr_destroy(&ProcessListThreadAttr);
    }

    // if (nSocket_fd > -1)
    // {
    //     shutdown(nSocket_fd, SHUT_RDWR);
    //     close(nSocket_fd);
    //     nSocket_fd = -1;
    // }
    if (server_sockfd > -1)
    {
        shutdown(server_sockfd, SHUT_RDWR);
        close(server_sockfd);
        server_sockfd = -1;
    }

    if (pNetEquipment)
    {
        delete pNetEquipment;
        pNetEquipment = NULL;
    }

    sleep(1);
    return true;
}

int UDPBase::Initialize(const char *NetEquipment, int UDPPort, UDPCB nHandle)
{
    funHandle = nHandle;
    if (!NetEquipment)
        return false;

    strncpy(strNetEquipment, NetEquipment, 32);
    strNetEquipment[strlen(NetEquipment)] = '\0';

    if (pNetEquipment)
    {
        delete pNetEquipment;
        pNetEquipment = NULL;
    }
    pNetEquipment = Comm.GetNetwork(strNetEquipment);
    if (pNetEquipment)
    {
        // LogTrace.TraceInfo("IPv4: %s", pNetEquipment->IPv4);
        // LogTrace.TraceInfo("IPv6: %s", pNetEquipment->IPv6);
        // LogTrace.TraceInfo("MAC Address: ");
        for (int k = 0; k < 6; k++)
        {
            printf("%02x", pNetEquipment->macaddr[k]);

            if (k < 5)
                printf(":");
        }
        printf("\r\n");
        // LogTrace.TraceInfo("BroadCast: %s", pNetEquipment->BoradCastAddr);
    }
    else
    {
        // LogTrace.TraceError("Can not FOUND match Net Equipment!");
        return -1;
    }

    if ((server_sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        // LogTrace.TraceError("socket error");
        return -1;
    }

    // Time out
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 1000 ms
    setsockopt(server_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(struct timeval));

    /* 将套接字和IP、端口绑定 */
    struct sockaddr_in udp_addr_serv;
    int len;
    memset(&udp_addr_serv, 0, sizeof(struct sockaddr_in)); // a
    udp_addr_serv.sin_family = AF_INET;                    // AF_UNIX;//AF_INET;                    // 使用IPV4地址
    udp_addr_serv.sin_port = htons(UDPPort);               // UDP监听端口
    /* INADDR_ANY表示不管是哪个网卡接收到数据，只要目的端口是SERV_PORT，就会被该应用程序接收到 */
    udp_addr_serv.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr("192.254.1.26") ;//htonl(INADDR_ANY); //自动获取IP地址
    len = sizeof(udp_addr_serv);

    unsigned long addr = inet_addr(pNetEquipment->IPv4);
    // setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR | SO_BROADCAST, &addr, sizeof(addr));
    int broadcastEnable = true;
    setsockopt(server_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    /* 绑定socket */
    // if (bind(server_sockfd, (struct sockaddr *)&udp_addr_serv, sizeof(udp_addr_serv)) < 0)
    // {
    //     //  LogTrace.TraceError("UDP bind error");
    //     if (server_sockfd > -1)
    //     {
    //         shutdown(server_sockfd, SHUT_RDWR);
    //         close(server_sockfd);
    //         server_sockfd = -1;
    //     }
    //     return -1;
    // }

    bThreadRunning = true;

    pthread_attr_init(&RecvThreadAttr);
    pthread_attr_setdetachstate(&RecvThreadAttr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&pRecvThreadId, &RecvThreadAttr, RecvUDPThread, (void *)this);
    // LogTrace.TraceError("pRecvThreadId: %ld", pRecvThreadId);
    if (ret != 0)
    {
        if (pRecvThreadId > 0)
        {
            pthread_cancel(pRecvThreadId);
            pRecvThreadId = NULL;
            pthread_attr_destroy(&RecvThreadAttr);
            // LogTrace.TraceInfo("SocketBase thread finished, :%s, Socket ID:%d!\n", (char *)thread_res, nSocket_fd);
        }

        if (server_sockfd > -1)
        {
            shutdown(server_sockfd, SHUT_RDWR);
            close(server_sockfd);
            server_sockfd = -1;
        }

        // LogTrace.TraceError("create thread error");

        return -1;
    }

    pthread_attr_init(&ProcessListThreadAttr);
    pthread_attr_setdetachstate(&ProcessListThreadAttr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&pProcessListThreadId, &ProcessListThreadAttr, ProcessUDPList, (void *)this);

    if (ret != 0)
    {
        if (pProcessListThreadId > 0)
        {
            pthread_cancel(pProcessListThreadId);
            pProcessListThreadId = NULL;
            pthread_attr_destroy(&ProcessListThreadAttr);
            // LogTrace.TraceInfo("SocketBase thread finished, :%s, Socket ID:%d!\n", (char *)thread_res, nSocket_fd);
        }

        if (server_sockfd > -1)
        {
            shutdown(server_sockfd, SHUT_RDWR);
            close(server_sockfd);
            server_sockfd = -1;
        }

        // LogTrace.TraceError("create thread error");

        return -1;
    }
    else
    {
        if (udpdatalist.size() < MAXLIST && funHandle)
        {
            // TransferData *p = new TransferData;
            // memset(p, 0, sizeof(p));
            // p->MsgType = NETWORKSTATUS;
            // p->param1 = ONLINE;
            // p->param2 = NULL;
            // p->Data = new unsigned char[BUFFERSIZE];
            // memset(p->Data, 0, sizeof(p->Data));
            // memcpy(p->Data, (unsigned char *)"UDP Server Started", 18);
            // p->pHandle = (long int)funHandle;

            // AddToList(p);
        }

        // LogTrace.TraceInfo("UDPBase main thread running\n");

        return server_sockfd;
    }
}

void *UDPBase::RecvUDPThread(void *lpvParam)
{
    UDPBase *lpUDPBase = (UDPBase *)lpvParam;
    int len, socket_zise;
    char buf[BUFFERSIZE];
    struct sockaddr_in udp_addr_client;

    socket_zise = sizeof(udp_addr_client);

    if (lpUDPBase->server_sockfd < 0)
    {
        pthread_exit((void *)"exit RecvThread with error");
    }
    else
    {
        while (lpUDPBase->bThreadRunning)
        {

            len = recvfrom(lpUDPBase->server_sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&udp_addr_client, (socklen_t *)&socket_zise);
            // lpUDPBase->LogTrace.TraceInfo("UDP RecvThread Socket FD: %d", lpUDPBase->server_sockfd);
            buf[len] = 0;
            if (len > 0)
            {
                if (lpUDPBase->udpdatalist.size() < MAXLIST && lpUDPBase->funHandle)
                {

                    TransferData *p = new TransferData;
                    memset(p, 0, sizeof(p));
                    p->MsgType = NOTIFY;
                    p->param1 = ntohs(udp_addr_client.sin_port);
                    p->param2 = new char[1024];
                    p->param3 = len;
                    memset(p->param2, 0, sizeof(p->param2));
                    strcpy((char *)p->param2, inet_ntoa(udp_addr_client.sin_addr));
                    p->Data = new unsigned char[BUFFERSIZE];
                    memset(p->Data, 0, sizeof(p->Data));
                    memcpy(p->Data, buf, p->param3);
                    p->pHandle = (long int)lpUDPBase->funHandle;
                    lpUDPBase->AddToList(p);
                }
            }
            else
            { // disconnected from server

                if (lpUDPBase->udpdatalist.size() < MAXLIST && lpUDPBase->funHandle)
                {
                    // TransferData *p = new TransferData;
                    // memset(p, 0, sizeof(p));
                    // p->MsgType = NETWORKSTATUS;
                    // p->param2 = NULL;
                    // p->Data = new unsigned char[BUFFERSIZE];
                    // memset(p->Data, 0, sizeof(p->Data));
                    // // strcpy(p->Data, "TCP disconnected!");
                    // memcpy(p->Data, (unsigned char *)"UDP disconnected!", 17);
                    // p->param1 = OFFLINE;
                    // p->pHandle = (long int)lpUDPBase->funHandle;

                    // lpUDPBase->AddToList(p);
                }

                // printf("receive nothing\n");
            }

            if (lpUDPBase->server_sockfd < 0)
                printf("server_sockfd: %d\r\n", lpUDPBase->server_sockfd);

            usleep(1000);
        }

        printf("exit RecvThread code 0\r\n");
        pthread_exit((void *)"exit RecvThread code 0");
    }
}

void *UDPBase::ProcessUDPList(void *lpvParam)
{

    UDPBase *lpUDPBasePL = (UDPBase *)lpvParam;

    while (lpUDPBasePL->bThreadRunning)
    {

        if (!lpUDPBasePL->udpdatalist.empty())
        {

            TransferData p;
            if (lpUDPBasePL->ReadFromList(&p))
            {
                switch (p.MsgType)
                {
                case NOTIFY:
                    if (p.pHandle)
                    {
                        lpUDPBasePL->executeCallback((UDPCB)p.pHandle, &p, p.MsgType);
                        // printf("executeCallback: %s\n", p.Data);
                    }
                    break;
                case NETWORKSTATUS:
                    if (p.pHandle)
                    {
                        lpUDPBasePL->executeCallback((UDPCB)p.pHandle, &p, p.MsgType);
                    }
                    break;
                case WRITE:
                    if (p.Data)
                    {
                        if (!strcasecmp((char *)p.param2, BROADCAST))
                        {
                            if (lpUDPBasePL->server_sockfd == -1)
                            {
                                // lpUDPBasePL->LogTrace.TraceInfo("UDP Send Server Socket FD: %d, MAC: %02x, %ld", lpUDPBasePL->server_sockfd, p.Data[8], lpUDPBasePL->pRecvThreadId);
                            }
                            // if (p.Data[2] == 0x01)
                            // {
                            /* code */
                            lpUDPBasePL->SendUDPBroadCast(lpUDPBasePL->server_sockfd, p.param1, p.Data, p.param3);
                            // }

                            // lpUDPBase->SendUDPBroadCast(lpUDPBase->server_sockfd, p.param1, p.Data, p.param3);
                            // lpUDPBase->SendUDPBroadCast(4, p.param1, p.Data, p.param3);
                        }
                        else
                        {
                            lpUDPBasePL->SendUDPData(lpUDPBasePL->server_sockfd, (char *)p.param2, p.param1, p.Data, p.param3);
                        }
                    }
                    break;
                }

                if (p.Data)
                    delete[] p.Data;

                if (p.param2)
                    delete[] p.param2;
            }
        }
        usleep(1000);
        // sleep(1);
    }

    pthread_exit((void *)"exit ProcessList code 0");
}

bool UDPBase::PushToWriteList(char *mIP, int nPort, unsigned char *SendData, int Len)
{
    if (funHandle)
    {
        if (udpdatalist.size() < MAXLIST)
        {
            TransferData *p = new TransferData;
            p->MsgType = WRITE;
            p->Data = new unsigned char[BUFFERSIZE];
            memset(p->Data, 0, sizeof(p->Data));
            memcpy(p->Data, SendData, Len);
            p->param1 = nPort;
            p->param2 = new char[strlen(mIP) + 1];
            memset(p->param2, 0, strlen(mIP) + 1);
            memcpy(p->param2, mIP, strlen(mIP));
            p->param3 = Len;
            p->pHandle = NULL;

            AddToList(p);

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

int UDPBase::SendUDPData(int Server_FD, char *mIP, int nPort, unsigned char *mData, int nLen)
{
    /* socket文件描述符 */

    if (Server_FD < 0)
    {
        // LogTrace.TraceError("UDP send error,SocketId:%d, Server Socket Closed", Server_FD);
        return 0;
    }
    /* 设置address */
    struct sockaddr_in addr_recv;
    int len;
    memset(&addr_recv, 0, sizeof(addr_recv));
    addr_recv.sin_family = AF_INET;
    addr_recv.sin_addr.s_addr = inet_addr(mIP);
    addr_recv.sin_port = htons(nPort);
    len = sizeof(addr_recv);

    char send_buf[BUFFERSIZE];
    memset(send_buf, 0, sizeof(send_buf));

    memcpy(send_buf, mData, nLen);

    int send_num = sendto(Server_FD, send_buf, nLen, 0, (struct sockaddr *)&addr_recv, len);

    if (send_num < 0)
    {
        // close(sock_fd);
        // printf("UDP send error.\n");
        return 0;
    }

    // LogTrace.TraceInfo("UDP Send OK, SocketId:%d, IP:%s, Port:%d, Send Data Number:%d byte, Data:%s\n", Server_FD, mIP, nPort, send_num, send_buf);

    // close(sock_fd);

    return send_num;
}

int UDPBase::SendUDPBroadCast(int Server_FD, int nPort, unsigned char *mData, int nLen)
{
    /* socket文件描述符 */
    if (Server_FD < 0)
    {
        // LogTrace.TraceError("UDP send BroadCast error,SocketId:%d, Server Socket Closed", Server_FD);
        // printf("**********\n"
        //        "SendUDPBroadCast()::Socket:%d\n"
        //        "**********\n",
        //        Server_FD);
        return 0;
    }

    /* 设置address */
    struct sockaddr_in addr_broadcast;
    int len;
    memset(&addr_broadcast, 0, sizeof(addr_broadcast));
    addr_broadcast.sin_family = AF_INET;                                      //AF_UNIX;//AF_INET;
    addr_broadcast.sin_addr.s_addr = inet_addr(pNetEquipment->BoradCastAddr); //inet_addr("255.255.255.255");//inet_addr(pNetEquipment->BoradCastAddr); //htonl(INADDR_BROADCAST);//inet_addr(BROADCAST);
    addr_broadcast.sin_port = htons(nPort);
    // addr_broadcast
    len = sizeof(addr_broadcast);

    char send_buf[BUFFERSIZE];
    memset(send_buf, 0, sizeof(send_buf));

    memcpy(send_buf, mData, nLen);

    int send_num = sendto(Server_FD, send_buf, nLen, 0, (struct sockaddr *)&addr_broadcast, len);

    if (send_num < 0)
    {
        // printf("**********\n"
        //        "SendUDPBroadCast()::sendLen:%d\n"
        //        "**********\n",
        //        send_num);
        // close(sock_fd);
        // LogTrace.TraceError("UDP send BroadCast error,SocketId:%d, send_num:%d, Data:%s\n", Server_FD, send_num, send_buf);
        return 0;
    }

    // LogTrace.TraceInfo("UDP Send Broadcast OK, SocketId:%d, IP:%s, Port:%d, Send Data Number:%d byte, Data:%s\n", Server_FD, BROADCAST, nPort, send_num, send_buf);

    // if (broadcast_sockfd > -1)
    // {
    //     shutdown(broadcast_sockfd, SHUT_RDWR);
    //     close(broadcast_sockfd);
    //     broadcast_sockfd = -1;
    // }
    return send_num;
}

int UDPBase::executeCallback(UDPCB p, TransferData *data, int type)
{
    // printf("%s\n",data->Data);
    return (*p)(data, type);
}

bool UDPBase::AddToList(pTransferData pINFO)
{
    pthread_mutex_lock(&udp_mutex);
    if (udpdatalist.size() > MAXLIST)
    {

        TransferData *pBuf = NULL;
        std::vector<TransferData *>::iterator it;

        for (it = udpdatalist.begin(); it != udpdatalist.end(); it++)
        {
            pBuf = static_cast<TransferData *>(*it);
            it = udpdatalist.erase(it);
            it--;
            if (pBuf)
            {
                // if (pBuf->Data)
                // {
                //     delete[] pBuf->Data;
                //     pBuf->Data = NULL;
                // }

                // if (pBuf->param2)
                // {
                //     delete pBuf->param2;
                //     pBuf->param2 = NULL;
                // }

                free(pBuf);
                pBuf = NULL;
            }
        }
        udpdatalist.clear();
    }
    TransferData *pAddBuf = NULL;

    pAddBuf = (TransferData *)malloc(sizeof(TransferData));
    if (pAddBuf)
    {
        memcpy(pAddBuf, pINFO, sizeof(TransferData));
        // if (pINFO->Data)
        // {
        //     delete[] pINFO->Data;
        //     pINFO->Data = NULL;
        // }

        // if (pINFO->param2)
        // {
        //     delete pINFO->param2;
        //     pINFO->param2 = NULL;
        // }

        delete[] pINFO;
        udpdatalist.push_back(pAddBuf);
    }

    pthread_mutex_unlock(&udp_mutex);
    return true;
}

bool UDPBase::ReadFromList(pTransferData pINFO)
{
    pthread_mutex_lock(&udp_mutex);
    TransferData *pBuf = NULL;

    if (udpdatalist.empty())
    {
        pthread_mutex_unlock(&udp_mutex);
        return false;
    }
    //
    std::vector<TransferData *>::iterator it = udpdatalist.begin();
    pBuf = static_cast<TransferData *>(*it);
    if (pBuf != NULL)
    {
        memcpy(pINFO, pBuf, sizeof(TransferData));
        udpdatalist.erase(it);
        if (pBuf)
        {
            // if (pBuf->Data)
            // {
            //     delete[] pBuf->Data;
            //     pBuf->Data = NULL;
            // }

            // if (pBuf->param2)
            // {
            //     delete pBuf->param2;
            //     pBuf->param2 = NULL;
            // }

            free(pBuf);
            pBuf = NULL;
        }
        pthread_mutex_unlock(&udp_mutex);
        return true;
    }

    pthread_mutex_unlock(&udp_mutex);
    return true;
}

bool UDPBase::DeleteList()
{
    pthread_mutex_lock(&udp_mutex);
    TransferData *pBuf = NULL;
    std::vector<TransferData *>::iterator it;
    // yx  2010-05-06 delete
    for (it = udpdatalist.begin(); it != udpdatalist.end(); it++)
    {
        pBuf = static_cast<TransferData *>(*it);
        it = udpdatalist.erase(it);
        it--;
        if (pBuf)
        {
            // if (pBuf->Data)
            // {
            //     delete[] pBuf->Data;
            //     pBuf->Data = NULL;
            // }

            // if (pBuf->param2)
            // {
            //     delete pBuf->param2;
            //     pBuf->param2 = NULL;
            // }

            free(pBuf);
            pBuf = NULL;
        }
    }
    udpdatalist.clear();
    pthread_mutex_unlock(&udp_mutex);
    return true;
}
