#include "../include/HttpsRequest.h"

const char HttpsPostHeaders[] = "Accept: */*\r\n"
								"User-Agent: Mozilla/4.0\r\n"
								"Cache-Control: no-cache\r\n"
								"Connection: Keep-Alive\r\n"
								"Accept-Language: cn\r\n"
								"Content-type: application/json;charset=utf-8\r\n";

const char HttpsGetHeaders[] = "Accept: */*\r\n"
							   "User-Agent: Mozilla/4.0\r\n"
							   "Cache-Control: no-cache\r\n"
							   "Connection: Keep-Alive\r\n"
							   "Accept-Language: cn\r\n"
							   "Content-type: application/json;charset=utf-8\r\n";

HttpsRequest::HttpsRequest()
{
}

HttpsRequest::~HttpsRequest()
{
}

char *HttpsRequest::GetIPbyName(const char *name)
{
	char *ret = new char[INET_ADDRSTRLEN];
	// char *service = new char[1024];
	memset(ret, 0, sizeof(char) * INET_ADDRSTRLEN);
	// memset(service, 0, sizeof(char) * 1024);

	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	hints.ai_protocol = 0;

	// in_addr addr;
	int err;

	if ((err = getaddrinfo(name, NULL, &hints, &res)) != 0)
	{
		printf("HTTPS::GetIPbyName()::getaddrinfo(): errorID: %d, %s.\n", err, gai_strerror(err));

		return NULL;
	}
	sprintf(ret, "%s", inet_ntoa(((sockaddr_in *)(res->ai_addr))->sin_addr));

	freeaddrinfo(res);
	res = NULL;

	return ret;
}

void HttpsRequest::FreeSocket(int fd)
{
	if (fd > 2)
	{
		shutdown(fd, SHUT_RDWR);
		close(fd);
		fd = -1;
	}
}

void HttpsRequest::FreeSSL(SSL *sslFd)
{
	if (NULL != sslFd)
	{
		SSL_shutdown(sslFd);
		SSL_free(sslFd);
		sslFd = NULL;
	}
}

/*
 * @Name 			- 创建TCP连接, 并建立到连接
 * @Parame *server 	- 字符串, 要连接的服务器地址, 可以为域名, 也可以为IP地址
 * @Parame 	port 	- 端口
 *
 * @return 			- 返回对应sock操作句柄, 用于控制后续通信
 */
int HttpsRequest::client_connect_tcp(char *server, int port)
{
	int sockfd;
	struct hostent *host = NULL;

	struct sockaddr_in cliaddr;
	char *svrAddr = NULL;
	char buf[1024];
	int ret = 0;
	svrAddr = new char[1024];
	memset(svrAddr, 0, sizeof(char) * 1024);
	memset(buf, 0, sizeof(buf));

	sprintf(svrAddr, "%s", server);

	// pthread_mutex_lock(&httpsGetHostMutex);
	if (!(host = gethostbyname(svrAddr)))
	{
		// pthread_mutex_unlock(&httpsGetHostMutex);
		printf("HTTPS::client_connect_tcp()::gethostbyname(): Get [%s]'s name error!\n", server);
		delete[] svrAddr;
		svrAddr = NULL;
		return -2;
	}
	// pthread_mutex_unlock(&httpsGetHostMutex);

	// for (int j = 0;; j++)
	// { /*循环 */
	// 	if (host->h_addr_list[j] != NULL)
	// 	{																					 /* 没有到达域名数组的结尾 */
	// 		printf("alias %d: %s\n", j, inet_ntoa(*(struct in_addr *)host->h_addr_list[j])); /* 打印域名 */
	// 	}
	// 	else
	// 	{ /*结尾 */
	// 		printf("\n");
	// 		break; /*退出循环 */
	// 	}
	// }

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("HTTPS::client_connect_tcp()::socket()");
		delete[] svrAddr;
		svrAddr = NULL;
		return -1;
	}

	// Time out
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0; // 1000 ms
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(struct timeval));

	// printf("HTTPS::client_connect_tcp(): server: %s, port: %d.\n", server, port);

	// addrinfo hints, *res;
	// // in_addr addr;
	// int err;

	// if ((err = getaddrinfo(svrAddr, NULL, NULL, &res)) != 0)
	// {
	// 	printf("error %d : %s\n", err, gai_strerror(err));
	// 	return 1;
	// }

	// if (gethostbyname_r(svrAddr, &host, buf, sizeof(char) * 1024, &phost, &ret))
	// {
	// 	printf("gethostbyname(%s) error!\n", server);
	// 	delete[] svrAddr;
	// 	svrAddr = NULL;
	// 	return -2;
	// }

	// if (ret > 0)
	// {
	// 	printf("gethostbyname(%s) error!\n", server);
	// 	delete[] svrAddr;
	// 	svrAddr = NULL;
	// 	return -2;
	// }

	bzero(&cliaddr, sizeof(struct sockaddr));
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(port);
	cliaddr.sin_addr = *((struct in_addr *)host->h_addr_list[0]);
	// cliaddr.sin_addr = *((struct in_addr *)phost->h_addr);
	//  cliaddr.sin_addr =  ((sockaddr_in*)(res->ai_addr))->sin_addr;
	// cliaddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));

	if (connect(sockfd, (struct sockaddr *)&cliaddr, sizeof(struct sockaddr)) < 0)
	{
		perror("HTTPS::client_connect_tcp()::connect()");
		delete[] svrAddr;
		svrAddr = NULL;

		// freeaddrinfo(res);
		FreeSocket(sockfd);

		return -3;
	}
	delete[] svrAddr;
	svrAddr = NULL;

	// freeaddrinfo(res);
	return (sockfd);
}

