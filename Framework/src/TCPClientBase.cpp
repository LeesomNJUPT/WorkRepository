#include "../include/TCPClientBase.h"

CTCPClientBase::CTCPClientBase()
{
	// 初始化成员变量
	socketClient = -1;
	isConnected = false;
	memset(serverIP, 0, INET_ADDRSTRLEN);
	serverPort = 0;
	callbackHandle = NULL;
	recvThreadID = 0;
	handleThreadID = 0;
	memset(&recvThreadAttr, 0, sizeof(pthread_attr_t));
	memset(&handleThreadAttr, 0, sizeof(pthread_attr_t));
	isRecvRunning = false;
	isHandleRunning = false;
	pthread_mutex_init(&dataListMutex, NULL);
	// 创建处理线程
	isHandleRunning = true;
	pthread_attr_init(&handleThreadAttr);
	pthread_attr_setdetachstate(&handleThreadAttr, PTHREAD_CREATE_DETACHED);
	if (0 != pthread_create(&handleThreadID, &handleThreadAttr, HandleList, this))
		perror("CTCPClientBase()");
}

CTCPClientBase::~CTCPClientBase()
{
	Disconnect();
	// 处理线程停止
	isHandleRunning = false;
	if (handleThreadID > 0)
	{
		pthread_cancel(handleThreadID);
		handleThreadID = 0;
	}
	pthread_attr_destroy(&handleThreadAttr);
	DeleteList();
	pthread_mutex_destroy(&dataListMutex);
}

void *CTCPClientBase::Recieve(void *pParam)
{
	CTCPClientBase *pTCPClient = (CTCPClientBase *)pParam;
	char recvBuf[BUFFERSIZE];
	ssize_t recvLen;

	if (pTCPClient->socketClient < 3)
		pthread_exit(NULL);
	while (pTCPClient->isRecvRunning)
	{
		recvLen = recv(pTCPClient->socketClient, recvBuf, BUFFERSIZE, 0);
		recvBuf[recvLen] = '\0';
		if (recvLen > 0)
		{
#ifdef DEBUG_FLAG
			printf("TCP Recv:");
			for (size_t dataIndex = 0; dataIndex < recvLen; dataIndex++)
				printf(" %02X", (uint8_t)recvBuf[dataIndex]);
			printf("\n");
#endif
			if (pTCPClient->callbackHandle)
			{
				TransferData *pRecv = new TransferData;
				memset(pRecv, 0, sizeof(TransferData));
				// MsgType
				pRecv->MsgType = NOTIFY;
				// param1
				pRecv->param1 = DATA;
				// param2 = serverIP
				pRecv->param2 = new char[INET_ADDRSTRLEN];
				memset(pRecv->param2, 0, INET_ADDRSTRLEN);
				memcpy(pRecv->param2, pTCPClient->serverIP, INET_ADDRSTRLEN);
				// param3 = recvLen
				pRecv->param3 = recvLen;
				// Data = recvData
				pRecv->Data = new uint8_t[BUFFERSIZE];
				memset(pRecv->Data, 0, BUFFERSIZE);
				memcpy(pRecv->Data, recvBuf, recvLen);
				// pHandle = callbackHandle
				pRecv->pHandle = (long)pTCPClient->callbackHandle;
				pTCPClient->WriteToList(pRecv);
				delete pRecv;
				pRecv = NULL;
			}
			pTCPClient->isConnected = true;
		}
		else if (((recvLen < 0) &&
				  (errno != EINTR) &&
				  (errno != EAGAIN) &&
				  (errno != EWOULDBLOCK)) ||
				 (0 == recvLen))
		{
			if (pTCPClient->isConnected &&
				pTCPClient->callbackHandle)
			{
				// TransferData *pDis = new TransferData;
				// memset(pDis, 0, sizeof(TransferData));
				// pDis->MsgType = NETWORKSTATUS;
				// pDis->param1 = OFFLINE;
				// pDis->param2 = NULL;
				// pDis->param3 = sizeof("TCP Disconnected");
				// pDis->Data = new uint8_t[BUFFERSIZE];
				// memset(pDis->Data, 0, BUFFERSIZE);
				// memcpy(pDis->Data, "TCP Disconnected", sizeof("TCP Disconnected"));
				// pDis->pHandle = (long)pTCPClient->callbackHandle;
				// pTCPClient->WriteToList(pDis);
				// delete pDis;
				// pDis = NULL;
			}
			pTCPClient->isConnected = false;
		}
		pthread_testcancel();
		usleep(1000);
		pthread_testcancel();
	}

	return (void *)0;
}

