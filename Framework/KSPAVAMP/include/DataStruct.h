#ifndef _DATA_STRUCT_H
#define _DATA_STRUCT_H

#include <stdint.h>
#include <netinet/in.h>

#include "../../include/BaseTool.h"

// #define DEBUG
#define DRINAME "KSPAVAMP"

#define DEVTYPE_SERIES "3881569389538" // 系列     
enum EDevType   //设备类型
{
    UNKNOW,
    SERIES      
};

enum EDevState // 设备状态
{
    ONLINE,
    OFFLINE
};

#define STRSIZE_STATUS 8         // 状态
#define STRSIZE_EPVALUE 16       // 功能点值
#define BYTESIZE_TRANSFERDATA 16 // 传输数据

struct SDriCommParam // 驱动通讯参数
{
    char ip[INET_ADDRSTRLEN];           // IP地址
    int port;                           // 端口号
    char mac[16];                       // MAC地址   
    uint64_t connectedTS;               // 连接时间戳
    char connectStatus[STRSIZE_STATUS]; // 连接状态
};

enum EOrderType
{
    CTRL,       //控制
    SYNC        //同步
};

//发送命令状态
enum EOrderState
{
    WAIT,                //阻塞  
    COMPLETE             //完成
};

struct SDevOrder // 设备操作命令
{
    int devIndex;                           // 设备索引
    EOrderType orderType;                   // 命令类型
    uint8_t data[BYTESIZE_TRANSFERDATA];    // 数据
    uint8_t dataLen;                        // 数据长度

    char orderID[128];      // 命令ID
    int repeat;             // 重试次数
    uint64_t orderTS;       // 命令时间戳(ms)
    long orderTO;           // 命令超时时间(ms)
        
    EOrderState state;      // 命令执行状态
};

struct SDevEPInfo // 设备功能点信息
{
    uint8_t epID;
    uint8_t epNum;                        
    char repLast[STRSIZE_EPVALUE];        // 上次响应存储
};

struct SDevStatusParam              // 设备状态参数
{                                   // *
    int devNum;                     // 设备数量
    EDevType devType;               // 设备类型
    bool isOnline;                  // 是否在线 true:online false:offline
    char strOnline[STRSIZE_STATUS]; // 是否在线 "002":online "003":offline
    bool isFault;                   // 是否异常 true:error  false:usual
    char strFault[STRSIZE_STATUS];  // 是否异常 "101":error  "100":usual

    SDevEPInfo *epInfo; // 功能点信息
};

struct SRecvBuffer                       // 接收缓存
{                                        // *
    uint8_t data[BYTESIZE_TRANSFERDATA]; // 数据
    size_t dataLen;                      // 数据长度
};

#endif