/*
 * @Name 			- 封装post数据包括headers
 * @parame *host 	- 主机地址, 域名
 * @parame  port 	- 端口号
 * @parame 	page 	- url相对路径
 * @parame 	len 	- 数据内容的长度
 * @parame 	content - 数据内容
 * @parame 	data 	- 得到封装的数据结果
 *
 * @return 	int 	- 返回封装得到的数据长度
 */
int HttpsRequest::post_pack(const char *host, int port, const char *page, int len, const char *content, char *data)
{
	int re_len = strlen(page) + strlen(host) + strlen(HttpsPostHeaders) + len + HTTP_HEADERS_MAXLEN;

	char *post = NULL;
	post = (char *)malloc(re_len);
	if (post == NULL)
	{
		return -1;
	}
	// printf("===== Page: %s =======\r\n", page);
	sprintf(post, "POST %s HTTP/1.1\r\n", page);
	sprintf(post, "%sHost: %s:%d\r\n", post, host, port);
	// sprintf(post, "%sHost: sslcm.morelinks.cn:80\r\n",post);
	sprintf(post, "%s%s", post, HttpsPostHeaders);
	sprintf(post, "%sContent-Length: %d\r\n\r\n", post, len);
	sprintf(post, "%s%s", post, content); // 此处需要修改, 当业务需要上传非字符串数据的时候, 会造成数据传输丢失或失败

	re_len = strlen(post);
	memset(data, 0, re_len + 1);
	memcpy(data, post, re_len);

	free(post);
	return re_len;
}

/*
 * @Name 			- 封装 get数据包括headers
 * @parame *host 	- 主机地址, 域名
 * @parame  port 	- 端口号
 * @parame 	page 	- url相对路径
 * @parame 	len 	- 数据内容的长度
 * @parame 	content - 数据内容
 * @parame 	data 	- 得到封装的数据结果
 *
 * @return 	int 	- 返回封装得到的数据长度
 */
int HttpsRequest::get_pack(const char *host, int port, const char *page, char *data)
{
	int re_len = strlen(page) + strlen(host) + strlen(HttpsGetHeaders) + HTTP_HEADERS_MAXLEN;

	char *post = NULL;
	post = (char *)malloc(re_len);
	if (post == NULL)
	{
		return -1;
	}

	sprintf(post, "GET %s HTTP/1.1\r\n", page);
	sprintf(post, "%sHost: %s:%d\r\n", post, host, port);
	sprintf(post, "%s%s", post, HttpsPostHeaders);

	re_len = strlen(post);
	memset(data, 0, re_len + 1);
	memcpy(data, post, re_len);

	free(post);
	return re_len;
}

/*
 * @Name 		- 	初始化SSL, 并且绑定sockfd到SSL
 * 					此作用主要目的是通过SSL来操作sock
 * 					
 * @return 		- 	返回已完成初始化并绑定对应sockfd的SSL指针
 */
SSL *HttpsRequest::ssl_init(int sockfd)
{
	int re = 0;
	SSL *ssl;
	SSL_CTX *ctx;

	SSL_library_init();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(SSLv23_client_method());
	if (ctx == NULL)
	{
		return NULL;
	}

	ssl = SSL_new(ctx);
	if (ssl == NULL)
	{
		return NULL;
	}

	/* 把socket和SSL关联 */
	re = SSL_set_fd(ssl, sockfd);
	if (re == 0)
	{
		SSL_free(ssl);
		return NULL;
	}

	/*
     * 经查阅, WIN32的系统下, 不能很有效的产生随机数, 此处增加随机数种子
     */
	RAND_poll();
	while (RAND_status() == 0)
	{
		unsigned short rand_ret = rand() % 65536;
		RAND_seed(&rand_ret, sizeof(rand_ret));
	}

	/*
     * ctx使用完成, 进行释放
     */
	SSL_CTX_free(ctx);

	return ssl;
}

/*
 * @Name 			- 通过SSL建立连接并发送数据
 * @Parame 	*ssl 	- SSL指针, 已经完成初始化并绑定了对应sock句柄的SSL指针
 * @Parame 	*data 	- 准备发送数据的指针地址
 * @Parame 	 size 	- 准备发送的数据长度
 *
 * @return 			- 返回发送完成的数据长度, 如果发送失败, 返回 -1
 */
int HttpsRequest::ssl_send(SSL *ssl, const char *data, int size)
{
	int re = 0;
	int count = 0;
	if (ssl == NULL)
	{
		return -3;
	}
	re = SSL_connect(ssl);
	// printf("SSL_connect:%d",re);
	if (re != 1)
	{
		return -1;
	}

	while (count < size)
	{
		re = SSL_write(ssl, data + count, size - count);
		if (re == -1)
		{
			return -2;
		}
		count += re;
	}

	return count;
}