void *CTCPClientBase::HandleList(void *pParam)
{
	CTCPClientBase *pTCPClient = (CTCPClientBase *)pParam;
	TransferData HandleData;
	TransferData *pHandleData = &HandleData;

	while (pTCPClient->isHandleRunning)
	{
		if (pTCPClient->ReadFromList(pHandleData))
		{
			// 分类处理
			switch (pHandleData->MsgType)
			{
			case NOTIFY:
			case NETWORKSTATUS:
				if (pHandleData->pHandle)
					pTCPClient->executeCallback((SocketCB)pHandleData->pHandle,
												pHandleData, pHandleData->MsgType);
				break;
			case WRITE:
				pTCPClient->WriteToServer(pHandleData->Data,
										  pHandleData->param3);
				break;
			}
			if (pHandleData->param2)
			{
				delete[] pHandleData->param2;
				pHandleData->param2 = NULL;
			}
			if (pHandleData->Data)
			{
				delete[] pHandleData->Data;
				pHandleData->Data = NULL;
			}
		}
		pthread_testcancel();
		usleep(1000);
		pthread_testcancel();
	}

	return (void *)0;
}

int CTCPClientBase::ConnectToServer(char *IP, int port, SocketCB callback)
{
	memset(serverIP, 0, INET_ADDRSTRLEN);
	strncpy(serverIP, IP, INET_ADDRSTRLEN);
	serverPort = (uint16_t)port;
	callbackHandle = callback;

	sockaddr_in remoteAddr;
	memset(&remoteAddr, 0, sizeof(sockaddr_in));
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_addr.s_addr = inet_addr(IP);
	remoteAddr.sin_port = htons(port);
	// 创建客户端套接字
	socketClient = socket(PF_INET, SOCK_STREAM, 0);
	if (socketClient < 3)
	{
		Disconnect();
		printf("ConnectToServer(): New socket error.\n");
		return -1;
	}
	// 设置套接字超时参数
	timeval tv;
	tv.tv_sec = 3; // 接收超时: 3秒
	tv.tv_usec = 0;
	setsockopt(socketClient, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(timeval));
	// 套接字绑定地址
	if (connect(socketClient, (sockaddr *)&remoteAddr, sizeof(sockaddr)) < 0)
	{
		Disconnect();
		// printf("ConnectToServer(): Connect error, error code %d.\n", errno);
		perror("ConnectToServer()");
		return -1;
	}
	isConnected = true;
	// 设置套接字保活参数
	if (-1 == SetKeepAlive(socketClient, 10, 3, 5))
	{
		Disconnect();
		printf("ConnectToServer(): Set keepalive error.\n");
		return -1;
	}
	isRecvRunning = true;
	pthread_attr_init(&recvThreadAttr);
	pthread_attr_setdetachstate(&recvThreadAttr, PTHREAD_CREATE_DETACHED);
	int ret = pthread_create(&recvThreadID, &recvThreadAttr, Recieve, this);
	if (ret != 0)
	{
		Disconnect();
		printf("ConnectToServer(): Create thread error.\n");
		return -1;
	}
	if (isConnected && callbackHandle)
	{
		// TransferData *pCon = new TransferData;
		// memset(pCon, 0, sizeof(TransferData));
		// pCon->MsgType = NETWORKSTATUS;
		// pCon->param1 = ONLINE;
		// pCon->param2 = NULL;
		// pCon->param3 = sizeof("TCP Connected");
		// pCon->Data = new uint8_t[BUFFERSIZE];
		// memset(pCon->Data, 0, BUFFERSIZE);
		// memcpy(pCon->Data, "TCP Connected", sizeof("TCP Connected"));
		// pCon->pHandle = (long)callbackHandle;
		// WriteToList(pCon);
		// delete pCon;
		// pCon = NULL;
	}

	return socketClient;
}

