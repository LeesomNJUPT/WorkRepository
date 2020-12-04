#ifndef _DATA_STRUCT_H
#define _DATA_STRUCT_H

#include <stdint.h>
#include <netinet/in.h>

#include "../../include/BaseTool.h"

// #define DEBUG
#define DRINAME "A818S"
#define DEVTYPE_A818S "9201611950394" // A818S

enum EDevType
{
    UNKNOW,
    A818S
};

enum EDevState // 设备状态
{
    STATE_ONLINE,
    STATE_OFFLINE
};

#define STRSIZE_ID 16
#define STRSIZE_STATUS 8         // 状态
#define STRSIZE_EPVALUE 16       // 功能点值
#define BYTESIZE_TRANSFERDATA 16 // 传输数据

struct SDriCommParam // 驱动通讯参数
{
    char ip[INET_ADDRSTRLEN];           // IP地址
    int port;                           // 端口号
    char mac[16];                       // MAC地址
    uint64_t connectedTs;               // 连接时间戳
    char connectStatus[STRSIZE_STATUS]; // 连接状态
};

enum EOrderType
{
    CTRL,
    SYNC,
    SEARCH
};

enum EOrderState
{
    WAIT,
    DONE
};

struct SDevOrder // 设备操作命令
{
    int devIndex;         // 设备索引
    EOrderType orderType; // 命令类型

    uint8_t data[BYTESIZE_TRANSFERDATA]; // 数据
    uint8_t dataLen;

    char orderId[128]; // 命令ID
    int repeat;        // 重试次数
    uint64_t orderTs;  // 命令时间戳(ms)
    long orderTo;      // 命令超时时间(ms)

    EOrderState state; // 命令执行状态
};

struct SDevEpInfo // 设备功能点信息
{
    uint8_t epId;
    uint8_t epNum;
    char repLast[STRSIZE_EPVALUE];
};

struct SDevStatusParam              // 设备状态参数
{                                   // *
    int devNum;                     // 设备数量
    int devId;
    EDevType devType;               // 设备类型
    bool isOnline;                  // 是否在线 true:online false:offline
    char strOnline[STRSIZE_STATUS]; // 是否在线 "002":online "003":offline
    bool isFault;                   // 是否异常 true:error  false:usual
    char strFault[STRSIZE_STATUS];  // 是否异常 "101":error  "100":usual

    SDevEpInfo *epInfo; // 功能点信息
    uint8_t syncToc;
};

struct SRecvBuffer // 接收缓存
{
    uint8_t *data;
    size_t dataLen;
};

enum ESyncState
{
    SYNC_EP02
};


#endif