/*
 * @Name 			- SSL接收数据, 需要已经建立连接
 * @Parame 	*ssl 	- SSL指针, 已经完成初始化并绑定了对应sock句柄的SSL指针
 * @Parame  *buff 	- 接收数据的缓冲区, 非空指针
 * @Parame 	 size 	- 准备接收的数据长度
 *
 * @return 			- 返回接收到的数据长度, 如果接收失败, 返回值 <0 
 */
int HttpsRequest::ssl_recv(SSL *ssl, char *buff, int size, char *errCode)
{
	int i = 0; // 读取数据取换行数量, 即判断headers是否结束
	// int headerIndex = 0;
	int re;
	int len = 0;
	char headers[HTTP_HEADERS_MAXLEN];
	char buf[2];

	if (ssl == NULL)
	{
		return -1;
	}

	memset(headers, 0, sizeof(char) * HTTP_HEADERS_MAXLEN);
	memset(buf, 0, sizeof(char) * 2);

	// Headers以换行结束, 此处判断头是否传输完成
	while ((len = SSL_read(ssl, buf, 1)) == 1)
	{
		if (i < 4)
		{
			if (buf[0] == '\r' || buf[0] == '\n')
			{
				i++;
				if (i >= 4)
				{
					break;
				}
			}
			else
			{
				i = 0;
			}
		}
		// printf("%c", headers[0]);		// 打印 Headers
		strcat(headers, buf);
		memset(buf, 0, sizeof(char) * 2);
	}

	// printf("\r\n%s\r\n", headers); // 打印Headers

	std::string key = "Transfer-Encoding";

	std::string headerString;
	headerString.assign(headers);
	// printf("************ Transfer-Encoding: %s\r\n", getHeader(headerString, key).c_str());
	if (errCode)
	{
		std::string errkey = "HTTP/1.1";
		strncpy(errCode, getHeader(headerString, errkey).c_str(), 64);

		// printf("************ HTTP/1.1 Error Code: %s\r\n", errCode);
	}

	len = 0;
	if (!strcasecmp(getHeader(headerString, key).c_str(), "chunked"))
	{
		len = getChunkedData(ssl, buff, READBUFSIZE);
	}
	else
	{
		len = getUnChunkedData(ssl, buff, READBUFSIZE);
	}

	// printf("read : %s\r\n", buff);
	return len;
}