bool CTCPClientBase::Send(uint8_t *sendData, int sendLen)
{
	if (callbackHandle)
	{
		TransferData *pAdd = new TransferData;
		memset(pAdd, 0, sizeof(TransferData));
		// MsgType
		pAdd->MsgType = WRITE;
		// Data = sendData
		pAdd->Data = new uint8_t[BUFFERSIZE];
		memset(pAdd->Data, 0, BUFFERSIZE);
		memcpy(pAdd->Data, sendData, sendLen);
		// param1 = socket
		pAdd->param1 = socketClient;
		// param2
		pAdd->param2 = NULL;
		// param3 = sendLen
		pAdd->param3 = sendLen;
		// pHandlepCon
		pAdd->pHandle = NULL;
		WriteToList(pAdd);
		delete pAdd;
		pAdd = NULL;

		return true;
	}
	else
		return false;
}

int CTCPClientBase::WriteToServer(uint8_t *data, int dataLen)
{
	if (!isTCPConnected())
		return 0;

	uint8_t sendBuffer[BUFFERSIZE];
	memset(sendBuffer, 0, BUFFERSIZE);
	memcpy(sendBuffer, data, dataLen);
	uint8_t *pSend = sendBuffer; // 发送指针
	int byteLeft = dataLen;		 // 剩余待发送字节数
	int byteWritten;			 // 单次已发送字节数
	while (byteLeft > 0)
	{
#ifdef DEBUG_FLAG
		printf("TCP Send:");
		for (size_t dataIndex = 0; dataIndex < byteLeft; dataIndex++)
			printf(" %02X", (uint8_t)pSend[dataIndex]);
		printf("\n");
#endif
		byteWritten = send(socketClient, pSend, byteLeft, MSG_NOSIGNAL);
		if (!(byteWritten > 0))
		{
			if (EINTR == errno)
			{
				printf("WriteToServer(): Send error, error code is EINTR.\n");
				continue;
			}
			else if (EAGAIN == errno) /* EAGAIN : Resource temporarily unavailable*/
			{
				printf("WriteToServer(): Send error, error code is EAGAIN.\n");
				sleep(1); // 休眠一秒, 期望发送缓冲区得到释放.
				continue;
			}
			else
			{
				printf("WriteToServer(): Send error, "
					   "error code: %d, "
					   "error info: %s.\n",
					   errno, strerror(errno));
				return 0;
			}
		}
		byteLeft -= byteWritten;
		pSend += byteWritten;
	}

	return dataLen;
}

bool CTCPClientBase::Disconnect()
{
	// 接收线程停止
	isRecvRunning = false;
	if (recvThreadID > 0)
	{
		pthread_cancel(recvThreadID);
		recvThreadID = 0;
	}
	pthread_attr_destroy(&recvThreadAttr);
	// TCP连接断开
	if (socketClient > 2)
	{
		shutdown(socketClient, SHUT_RDWR);
		close(socketClient);
		socketClient = -1;
	}
	isConnected = false;
	sleep(1);

	return true;
}

bool CTCPClientBase::isTCPConnected()
{
	if (socketClient < 3)
		return false;
	if (false == isConnected)
		return false;
	tcp_info info;
	int infoLen = sizeof(info);
	getsockopt(socketClient, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&infoLen);
	if (TCP_ESTABLISHED != info.tcpi_state)
		return false;

	return true;
}

