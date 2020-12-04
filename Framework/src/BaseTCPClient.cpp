#include "../include/BaseTCPClient.h"

CBaseTCPClient::CBaseTCPClient()
{
	pthread_t recvDataTID, handleSendQueTID, handleRecvQueTID;

	fd = -1;
	memset(serverIP, 0x00, INET_ADDRSTRLEN);
	serverPort = 0;
	pCallback = NULL;
	recvDataTID = 0;

	pthread_attr_t tAttr;
	pthread_attr_init(&tAttr);
	pthread_attr_setdetachstate(&tAttr, PTHREAD_CREATE_DETACHED);
	if (0 != pthread_create(&handleSendQueTID, &tAttr, HandleSendQue, this))
		perror("CBaseTCPClient()::pthread_create()::HandleSendQue()");
	if (0 != pthread_create(&handleRecvQueTID, &tAttr, HandleRecvQue, this))
		perror("CBaseTCPClient()::pthread_create()::HandleRecvQue()");
	pthread_attr_destroy(&tAttr);
}

CBaseTCPClient::~CBaseTCPClient()
{
	Disconnect();
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

void *CBaseTCPClient::RecvData(void *pParam)
{
	CBaseTCPClient *pTCPClient = (CBaseTCPClient *)pParam;

	while (true)
	{
		pTCPClient->RecvFromServer();
		usleep(1 * 1000);
		pthread_testcancel();
	}

	return (void *)0;
}

size_t CBaseTCPClient::RecvFromServer()
{
	if (false == IsTCPConnected())
		return 0;

	uint8_t recvBuf[1024];
	ssize_t recvLen;
	recvLen = recv(fd, recvBuf, 1024, 0);
	if (0 == recvLen ||
		(-1 == recvLen && EINTR != errno && EAGAIN != errno && EWOULDBLOCK != errno))
	{
		STCPTransmit recvInfo;
		recvInfo.type = ETCPTransmitType::DISCONNECT;
		recvQue.WriteToQueue(&recvInfo);
		return 0;
	}
	else if (recvLen > 0)
	{
		STCPTransmit recvInfo;
		recvInfo.type = ETCPTransmitType::RECV;
		recvInfo.data = new uint8_t[recvLen];
		memcpy(recvInfo.data, recvBuf, recvLen);
		recvInfo.dataLen = recvLen;
		strncpy(recvInfo.srcIP, serverIP, INET_ADDRSTRLEN);
		recvInfo.srcPort = serverPort;
		recvQue.WriteToQueue(&recvInfo);

		return recvInfo.dataLen;
	}
}

void *CBaseTCPClient::HandleSendQue(void *pParam)
{
	CBaseTCPClient *pTCPClient = (CBaseTCPClient *)pParam;
	STCPTransmit sendInfo;

	while (true)
	{
		if (pTCPClient->sendQue.ReadFromQueue(&sendInfo))
		{
			pTCPClient->SendToServer(&sendInfo);

			delete[] sendInfo.data;
			sendInfo.data = NULL;
		}
		usleep(1 * 1000);
		pthread_testcancel();
	}

	return (void *)0;
}

void *CBaseTCPClient::HandleRecvQue(void *pParam)
{
	CBaseTCPClient *pTCPClient = (CBaseTCPClient *)pParam;
	STCPTransmit recvInfo;

	while (true)
	{
		if (pTCPClient->recvQue.ReadFromQueue(&recvInfo))
		{

			pTCPClient->ExecuteCallback(&recvInfo);

			delete[] recvInfo.data;
			recvInfo.data = NULL;
		}
		usleep(1 * 1000);
		pthread_testcancel();
	}

	return (void *)0;
}

int CBaseTCPClient::ConnectToServer(const char *ip, uint16_t port, int (*pArgCallback)(STCPTransmit *))
{
	strncpy(serverIP, ip, INET_ADDRSTRLEN);
	serverPort = port;
	pCallback = pArgCallback;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 3)
	{
		Disconnect();
		perror("ConnectToServer()::socket()");
		return -1;
	}
	// 设置超时
	timeval tv;
	tv.tv_sec = 4;	// s
	tv.tv_usec = 0; // us
	if (0 != setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(timeval)))
		perror("ConnectToServer()::setsockopt()::SO_SNDTIMEO");
	if (0 != setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval)))
		perror("ConnectToServer()::setsockopt()::SO_RCVTIMEO");
	// 设置端口复用
	int setVal = true;
	if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &setVal, sizeof(int)))
		perror("ConnectToServer()::setsockopt()::SO_REUSEADDR");
	// 设置保活参数
	if (-1 == SetKeepAlive(10, 4, 5))
	{
		Disconnect();
		return -1;
	}
	// 连接
	sockaddr_in remoteAddr = {0};
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_addr.s_addr = inet_addr(ip);
	remoteAddr.sin_port = htons(port);
	if (0 != connect(fd, (sockaddr *)&remoteAddr, sizeof(sockaddr)))
	{
		Disconnect();
		perror("ConnectToServer()::connect()");
		return -1;
	}
	// 创建接收线程
	pthread_attr_t tAttr;
	pthread_attr_init(&tAttr);
	pthread_attr_setdetachstate(&tAttr, PTHREAD_CREATE_DETACHED);
	int ret = pthread_create(&recvDataTID, &tAttr, RecvData, this);
	pthread_attr_destroy(&tAttr);
	if (0 != ret)
	{
		Disconnect();
		perror("ConnectToServer()::pthread_create()::RecvData()");
		return -1;
	}

	return fd;
}