int HttpsRequest::https_post(char *host, int port, char *url, const char *data, int dsize, char *buff, int bsize)
{
	SSL *ssl;
	int re = 0;
	int sockfd;
	int data_len = 0;
	int ssize = dsize + HTTP_HEADERS_MAXLEN; // 欲发送的数据包大小

	char *sdata = (char *)malloc(ssize);
	if (sdata == NULL)
	{
		return 0;
	}

	// 1、建立TCP连接
	// sockfd = client_connect_tcp(ip, port);
	sockfd = client_connect_tcp(host, port);

	if (sockfd < 0)
	{
		free(sdata);
		return 0;
	}

	// 2、SSL初始化, 关联Socket到SSL
	ssl = ssl_init(sockfd);
	if (ssl == NULL)
	{
		free(sdata);
		FreeSocket(sockfd);
		return 0;
	}

	// 3、组合POST数据
	data_len = post_pack(host, port, url, dsize, data, sdata);

	// 4、通过SSL发送数据

	re = ssl_send(ssl, sdata, data_len);
	// printf("\n+++++\n%s,%d,%d\n+++++\n",sdata,data_len,re);
	if (re < 0)
	{
		free(sdata);
		FreeSocket(sockfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
		return 0;
	}

	// 5、取回数据
	int r_len = 0;

	r_len = ssl_recv(ssl, buff, bsize);

	if (r_len < 0)
	{
		free(sdata);
		FreeSocket(sockfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
		return 0;
	}

	// 6、关闭会话, 释放内存
	free(sdata);
	FreeSocket(sockfd);
	SSL_shutdown(ssl);
	SSL_free(ssl);
	ssl = NULL;
	ERR_free_strings();

	return r_len;
}

string HttpsRequest::GetHostFromUrl(const char *url)
{
	string strUrl = url;

	size_t hostLeft = strUrl.find("https://");
	if (strUrl.npos == hostLeft)
		hostLeft = 0;
	else
		hostLeft = hostLeft + strlen("https://");
	strUrl = strUrl.substr(hostLeft);

	size_t hostRight = strUrl.find_first_of("/");
	if (strUrl.npos != hostRight)
		strUrl = strUrl.substr(0, hostRight);

	return strUrl;
}

string HttpsRequest::GetParamFromUrl(const char *url)
{
	string strUrl = url;

	size_t hostLeft = strUrl.find("https://");
	if (strUrl.npos == hostLeft)
		hostLeft = 0;
	else
		hostLeft = hostLeft + strlen("https://");
	strUrl = strUrl.substr(hostLeft);

	size_t hostRight = strUrl.find_first_of("/");
	if (strUrl.npos == hostRight)
		strUrl = "";
	else
		strUrl = strUrl.substr(hostRight);

	return strUrl;
}

string HttpsRequest::HttpsHeaderCreate(const char *method, const char *url, const char *data)
{
	string strHost = GetHostFromUrl(url);
	string strParam = GetParamFromUrl(url);
	string strMethod = method;

	string httpsHeader = strMethod + " " + strParam + " HTTP/1.1\r\n" +
						 "Host: " + strHost + "\r\n" +
						 "Accept: */*\r\n" +
						 "User-Agent: Mozilla/4.0\r\n" +
						 "Cache-Control: no-cache\r\n" +
						 "Accept-Encoding: deflate\r\n" +
						 "Connection: close\r\n";

	if ("POST" == strMethod)
	{
		httpsHeader += "Content-Type: application/json;charset=utf-8\r\n";
		httpsHeader += "Content-Length: " + std::to_string(strlen(data)) + "\r\n";
		httpsHeader += "\r\n";
		httpsHeader += data;
	}
	else if ("GET" == strMethod)
		httpsHeader += "\r\n";

	return httpsHeader;
}

uint16_t HttpsRequest::GetPortFromUrl(const char *url)
{
	string host = GetHostFromUrl(url);
	if (0 == host.length())
		return 0;

	uint16_t port = 0;
	size_t portIndex = host.find_first_of(':');
	if (host.npos == portIndex)
		port = 443;
	else
	{
		host = host.substr(portIndex + 1);
		port = stoi(host);
	}

	return port;
}

string HttpsRequest::GetIPFromUrl(const char *url)
{
	string host = GetHostFromUrl(url);
	string ip;

	size_t colonIndex = host.find_first_of(":");
	if (host.npos != colonIndex)
		ip = host.substr(0, colonIndex);
	else
	{
		char cBuf;
		uint8_t dotCount = 0;
		bool dnFlag = false;
		for (size_t hostIndex = 0; hostIndex < host.length(); hostIndex++)
		{
			cBuf = host[hostIndex];
			if ('.' == cBuf)
				dotCount++;
			else if (cBuf < '0' || '9' < cBuf)
			{
				dnFlag = true;
				break;
			}
		}
		if (dnFlag)
		{
			hostent *hostDetail = gethostbyname(host.c_str());
			if (NULL != hostDetail)
			{
				in_addr **addrList = (in_addr **)hostDetail->h_addr_list;
				for (size_t addrIndex = 0; addrList[addrIndex] != NULL; addrIndex++)
					ip = inet_ntoa(*addrList[addrIndex]);
			}
		}
		else if (3 == dotCount)
			ip = host;
	}

	return ip;
}

int HttpsRequest::ConnectToServer(const char *ip, uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("ConnectToServer()::socket()");
		return -1;
	}

	timeval tv;
	tv.tv_sec = 16;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval));

	sockaddr_in serAddr;
	bzero(&serAddr, sizeof(sockaddr_in));
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &serAddr.sin_addr);

	if (connect(fd, (sockaddr *)&serAddr, sizeof(sockaddr)) < 0)
	{
		perror("ConnectToServer()::connect()");
		FreeSocket(fd);
		return -1;
	}

	return fd;
}

size_t HttpsRequest::SSLSend(SSL *ssl, string sendStr)
{
	if (NULL == ssl)
		return 0;

	int ret = SSL_connect(ssl);
	if (1 != ret)
	{
		printf("SSLSend()::SSL_connect(): Connect return: %d.\n", ret);
		return 0;
	}

	size_t sendLen = sendStr.length();
	uint8_t *sendData = new uint8_t[sendLen];
	memcpy(sendData, sendStr.c_str(), sendLen);
	size_t sendCount = 0;
	while (sendCount < sendLen)
	{
		ret = SSL_write(ssl, sendData + sendCount, sendLen - sendCount);
		if (-1 == ret)
		{
			break;
		}
		else
		{
			sendCount += ret;
		}
	}
	delete[] sendData;
	sendData = NULL;

	return sendCount;
}

int HttpsRequest::GetHeaderValue(string strHeader, const char *key, string &strValue)
{
	string strKey = key;
	vector<string> rowList;
	string rowBuf;
	char cBuf;
	for (string::iterator cIt = strHeader.begin(); cIt != strHeader.end(); cIt++)
	{
		cBuf = *cIt;
		rowBuf += cBuf;
		if ('\n' == cBuf)
		{
			rowList.push_back(rowBuf);
			rowBuf.clear();
		}
	}
	size_t keyPos, tailPos;
	for (vector<string>::iterator rowIt = rowList.begin(); rowIt != rowList.end(); rowIt++)
	{
		keyPos = (*rowIt).find(strKey);
		tailPos = (*rowIt).find("\r\n");
		if ((*rowIt).npos != keyPos && (*rowIt).npos != tailPos)
		{
			strValue = (*rowIt).substr(keyPos + strKey.length() + strlen(": "), tailPos - keyPos - strKey.length() - strlen(": "));
			return 0;
		}
	}

	return -1;
}