int CTCPClientBase::SetKeepAlive(int socket, int idle, int cnt, int intv)
{
	int alive = 1; // 保活机制启用
	if (0 != setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &alive, sizeof(alive)))
	{
		printf("SetKeepAlive(): Set keepalive fail.\n");
		return -1;
	}
	// 若idle秒后无数据, 则触发保活机制.
	if (0 != setsockopt(socket, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)))
	{
		printf("SetKeepAlive(): Set keepalive idle fail.\n");
		return -1;
	}
	// 若未收到回应, 则intv秒后重发保活包.
	if (0 != setsockopt(socket, SOL_TCP, TCP_KEEPINTVL, &intv, sizeof(intv)))
	{
		printf("SetKeepAlive(): Set keepalive intv fail.\n");
		return -1;
	}
	// 若连续cnt次未收到保活包, 则连接失效.
	if (0 != setsockopt(socket, SOL_TCP, TCP_KEEPCNT, &cnt, sizeof cnt))
	{
		printf("SetKeepAlive(): Set keepalive cnt fail.\n");
		return -1;
	}

	return 0;
}

int CTCPClientBase::executeCallback(SocketCB pCallback, TransferData *data, int type)
{
	return (*pCallback)(data, type);
}

int CTCPClientBase::GetSocketID()
{
	return socketClient;
}

bool CTCPClientBase::WriteToList(pTransferData pData)
{
	pthread_mutex_lock(&dataListMutex);

	if (dataList.size() > DATA_LIST_SIZE)
	{
		// 清空向量
		TransferData *pBuffer = NULL;
		std::vector<TransferData *>::iterator index;
		for (index = dataList.begin(); index != dataList.end(); index++)
		{
			pBuffer = (TransferData *)(*index);
			index = dataList.erase(index);
			index--;
			if (pBuffer)
			{
				// 释放结构体中指针指向空间
				if (pBuffer->param2)
				{
					delete[] pBuffer->param2;
					pBuffer->param2 = NULL;
				}
				if (pBuffer->Data)
				{
					delete[] pBuffer->Data;
					pBuffer->Data = NULL;
				}
				delete pBuffer;
				pBuffer = NULL;
			}
		}
		dataList.clear();
	}
	TransferData *pAddBuffer = NULL;
	pAddBuffer = new TransferData;
	if (pAddBuffer)
	{
		memcpy(pAddBuffer, pData, sizeof(TransferData));
		dataList.push_back(pAddBuffer);
	}

	pthread_mutex_unlock(&dataListMutex);
	return true;
}

bool CTCPClientBase::ReadFromList(pTransferData pData)
{
	pthread_mutex_lock(&dataListMutex);

	if (dataList.empty())
	{
		pthread_mutex_unlock(&dataListMutex);
		return false;
	}
	std::vector<TransferData *>::iterator index = dataList.begin();
	TransferData *pBuffer = (TransferData *)(*index);
	if (pBuffer != NULL)
	{
		memcpy(pData, pBuffer, sizeof(TransferData));
		dataList.erase(index);
		if (pBuffer)
		{
			delete pBuffer;
			pBuffer = NULL;
		}
		pthread_mutex_unlock(&dataListMutex);
		return true;
	}
	else
	{
		dataList.erase(index);
		pthread_mutex_unlock(&dataListMutex);
		return false;
	}
}

bool CTCPClientBase::DeleteList()
{
	pthread_mutex_lock(&dataListMutex);

	TransferData *pBuffer = NULL;
	std::vector<TransferData *>::iterator index;
	for (index = dataList.begin(); index != dataList.end(); index++)
	{
		pBuffer = (TransferData *)(*index);
		index = dataList.erase(index);
		index--;
		if (pBuffer)
		{
			// 释放结构体中指针指向空间
			if (pBuffer->param2)
			{
				delete[] pBuffer->param2;
				pBuffer->param2 = NULL;
			}
			if (pBuffer->Data)
			{
				delete[] pBuffer->Data;
				pBuffer->Data = NULL;
			}
			delete pBuffer;
			pBuffer = NULL;
		}
	}
	dataList.clear();

	pthread_mutex_unlock(&dataListMutex);
	return true;
}
