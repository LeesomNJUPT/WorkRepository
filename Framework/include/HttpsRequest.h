#ifndef _HTTPS_REQUEST_H
#define _HTTPS_REQUEST_H

#include <vector>
#include <iostream>
#include <string>
#include <vector>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "openssl/err.h"
#include "openssl/ssl.h"
#include "openssl/rand.h"
#include "openssl/crypto.h"

#define BUFSIZE 1024 * 1024
#define RECVBUFSIZE 1024
#define URLSIZE 1024
#define INVALID_SOCKET -1
#define READBUFSIZE 512

#define HTTP_HEADERS_MAXLEN 2048 // Headers 的最大长度
#define URL_MAX_SIZE 1024

using std::string;
using std::vector;

#define BYTESIZE_HTTPSBASE 4 * 1024 // HTTPS缓冲
#define BYTESIZE_HTTPSRECV 1 * 1024 // HTTPS接收

class HttpsRequest
{
public:
  struct SHttpsRsp
  {
    string *header;
    uint8_t *data;
    size_t dataLen;
  };

  enum EDecodeState
  {
    DECODELEN,
    DECODEDATA,
    DECODETAIL
  };

  int HttpsPost(const char *url, const char *strData, SHttpsRsp &rsp);

private:
  string GetHostFromUrl(const char *url);
  string GetParamFromUrl(const char *url);
  string GetIPFromUrl(const char *url);
  uint16_t GetPortFromUrl(const char *url);
  string HttpsHeaderCreate(const char *method, const char *url, const char *data);
  int ConnectToServer(const char *ip, uint16_t port);
  size_t SSLSend(SSL *ssl, string sendStr);
  int MemRealloc(uint8_t *&addr, size_t orgSize, size_t newSize);
  size_t GetStreamData(SSL *ssl, uint8_t *&dataBuf);
  size_t GetChunkedData(SSL *ssl, uint8_t *&dataBuf);
  int SSLRecv(SSL *ssl, SHttpsRsp &rsp);
  int GetHeaderValue(string strHeader, const char *key, string &strValue);
  uint8_t ChToHex(const char ch);
  size_t HexStrToSize(string strHex);

public:
  HttpsRequest();
  ~HttpsRequest();
  char *GetIPbyName(const char *name);

  bool GetIPFromURL(const char *strURL, char *returnIP);
  int https_post(char *host, int port, char *url, const char *data, int dsize, char *buff, int bsize);
  int https_post(char *url, int port, const char *data, int dsize, char *buff, int bsize, char *errCode);
  int https_get(char *host, char *ip, int port, char *url, char *buff, int bsize, char *errCode);

private:
  int client_connect_tcp(char *server, int port);
  int post_pack(const char *host, int port, const char *page, int len, const char *content, char *data);
  int get_pack(const char *host, int port, const char *page, char *data);
  SSL *ssl_init(int sockfd);
  int ssl_send(SSL *ssl, const char *data, int size);
  int ssl_recv(SSL *ssl, char *buff, int size, char *errCode = NULL);

  std::string getHeader(std::string respose, std::string key);
  std::vector<std::string> split(const std::string &s, const std::string &seperator);
  char *GetTimeStamp();
  int getChunkedData(SSL *ssl, char *buff, int readsizesize);
  int getUnChunkedData(SSL *ssl, char *buff, int readsizesize);
  char *GetHostFromURL(const char *strURL);
  void FreeSocket(int fd);
  void FreeSSL(SSL *sslFd);

  /*
  Headers 按需更改
  */
  // const char *HttpsPostHeaders = "User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n"
  //                                "Cache-Control: no-cache\r\n"
  //                                "Accept: */*\r\n"
  //                                "Content-type: application/json;charset=utf-8\r\n";
};
#endif