int HttpsRequest::MemRealloc(uint8_t *&addr, size_t orgSize, size_t newSize)
{
	uint8_t *newAddr = new uint8_t[newSize];
	memset(newAddr, 0x00, newSize);
	if (NULL == addr)
		addr = newAddr;
	else
	{
		if (orgSize <= newSize)
			memcpy(newAddr, addr, orgSize);
		else
			memcpy(newAddr, addr, newSize);
		delete[] addr;
		addr = newAddr;
	}

	return 0;
}

size_t HttpsRequest::GetStreamData(SSL *ssl, uint8_t *&dataBuf)
{
	size_t totalSize = BYTESIZE_HTTPSBASE;
	dataBuf = new uint8_t[totalSize];
	size_t totalLen = 0;

	uint8_t recvBuf[BYTESIZE_HTTPSRECV];
	size_t recvLen;
	int ret;

	while (0 < (ret = SSL_read(ssl, recvBuf, BYTESIZE_HTTPSRECV)))
	{
		recvLen = ret;
		if (totalSize < (totalLen + recvLen))
		{
			MemRealloc(dataBuf, totalSize, totalSize * 2);
			totalSize *= 2;
		}
		memcpy(dataBuf + totalLen, recvBuf, recvLen);
		totalLen = totalLen + recvLen;
	}

	return totalLen;
}

uint8_t HttpsRequest::ChToHex(const char ch)
{
	uint8_t ret = 0x00;

	if ('0' <= ch && ch <= '9')
		ret = ch - 0x30;
	else if ('a' <= ch && ch <= 'f')
		ret = ch - 0x57;
	else if ('A' <= ch && ch <= 'F')
		ret = ch - 0x37;

	return ret;
}

size_t HttpsRequest::HexStrToSize(string strHex)
{
	if (0 == strHex.length())
		return 0;

	size_t hex = 0;
	size_t hexWeight = 1;
	for (size_t strIndex = 0; strIndex < strHex.length(); strIndex++)
	{
		hex += ChToHex(strHex[strHex.length() - 1 - strIndex]) * hexWeight;
		hexWeight = hexWeight << 4;
	}

	return hex;
}

size_t HttpsRequest::GetChunkedData(SSL *ssl, uint8_t *&dataBuf)
{
	uint8_t *encodeBuf = NULL;
	size_t encodeLen = GetStreamData(ssl, encodeBuf);

	size_t totalSize = BYTESIZE_HTTPSBASE;
	dataBuf = new uint8_t[totalSize];
	size_t totalLen = 0;

	uint8_t *pDecode = encodeBuf;
	string strDecodeLen;
	size_t decodeLen;
	EDecodeState decodeState = DECODELEN;
	while (pDecode < (encodeBuf + encodeLen))
	{
		switch (decodeState)
		{
		case DECODELEN:
		{
			switch (*pDecode)
			{
			case '\r':
				break;
			case '\n':
				decodeState = DECODEDATA;
				break;
			default:
				strDecodeLen += (char)(*pDecode);
				break;
			}
			pDecode++;
		}
		break;
		case DECODEDATA:
		{
			decodeLen = HexStrToSize(strDecodeLen);
			if ((encodeBuf + encodeLen) <= (pDecode + decodeLen - 1))
				decodeLen = encodeBuf + encodeLen - pDecode;
			while (totalSize < (totalLen + decodeLen))
			{
				MemRealloc(dataBuf, totalSize, totalSize * 2);
				totalSize *= 2;
			}
			memcpy(dataBuf + totalLen, pDecode, decodeLen);
			totalLen += decodeLen;
			pDecode += decodeLen;
			decodeState = DECODETAIL;
		}
		break;
		case DECODETAIL:
		{
			switch (*pDecode)
			{
			case '\n':
			{
				decodeState = DECODELEN;
				strDecodeLen = "";
			}
			break;
			default:
				break;
			}
			pDecode++;
		}
		break;
		default:
			pDecode++;
			break;
		}
	}
	delete[] encodeBuf;
	encodeBuf = NULL;

	return totalLen;
}

int HttpsRequest::SSLRecv(SSL *ssl, SHttpsRsp &rsp)
{
	if (NULL == ssl)
		return -1;

	// 获取Header
	string strHeaderBuf;
	char cBuf;
	uint8_t tailState = 0;
	while (4 != tailState)
	{
		if (1 != SSL_read(ssl, &cBuf, sizeof(char)))
			break;
		strHeaderBuf += cBuf;
		switch (cBuf)
		{
		case '\r':
			switch (tailState)
			{
			case 0:
			case 1:
			case 3:
				tailState = 1;
				break;
			case 2:
				tailState = 3;
				break;
			}
			break;
		case '\n':
			switch (tailState)
			{
			case 0:
			case 2:
				tailState = 0;
				break;
			case 1:
				tailState = 2;
				break;
			case 3:
				tailState = 4;
				break;
			}
			break;
		default:
			tailState = 0;
			break;
		}
	}
	// 解析Header
	uint8_t *dataBuf = NULL;
	size_t dataLen = 0;
	string strEncode;
	if (0 == GetHeaderValue(strHeaderBuf, "Transfer-Encoding", strEncode) && "chunked" == strEncode)
		dataLen = GetChunkedData(ssl, dataBuf);
	else
		dataLen = GetStreamData(ssl, dataBuf);

	// 传递结果
	rsp.header = new string;
	*rsp.header = strHeaderBuf;
	if (NULL != dataBuf && 0 < dataLen)
	{
		rsp.dataLen = dataLen;
		rsp.data = new uint8_t[dataLen];
		memcpy(rsp.data, dataBuf, dataLen);
	}
	delete[] dataBuf;
	dataBuf = NULL;

	return 0;
}

