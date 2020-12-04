#ifndef _DATA_STRUCT_H
#define _DATA_STRUCT_H

#include <stdint.h>
#include <netinet/in.h>
#include "../../include/BaseTool.h"

// 调试标志
// #define DEBUG
#define DRINAME "JVCPJLAN"
#define DEVTYPE_JVCPJLAN "6441664440611" // A818S
// #define SPLIT "====================\n"

// 比对结果
#define CMP_UNDIFFER 0 // 无差异
#define CMP_INITSYNC 1 // 初始同步
#define CMP_DIFFER 2   // 有差异
// 上报数据类型
#define REP_SYNC "sync"     // 同步
#define REP_CHANGE "change" // 差异
#define REP_STATUS "status" // 状态
#define REP_REQ "req"       // 执行

#define BYTESIZE_TRANSFERDATA 16 // 传输数据

// 设备类型
// #define DEV_TYPE_SERIES "7571500078564"

// 设备状态
#define STATE_NORMAL 0   // 正常
#define STATE_ERROR -1   // 故障
#define STATE_OFFLINE -2 // 离线
// 时间参数
#define INTERVAL_VOID 10                 // 空操作间隔(ms)
#define INTERVAL_GETORDER 10             // 命令获取间隔(ms)
#define DURATION_MANUAL_SEARCH 10 * 1000 // 手动添加时长(ms)
// 网络参数
#define PORT_DEFAULT 20554      // 默认端口
#define ADAPTER_NAME "enp2s0" // 适配器名称
// 空间参数
#define STRSIZE_MAC 16   // MAC地址
#define SIZE_SENDDATA 64 // 发送数据
// 状态同步
// #define SYNC_TOTAL 5      // 同步总数
#define SYNC_SYSSWITCH 0 // 系统开关

struct SDriCommParam          // 驱动通讯参数
{                             // *
    char IP[INET_ADDRSTRLEN]; // IP地址
    char mac[STRSIZE_MAC];    // MAC地址
    int port;                 // 端口号
    long timeout;             // 超时时间(ms)
    long minActTime;          // 同一设备动作间隔(ms)
    long interval;            // 不同设备动作间隔(ms)
    int socketID;             // 连接套接字
};

enum EOrderState
{
    WAIT,
    DONE
};

// enum AckCH
enum SendRecvState
{
    EXECSUCCESS,
    EXECFALSE
};

enum CommuicationState
{
    tcpConnected,
    tcpDisconnected,
    waitPJ_OK,
    noRecvedPJ_OK,
    recvPJ_OK,
    sentPJ_REQ,
    recvPJ_ACK
};

enum CheckStateFlag
{
    checkSent,
    recvCheck
};
 
#define SIZE_ORDERDATA 32   // 数据容量
#define STRSIZE_ORDERID 128 // 命令序列号容量大小
struct SDevOrder            // 设备操作命令
{                           // *
    int devIndex;           // 设备索引
    uint8_t devAct;         // 设备操作 write:0x00 read:0x01
    uint8_t data[SIZE_ORDERDATA];  // 数据
    // uint8_t *data;                 // 数据
    uint8_t dataLen;               // 数据长度
    char orderID[STRSIZE_ORDERID]; // 命令序列号
    int repeat;                    // 重试次数
    uint64_t timeStamp;            // 命令时间戳(ms)
    long timeout;                  // 超时时间(ms)
    EOrderState state;             // 命令执行状态
};

#define STRSIZE_EPVALUE 32         // 功能点值
struct SDevEPInfo                  // 设备功能点信息
{                                  // *
    int epID;                      // 功能点编号
    int epNum;                     // 功能点数量
    char repLast[STRSIZE_EPVALUE]; // 功能点最新上报
};

#define MAX_EPID 17                   // 功能点最大编号
struct SDevStateParam                // 设备状态参数
{                                    // *
    int devNum;                      // 设备数量
    bool isOnline;                   // 是否在线 true:online false:offline
    char strOnline[STRSIZE_EPVALUE]; // 是否在线 "002":online "003":offline
    bool isFault;                    // 是否异常 true:error  false:usual
    char strFault[STRSIZE_EPVALUE];  // 是否异常 "101":error  "100":usual
    SDevEPInfo epInfo[MAX_EPID];     // 功能点信息
};

#define SIZE_RECVDATA 64 // 数据容量
struct SRecvBuffer       // 接收缓存
{                        // *
    uint8_t data[SIZE_RECVDATA]; // 数据
    // uint8_t *data;
    size_t dataLen; // 数据长度
};

enum ESyncState
{
    SYNC_EP01,
    SYNC_EP03
};

#endif