#ifndef _TCP_CLIENT_BASE_H
#define _TCP_CLIENT_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../include/global.h"

#define DATA_LIST_SIZE 128

// #define DEBUG_FLAG

class CTCPClientBase
{
private:
  int socketClient;                                // 客户端套接字
  bool isConnected;                                // 套接字连接状态
  char serverIP[INET_ADDRSTRLEN];                  // 服务器IP地址
  uint16_t serverPort;                             // 服务器端口号
  SocketCB callbackHandle;                         // 回调函数值针
  pthread_t recvThreadID, handleThreadID;          // 接收&处理线程号
  pthread_attr_t recvThreadAttr, handleThreadAttr; // 接收&处理线程属性
  bool isRecvRunning, isHandleRunning;             // 线程运行标志
  pthread_mutex_t dataListMutex;                   // 数据列表互斥锁
  std::vector<TransferData *> dataList;            // 数据列表

  bool WriteToList(pTransferData pData);  // 写入队列
  bool ReadFromList(pTransferData pData); // 读出队列
  bool DeleteList();                      // 删除队列

  int executeCallback(SocketCB pCallback, TransferData *data, int type); // 执行回调函数
  int WriteToServer(uint8_t *data, int dataLen);                         // 发送数据到服务器
  int SetKeepAlive(int socket, int idle, int cnt, int intv);             // 设置保活机制参数

  static void *HandleList(void *pParam); // 处理线程函数
  static void *Recieve(void *pParam);    // 接收线程函数

public:
  CTCPClientBase();          // 构造函数
  virtual ~CTCPClientBase(); // 析构函数

  int GetSocketID();                                          // 获取套接字
  int ConnectToServer(char *IP, int port, SocketCB callback); // 连接服务器
  bool Disconnect();                                          // 断开连接
  bool isTCPConnected();                                      // 判断TCP套接字连接状态
  bool Send(uint8_t *sendData, int sendLen);                  // 发送数据
};

#endif