int HttpsRequest::HttpsPost(const char *url, const char *data, SHttpsRsp &rsp)
{
	if (NULL == url || 0 == strlen(url))
	{
		printf("HttpsPost(): Url can't be null.\n");
		return -1;
	}
	string header = HttpsHeaderCreate("POST", url, data);
	uint16_t port = GetPortFromUrl(url);
	if (0 == port)
	{
		printf("HttpsPost()::GetPortFromUrl(): Get port error.\n");
		return -1;
	}
	string ip = GetIPFromUrl(url);
	if (0 == ip.length())
	{
		printf("HttpsPost()::GetIPFromUrl(): Get ip error.\n");
		return -1;
	}

	int tcpFd;
	tcpFd = ConnectToServer(ip.c_str(), port);
	if (tcpFd < 0)
		return -1;
	// SSL初始化 Socket关联SSL
	SSL *sslFd;
	sslFd = ssl_init(tcpFd);
	if (NULL == sslFd)
	{
		FreeSocket(tcpFd);
		return -1;
	}
	// SSL发送
	size_t sendRet = SSLSend(sslFd, header);
	if (header.length() != sendRet)
	{
		FreeSSL(sslFd);
		FreeSocket(tcpFd);
		return -1;
	}

	// SSL接收
	int recvRet = SSLRecv(sslFd, rsp);
	FreeSSL(sslFd);
	FreeSocket(tcpFd);

	return recvRet;
}

int HttpsRequest::https_post(char *url, int port, const char *data, int dsize, char *buff, int bsize, char *errCode)
{
	SSL *ssl;
	char *host = NULL;
	int re = 0;
	int sockfd;
	int data_len = 0;
	int ssize = dsize + HTTP_HEADERS_MAXLEN; // 发送数据包大小
	char *sdata = (char *)malloc(ssize);

	if (sdata == NULL)
	{
		return 0;
	}

	host = GetHostFromURL(url);
	if (!host)
	{
		free(sdata);
		return 0;
	}

	// printf("host: %s\r\n", host);

	// 建立TCP连接
	sockfd = client_connect_tcp(host, 443);
	if (sockfd < 0)
	{
		free(sdata);
		free(host);
		return 0;
	}

	// SSL初始化 关联Socket到SSL
	ssl = ssl_init(sockfd);
	if (ssl == NULL)
	{
		free(sdata);
		free(host);
		// free(ipAddr);
		// ipAddr = NULL;
		FreeSocket(sockfd);
		return 0;
	}

	// 组合POST数据
	data_len = post_pack(host, 443, url + 8 + strlen(host), dsize, data, sdata);

	// 通过SSL发送数据
	re = ssl_send(ssl, sdata, data_len);
	if (re < 0)
	{
		free(sdata);
		free(host);
		FreeSocket(sockfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
		return 0;
	}

	// 取回数据
	int r_len = 0;

	r_len = ssl_recv(ssl, buff, bsize, errCode);

	if (r_len < 0)
	{
		free(sdata);
		free(host);
		FreeSocket(sockfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
		return 0;
	}
	// printf("host: %s\r\n", host);
	// 关闭会话 释放内存
	free(sdata);
	free(host);
	FreeSocket(sockfd);
	SSL_shutdown(ssl);
	SSL_free(ssl);
	ssl = NULL;
	ERR_free_strings();

	return r_len;
}

int HttpsRequest::https_get(char *host, char *ip, int port, char *url, char *buff, int bsize, char *errCode)
{
	SSL *ssl;
	int re = 0;
	int sockfd;
	int data_len = 0;
	int ssize = HTTP_HEADERS_MAXLEN; // 欲发送的数据包大小

	char *sdata = (char *)malloc(ssize);
	if (sdata == NULL)
	{
		return -1;
	}

	// 1、建立TCP连接
	sockfd = client_connect_tcp(ip, port);
	if (sockfd < 0)
	{
		free(sdata);
		return -2;
	}

	// 2、SSL初始化, 关联Socket到SSL
	ssl = ssl_init(sockfd);
	if (ssl == NULL)
	{
		free(sdata);
		close(sockfd);
		return -3;
	}

	// printf("%s\r\n", url);
	// 3、组合 GET 数据
	data_len = get_pack(host, port, url, sdata);

	// 4、通过SSL发送数据

	re = ssl_send(ssl, sdata, data_len);
	// printf("\n+++++\n%s,%d,%d\n+++++\n",sdata,data_len,re);
	if (re < 0)
	{
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
		return -4;
	}

	// 5、取回数据
	int r_len = 0;
	r_len = ssl_recv(ssl, buff, bsize, errCode);
	if (r_len < 0)
	{
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
		return -5;
	}

	// 6、关闭会话, 释放内存
	free(sdata);
	close(sockfd);
	SSL_shutdown(ssl);
	SSL_free(ssl);
	ssl = NULL;
	ERR_free_strings();

	return r_len;
}

char *HttpsRequest::GetTimeStamp()
{
	char *timeStamp = NULL;
	timeStamp = (char *)calloc(1, 32);
	struct timeval ts;
	gettimeofday(&ts, NULL);
	snprintf(timeStamp, 31, "%lu", (ts.tv_sec * 1000 + ts.tv_usec / 1000));
	// printf("\r\n%s\r\n",timeStamp);

	return timeStamp;
}

std::vector<std::string> HttpsRequest::split(const std::string &s, const std::string &seperator)
{
	std::vector<std::string> result;
	typedef std::string::size_type string_size;
	string_size i = 0;

	while (i != s.size())
	{
		// 找到字符串中首个不等于分隔符的字母
		int flag = 0;
		while (i != s.size() && flag == 0)
		{
			flag = 1;
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[i] == seperator[x])
				{
					++i;
					flag = 0;
					break;
				}
		}

		// 找到又一个分隔符，将两个分隔符之间的字符串取出
		flag = 0;
		string_size j = i;
		while (j != s.size() && flag == 0)
		{
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[j] == seperator[x])
				{
					flag = 1;
					break;
				}
			if (flag == 0)
				++j;
		}
		if (i != j)
		{
			result.push_back(s.substr(i, j - i));
			i = j;
		}
	}
	return result;
}