int CBaseTCPClient::Send(uint8_t *data, size_t dataLen)
{
	STCPTransmit sendInfo;
	sendInfo.type = ETCPTransmitType::SEND;
	sendInfo.data = new uint8_t[dataLen];
	memcpy(sendInfo.data, data, dataLen);
	sendInfo.dataLen = dataLen;
	sendQue.WriteToQueue(&sendInfo);

	return 0;
}

size_t CBaseTCPClient::SendToServer(STCPTransmit *sendInfo)
{
	if (false == IsTCPConnected())
		return 0;

	uint8_t *sendHead = sendInfo->data;
	size_t byteLeft = sendInfo->dataLen;
	ssize_t sendRet;
	while (0 < byteLeft)
	{
		sendRet = send(fd, sendHead, byteLeft, 0);
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

int CBaseTCPClient::Disconnect()
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

bool CBaseTCPClient::IsTCPConnected()
{
	if (fd < 3)
		return false;
	tcp_info info;
	socklen_t infoLen = sizeof(info);
	getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &infoLen);
	if (TCP_ESTABLISHED == info.tcpi_state)
		return true;
	else
		return false;
}

int CBaseTCPClient::SetKeepAlive(int idle, int intvl, int cnt)
{
	// 保活机制使能
	int enable = true;
	if (0 != setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(int)))
	{
		perror("SetKeepAlive()::setsockopt()::SO_KEEPALIVE");
		return -1;
	}
	// 若idle秒内无数据 则触发保活机制
	if (0 != setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(int)))
	{
		perror("SetKeepAlive()::setsockopt()::TCP_KEEPIDLE");
		return -1;
	}
	// 若无响应 则intvl秒后重发保活包
	if (0 != setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &intvl, sizeof(int)))
	{
		perror("SetKeepAlive()::setsockopt()::TCP_KEEPINTVL");
		return -1;
	}
	// 若连续cnt次无响应 则连接失效
	if (0 != setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &cnt, sizeof(int)))
	{
		perror("SetKeepAlive()::setsockopt()::TCP_KEEPCNT");
		return -1;
	}

	return 0;
}

int CBaseTCPClient::ExecuteCallback(STCPTransmit *recvInfo)
{
	if (NULL == pCallback)
		return -1;

	return (*pCallback)(recvInfo);
}

int CBaseTCPClient::GetSocketID()
{
	return fd;
}