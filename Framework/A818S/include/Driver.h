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
    static CDriver *pDriverStatic;   // 静态对象指针
    DriversInfo *pDriverInfo;        // 驱动信息
    DevsInfo *pDevicesInfo;          // 设备信息
    SDriCommParam *pCommParam;       // 通讯参数
    SDevStatusParam *pDevicesStatus; // 设备状态
    CQueue<SDevOrder> sendList;      // 命令发送队列
    CQueue<SRecvBuffer> recvList;    // 数据接收队列
    CTCPClientBase tcp;              // 通信基类
    SHMFIFOBase shmW;                // 写共享内存
    SHMFIFOBase shmCfgW;             // 写配置共享内存

    int searchId;

    int syncIndex;
    ESyncState syncState;

    EDevType DevTypeConv(const char *devType); // 设备类型转换
    int DevEpInfoInit(int devIndex);           // 设备功能点信息初始化

    // 发送处理
    pthread_t sendHandleThreadID;
    static void *SendHandle(void *pParam);

    int CheckNetworkStatus();                            // 检查网络状态
    int WriteDriStatus(const char *driStatus);           // 写驱动状态
    int WriteDevStatus(int devIndex);                    // 写设备状态
    int ExecuteOrder(SDevOrder *order);                  // 执行命令
    int RefreshDevStatus(int devIndex, EDevState state); // 刷新设备状态
    int WriteCtrlRsp(SDevOrder *order);                  // 写控制响应
    int SyncConv(SDevOrder &syncOrder);
    int BuildSyncOrder();
    int DriverConfigLink();

    static int RecvCallback(TransferData *data, int type); // TCP接收回调
    // 接收处理
    pthread_t recvHandleThreadID;
    static void *RecvHandle(void *pParam);

    int RecvParse(SRecvBuffer *recvBuf);                   // 接收数据解析
    void BuildBaseEPData(DevsData *devData, int devIndex); // 写功能点数据
    void BuildWholeEPData(DevsData *devData, int devIndex, uint8_t epID, const char *epValue, double range);
    void WriteEpData(int devIndex, uint8_t epID, const char *epValue, double range);

    // 命令获取
    pthread_t getOrderThreadID;
    static void *GetOrder(void *pParam);

    int BuildCtrlOrder(DevsData *devData);                                           // 构建控制命令
    int GetDevIndex(const char *devID);                                              // 获取设备索引
    int GetEpIndex(int devIndex, uint8_t epID);                                      // 获取功能点索引
    int CtrlConv(int devIndex, int epID, const char *epValue, SDevOrder *ctrlOrder); // 控制命令转换
    int RefreshChStatus(int devIndex, uint8_t chID, bool chStatus);                  // 刷新通道状态

    // 配置命令获取
    pthread_t getConfigOrderThreadID;
    static void *GetConfigOrder(void *pParam);

    int HandleCfgData(CfgDevsData *cfgData); // 处理配置数据

public:
    CDriver(int argDriIndex, int argDevIndex, int argDevNum, int argMaxDriNum, int argMaxDevNum);
    ~CDriver();

    bool GetOrderStart();
    bool GetConfigOrderStart();
    bool HandleStart();
};

#endif