std::string HttpsRequest::getHeader(std::string respose, std::string key)
{
	std::vector<std::string> lines = split(respose, "\r\n");
	for (int i = 0; i < lines.size(); i++)
	{
		if (i == 0)
		{
			std::vector<std::string> line = split(lines[i], " "); // 注意空格
			if (line.size() >= 2 && line[0] == key)
			{
				return line[1];
			}
		}
		else
		{
			std::vector<std::string> line = split(lines[i], ": "); // 注意空格
			if (line.size() >= 2 && line[0] == key)
			{
				return line[1];
			}
		}
	}
	return "";
}

int HttpsRequest::getChunkedData(SSL *ssl, char *buff, int readsize)
{
	int len = 0;
	int resLen = 0;
	int r_len = 0;
	int err = 0;
	int index = 0;
	int blockSize = 0;
	char *stop = NULL;
	std::string readStr;
	unsigned char *readBuf = new unsigned char[BUFSIZE * 10];
	memset(readBuf, 0, sizeof(unsigned char) * BUFSIZE * 10);

	do
	{
		// r_len = ssl_recv(ssl,buff+len,bsize-len);
		r_len = SSL_read(ssl, readBuf + len, readsize);
		len += r_len;
		SSL_pending(ssl);
		err = SSL_get_error(ssl, r_len);
		// printf("\r\nread: %d, total: %d %s\r\n%s==========================", r_len, len, GetTimeStamp(),readBuf);
		// if (err  == SSL_ERROR_WANT_READ || err == SSL_ERROR_ZERO_RETURN || )

		// if ((readBuf[len - 1] == '\r' || readBuf[len - 1] == '\n') && (readBuf[len - 4] == '\r' || readBuf[len - 4] == '\n'))
		if ((readBuf[len - 1] == '\n') && (readBuf[len - 4] == '\r'))
		{
			// printf("readBuf + len - 3,2,1:%s,|%02x|,%s %s=====\r\n",readBuf+len - 5,readBuf+len - 5,readBuf+len - 1,GetTimeStamp());

			if (readBuf[len - 5] == 0x30)
			{
				// printf("num: %c\r\n", readBuf[len - 5]);
				break;
			}
		}

		if (err != SSL_ERROR_NONE)
		{
			break;
		}
	} while (true);

	strcat((char *)readBuf, "\0");
	// printf("readBuf: %s\r\n",readBuf);
	readStr.assign((char *)readBuf);

	std::vector<std::string> lines = split(readStr, "\r\n");
	for (int i = 0; i < lines.size(); i++)
	{
		// printf("index: %d, data: %s\r\n", i, lines[i].c_str());
		if (index % 2 == 1)
		{
			strcat(buff, lines[i].c_str());
		}

		index++;
	}

	// printf("readBuf: %s\r\n",buff);
	delete[] readBuf;
	readBuf = NULL;
	resLen = strlen(buff);
	return resLen;
}

