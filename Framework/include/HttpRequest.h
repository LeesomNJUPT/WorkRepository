#ifndef _HTTP_REQUEST_H
#define _HTTP_REQUEST_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>

#define STRSIZE_URL 1024            // URL
#define BYTESIZE_HTTPBASE 64 * 1024 // HTTP缓冲
#define BYTESIZE_HTTPRECV 1 * 1024  // HTTP接收

using namespace std;

class HttpRequest
{
public:
    struct SHttpRsp
    {
        string *header;
        uint8_t *data;
        size_t dataLen;
    };

private:
    int HttpRequestExec(const char *strMethod, const char *strURL, const char *strData, SHttpRsp &rsp);
    string GetHostFromURL(const char *strURL);
    string GetParamFromURL(const char *strURL);
    uint16_t GetPortFromURL(const char *strURL);
    void InvalidSocket(int &socket);
    string GetIPFromURL(const char *strURL);
    string HttpHeaderCreate(const char *strMethod, const char *strURL, const char *strData);
    int HttpDataTransmit(int socket, string header, SHttpRsp &rsp);
    int RecvDataParse(uint8_t *data, size_t dataLen, SHttpRsp &rsp);
    int MemRealloc(uint8_t *&addr, size_t orgSize, size_t newSize);

public:
    HttpRequest();
    ~HttpRequest();

    int HttpGet(const char *strURL, SHttpRsp &rsp);
    int HttpPost(const char *strURL, const char *strData, SHttpRsp &rsp);
};

#endif