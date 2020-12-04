#ifndef _DRIVER_H
#define _DRIVER_H

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <sstream>

#include "../../include/global.h"
#include "../../include/SHMFIFO.h"
#include "../../include/TCPClientBase.h"
#include "../../include/MLGWConfig.h"
#include "../../include/Queue.h"
#include "DataStruct.h"

using namespace NSBaseTool;

class CDriver
{
private:
    DriversInfo *pDriverInfo;      // 驱动信息
    DevsInfo *pDevicesInfo;        // 设备信息
    SDriCommParam *pCommParam;     // 通讯参数
    SDevStateParam *pDevicesState; // 设备状态
    CQueue<SDevOrder> sendList;    // 命令发送队列
    CQueue<SRecvBuffer> recvList;  // 数据接收队列
    CTCPClientBase tcp;            // 通信基类
    SHMFIFOBase shmW;              // 写共享内存
    SHMFIFOBase shmCfgW;           // 写配置共享内存
    uint8_t StateSyncIndex;        // 状态同步索引
    static CDriver *pDriverStatic; // 静态对象指针
    bool isRunning;                // 运行标志
    bool sendDoneFlag;             // 发送完成标志

    uint8_t *sendInfo;             // 发送信息备份

    int syncState;
    CommuicationState connectState;
    SendRecvState sendRecvState;

    uint8_t CmpAsString(char *lastRep, char *buf);               // 字符比对
    uint8_t CmpAsDouble(char *lastRep, char *buf, double range); // 数值比对
    uint8_t DataCmp(char *lastRep, char *buf, double range);     // 数据比对

    int GetPeerMAC(int socket, char *macBuf, char *adapterName); // 获取网关MAC
    bool AckCmp(uint8_t *sendInfo, uint8_t *recvInfo, size_t recvInfoLen);

    // bool checkFlag;
    CheckStateFlag checkConnectFlag;
    int repeatCheckTime;
    uint64_t standByTime;
    int connCheckCnt;
    // bool timeDone;

    // 发送处理
    pthread_t sendHandleThreadID;
    static void *SendHandle(void *pParam);

    int RepDevState();                  // 上报设备状态
    int ExecuteOrder(SDevOrder *order); // 执行指令
    int WriteOrderRsp(SDevOrder *order);
    void WriteDevState(int devIndex);           // 写设备状态数据包
    int FlashDevState(int devIndex, int state); // 刷新设备状态
    int BuildSyncOrder();                       // 构建同步指令
    int SyncConv(SDevOrder &syncOrder);
    bool SendPJ_REQ();
    bool ProtocalConnectChenck();

    static int RecvCallback(TransferData *data, int type); // TCP接收回调

    // 接收处理
    pthread_t recvHandleThreadID;
    static void *RecvHandle(void *pParam);

    int RecvParse(SRecvBuffer *recvBuffer); // 接收数据解析
    void WriteRepData(int devIndex, int epID, char *epValue, double range);
    void BuildBaseRepData(DevsData *devData, int devIndex);                                         // 构建上报数据包
    void BuildWholeRepData(DevsData *devData, int devIndex, int epID, char *epValue, double range); // 构建指令响应数据包

    // 命令获取
    pthread_t getOrderThreadID;
    static void *GetOrder(void *pParam);

    int GetDevIndex(char *devID);           // 获取设备索引
    int GetEPIndex(int devIndex, int epID); // 获取功能点索引
    int BuildReqOrder(DevsData *devData);
    int ReqConv(int devIndex, int epID, char *epValue, SDevOrder *reqOrder); // 请求指令值转换

    // 配置命令获取
    pthread_t getConfigOrderThreadID;
    static void *GetConfigOrder(void *pParam);

    int HandleConfigOrder(CfgDevsData *configData); // 处理配置命令

public:
    CDriver(int argDriIndex, int argDevIndex, int argDevNum, int argMaxDriNum, int argMaxDevNum);
    ~CDriver();

    bool HandleStart();
    bool GetOrderStart();
    bool GetConfigOrderStart();
};

#endif