/*
int HttpsRequest::getChunkedData(SSL *ssl, char *buff, int size)
{
	int len = 0;
	int r_len = 0;
	int err = 0;
	int readheadlen = 0;
	int  crindex = 0;
	bool isHeader = true;
	char headerBuf[2];
	char blockSizeStr[16];
    int  blockSize = 0;
	char *stop = NULL;

	do
	{
		// r_len = ssl_recv(ssl,buff+len,bsize-len);
		if (isHeader)
		{
			// 获取数据块大小
			crindex = 0;
			memset(blockSizeStr, 0, sizeof(char) * 16);
			memset(headerBuf, 0, sizeof(char) * 2);
			printf("\r\n ******** headerBuf: ");
			while ((readheadlen = SSL_read(ssl, headerBuf, 1)) == 1)
			{
				printf("%c", headerBuf[0]);		// 打印Headers
				if (headerBuf[0] == '\r' || headerBuf[0] == '\n')
				{
					memset(headerBuf, 0, sizeof(char) * 2);
					if(crindex == 0)
					{
						crindex++;
						continue;
					}
					else if(crindex > 1)
					{
						break;
					}
				}
				
				
				strcat(blockSizeStr, headerBuf);

				memset(headerBuf, 0, sizeof(char) * 2);

				SSL_pending(ssl);
				crindex++;
			}
		}


		  blockSize = strtol(blockSizeStr, &stop, 16);

		printf("\r\n******* Block Size: %d, %s\r\n",blockSize,blockSizeStr);
		if (blockSize == 0)
		{
			// 下一个数据块为 0, 退出接收
			break;
		}

		isHeader = false;

		r_len = SSL_read(ssl, buff + len, READBUFSIZE);

		len += r_len;

		if (r_len < READBUFSIZE)
		{
			// 已经收完一个数据块, 标记继续接收下一个数据块
			isHeader = true;
		}

		SSL_pending(ssl);
		err = SSL_get_error(ssl, r_len);
		printf("read: %d, total: %d %s ========================== \r\n", r_len, len, GetTimeStamp());
		// if (err  == SSL_ERROR_WANT_READ || err == SSL_ERROR_ZERO_RETURN || )

		if (err != SSL_ERROR_NONE)
		{
			break;
		}
	} while (true);

	return len;
}

*/

int HttpsRequest::getUnChunkedData(SSL *ssl, char *buff, int readsize)
{
	int len = 0;
	int r_len = 0;
	int err = 0;
	do
	{
		// r_len = ssl_recv(ssl,buff+len,bsize-len);
		r_len = SSL_read(ssl, buff + len, READBUFSIZE);
		len += r_len;
		SSL_pending(ssl);
		err = SSL_get_error(ssl, r_len);
		// printf("read: %d, total: %d %s ==========================", r_len, len, GetTimeStamp());
		// if (err  == SSL_ERROR_WANT_READ || err == SSL_ERROR_ZERO_RETURN || )
		if (err != SSL_ERROR_NONE)
		{
			break;
		}
	} while (true);

	return len;
}

char *HttpsRequest::GetHostFromURL(const char *strURL)
{
	char bufURL[URL_MAX_SIZE] = {0};
	strcpy(bufURL, strURL);

	char *strHandle = strstr(bufURL, "http://");
	if (strHandle != NULL)
		strHandle += 7;
	else
	{
		strHandle = strstr(bufURL, "https://");
		if (strHandle != NULL)
			strHandle += 8;
		else
			strHandle = bufURL;
	}

	int handleLen = strlen(strHandle);
	char *strHost = (char *)malloc(handleLen + 1);
	memset(strHost, 0, handleLen + 1);
	for (int handleIndex = 0; handleIndex < handleLen + 1; handleIndex++)
		if ('/' == strHandle[handleIndex])
			break;
		else
			strHost[handleIndex] = strHandle[handleIndex];

	return strHost;
}

bool HttpsRequest::GetIPFromURL(const char *strURL, char *returnIP)
{
	char *strHost = GetHostFromURL(strURL);
	int hostLen = strlen(strHost);
	char *strIP = (char *)malloc(hostLen + 1);
	memset(strIP, 0, hostLen + 1);
	int dotCount = 0;
	bool dnFlag = false;

	for (int hostIndex = 0; hostIndex < hostLen + 1; hostIndex++)
	{
		if (':' == strHost[hostIndex])
			break;
		strIP[hostIndex] = strHost[hostIndex];

		if ('.' == strHost[hostIndex])
		{
			dotCount++;
			continue;
		}
		if (dnFlag)
			continue;

		if ((strHost[hostIndex] < '0') ||
			('9' < strHost[hostIndex]))
			dnFlag = true;
	}
	free(strHost);

	if (strlen(strIP) < 1)
	{
		free(strIP);
		return false;
	}

	if ((3 == dotCount) && (!dnFlag))
	{
		strncpy(returnIP, strIP, INET_ADDRSTRLEN);
		returnIP[INET_ADDRSTRLEN - 1] = '\0';
		free(strIP);
		return true;
	}
	else
	{
		struct hostent *hostDetail = gethostbyname(strIP);
		free(strIP);
		if (NULL == hostDetail)
			return false;
		else
		{
			struct in_addr **addrList = (struct in_addr **)hostDetail->h_addr_list;
			for (int addrIndex = 0; addrList[addrIndex] != NULL; addrIndex++)
			{
				strncpy(returnIP, inet_ntoa(*addrList[addrIndex]), INET_ADDRSTRLEN);
				returnIP[INET_ADDRSTRLEN - 1] = '\0';
				return true;
			}
			return false;
		}
	}
}