#include "../include/HttpRequest.h"

HttpRequest::HttpRequest()
{
}

HttpRequest::~HttpRequest()
{
}

int HttpRequest::HttpGet(const char *strURL, SHttpRsp &rsp)
{
    return HttpRequestExec("GET", strURL, NULL, rsp);
}

int HttpRequest::HttpPost(const char *strURL, const char *strData, SHttpRsp &rsp)
{
    return HttpRequestExec("POST", strURL, strData, rsp);
}

int HttpRequest::HttpRequestExec(const char *strMethod, const char *strURL, const char *strData, SHttpRsp &rsp)
{
    if (NULL == strURL || 0 == strlen(strURL))
    {
        printf("HTTPRequestExec()::URL can't be null.\n");
        return -1;
    }
    if (STRSIZE_URL < strlen(strURL))
    {
        printf("HTTPRequestExec()::URL can't be longer than %d.\n", STRSIZE_URL);
        return -1;
    }
    string header = HttpHeaderCreate(strMethod, strURL, strData);
    int httpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (httpSocket < 3)
    {
        perror("HTTPRequestExec()::socket()");
        return -1;
    }
    uint16_t port = GetPortFromURL(strURL);
    if (port == 0)
    {
        printf("HTTPRequestExec()::GetPortFromURL(): Get port error.\n");
        InvalidSocket(httpSocket);
        return -1;
    }
    string ip = GetIPFromURL(strURL);
    if (0 == ip.length())
    {
        printf("HTTPRequestExec()::GetIPFromURL(): Get ip error.\n");
        InvalidSocket(httpSocket);
        return -1;
    }
    sockaddr_in servAddr;
    bzero(&servAddr, sizeof(sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    int ret = inet_pton(AF_INET, ip.c_str(), &servAddr.sin_addr);
    if (ret <= 0)
    {
        printf("HTTPRequestExec(): inet_pton() error.\n");
        InvalidSocket(httpSocket);
        return -1;
    }
    // set recv timeout
    timeval tv = {0};
    tv.tv_sec = 10;
    tv.tv_usec = 0; // 1000 ms
    ret = setsockopt(httpSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval));
    if (0 != ret)
    {
        perror("HTTPRequestExec()::setsockopt()");
        InvalidSocket(httpSocket);
        return -1;
    }
    ret = connect(httpSocket, (sockaddr *)&servAddr, sizeof(sockaddr));
    if (ret != 0)
    {
        perror("HTTPRequestExec()::connect()");
        InvalidSocket(httpSocket);
        return -1;
    }
    ret = HttpDataTransmit(httpSocket, header, rsp);

    InvalidSocket(httpSocket);
    return ret;
}

string HttpRequest::HttpHeaderCreate(const char *strMethod, const char *strURL, const char *strData)
{
    string host = GetHostFromURL(strURL);
    string param = GetParamFromURL(strURL);
    string method = strMethod;

    string httpHeader;

    httpHeader = method + " " + param + " HTTP/1.1\r\n" +
                 "User-Agent: Mozilla/4.0\r\n" +
                 "Accept: */*\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Host: " + host + "\r\n" +
                 "Accept-Encoding: deflate\r\n" +
                 "Connection: close\r\n";

    if ("POST" == method)
    {
        httpHeader += "Content-Type: application/json; charset=utf-8\r\n";
        httpHeader += "Content-Length: " + to_string(strlen(strData)) + "\r\n";
        httpHeader += "\r\n";
        httpHeader += strData;
    }
    else if ("GET" == method)
        httpHeader += "\r\n";

    return httpHeader;
}

int HttpRequest::MemRealloc(uint8_t *&addr, size_t orgSize, size_t newSize)
{
    uint8_t *newAddr = new uint8_t[newSize];
    memset(newAddr, 0, newSize);
    if (NULL == addr)
        addr = newAddr;
    else
    {
        if (newSize >= orgSize)
            memcpy(newAddr, addr, orgSize);
        else
            memcpy(newAddr, addr, newSize);
        delete[] addr;
        addr = newAddr;
    }

    return 0;
}

int HttpRequest::RecvDataParse(uint8_t *data, size_t dataLen, SHttpRsp &rsp)
{
    uint8_t endState = 0;
    size_t endIndex = 0;

    char cBuf = 0x00;
    for (size_t dataIndex = 0; dataIndex < dataLen; dataIndex++)
    {
        cBuf = data[dataIndex];
        switch (cBuf)
        {
        case '\r':
            switch (endState)
            {
            case 0:
            case 1:
            case 3:
                endState = 1;
                break;
            case 2:
                endState = 3;
                break;
            }
            break;
        case '\n':
            switch (endState)
            {
            case 0:
            case 2:
                endState = 0;
                break;
            case 1:
                endState = 2;
                break;
            case 3:
                endState = 4;
                break;
            }
            break;
        default:
            endState = 0;
            break;
        }
        if (4 == endState)
        {
            endIndex = dataIndex;
            break;
        }
    }
    if (0 == endIndex)
        return -1;
    size_t headerLen = endIndex + 1;
    rsp.header = new string;
    rsp.header->assign((char *)data, headerLen);
    rsp.dataLen = dataLen - headerLen;
    rsp.data = new uint8_t[rsp.dataLen];
    memcpy(rsp.data, &data[headerLen], rsp.dataLen);

    return 0;
}

int HttpRequest::HttpDataTransmit(int socket, string header, SHttpRsp &rsp)
{
    ssize_t ret = send(socket, header.c_str(), header.length(), 0);
    if (ret < 0)
    {
        perror("HttpDataTransmit()::send()");
        return -1;
    }

    uint8_t *totalBuf = new uint8_t[BYTESIZE_HTTPBASE];
    memset(totalBuf, 0, BYTESIZE_HTTPBASE);
    size_t totalSize = BYTESIZE_HTTPBASE; // size
    size_t totalLen = 0;                  // length
    char recvBuf[BYTESIZE_HTTPRECV] = {0};
    size_t recvLen = 0;
    while (true)
    {
        memset(recvBuf, 0x00, BYTESIZE_HTTPRECV);
        ret = recv(socket, recvBuf, BYTESIZE_HTTPRECV, 0);
        if (0 == ret)
        {
            ret = RecvDataParse(totalBuf, totalLen, rsp);
            delete[] totalBuf;
            totalBuf = NULL;
            return ret;
        }
        else if (ret > 0)
        {
            recvLen = ret;
            if ((totalLen + recvLen) >= totalSize)
            {
                MemRealloc(totalBuf, totalSize, totalSize + BYTESIZE_HTTPBASE);
                totalSize = totalSize + BYTESIZE_HTTPBASE;
            }
            memcpy(totalBuf + totalLen, recvBuf, recvLen);
            totalLen = totalLen + recvLen;
            continue;
        }
        else if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
            continue;
        else
        {
            delete[] totalBuf;
            totalBuf = NULL;
            return -1;
        }
    }
}

string HttpRequest::GetHostFromURL(const char *strURL)
{
    string urlBuf = strURL;

    size_t hostLeft = urlBuf.find("http://");
    if (urlBuf.npos == hostLeft)
        hostLeft = 0;
    else
        hostLeft = hostLeft + strlen("http://");
    urlBuf = urlBuf.substr(hostLeft);

    size_t hostRight = urlBuf.find_first_of("/");
    if (urlBuf.npos != hostRight)
        urlBuf = urlBuf.substr(0, hostRight);

    return urlBuf;
}

string HttpRequest::GetParamFromURL(const char *strURL)
{
    string urlBuf = strURL;

    size_t hostLeft = urlBuf.find("http://");
    if (urlBuf.npos == hostLeft)
        hostLeft = 0;
    else
        hostLeft = hostLeft + strlen("http://");
    urlBuf = urlBuf.substr(hostLeft);

    size_t hostRight = urlBuf.find_first_of("/");
    if (urlBuf.npos == hostRight)
        urlBuf = "";
    else
        urlBuf = urlBuf.substr(hostRight);

    return urlBuf;
}

uint16_t HttpRequest::GetPortFromURL(const char *strURL)
{
    string host = GetHostFromURL(strURL);
    if (0 == host.length())
        return 0;

    uint16_t port = 0;
    size_t portIndex = host.find_first_of(':');
    if (host.npos == portIndex)
        port = 80;
    else
    {
        host = host.substr(portIndex + 1);
        port = atoi(host.c_str());
    }

    return port;
}

string HttpRequest::GetIPFromURL(const char *strURL)
{
    string host = GetHostFromURL(strURL);
    string ip = "";

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

void HttpRequest::InvalidSocket(int &socket)
{
    if (socket > 2)
    {
        shutdown(socket, SHUT_RDWR);
        close(socket);
        socket = -1;
    }
}