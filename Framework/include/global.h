#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define ISRELEASE
#include <jsoncpp/json.h>
#include <malloc.h>
//主机信息
#define SERVERADDRESS "127.0.0.1"
#define FWVERSION "1.0.20"
#define SWVERSION "1.0.0"
#define OSVERSION "Centos7"
#define HWVERSION "MORELINKS"

// network status
// #define ONLINE 1
// #define OFFLINE 0

// device status
#define GATEWAY_ONLINE "000"
#define GATEWAY_OFFLINE "001"
#define DEVICE_ONLINE "002"
#define DEVICE_OFFLINE "003"
#define DEVICE_USUAL "100"
#define DEVICE_FAULT "101"

// process type
#define NOTIFY 0
#define WRITE 1
#define NETWORKSTATUS 2
#define DATA 3
#define REPORT 5
#define REQUEST 6

// Communication Status
#define DISCONNECTED 0
#define REGISTERING 1
#define REGISTERED 2
#define LOGING 3
#define LOGON 4
#define REGISTEREDDEVICES 5

//Communication Interval
#define REGISTERINTV 5
#define LOGININTV 2
#define HBINTVAL 30
#define HBTIMEOUT 90
#define LOGINPORT 8189

//HeartBeat Lock
#define UNLOCKED 0
#define LOCKED 1

// 云平台提供的 AesKey, Token, Appid, Type
#define AESKEY "y9J6R8qAuBSfy1y2Q7HhSYY9znM2TWdwsNacxwX2kbc"
#define SYSTOKEN "morelinks!$2019" //"test"
#define APPID "morelinks"          //"test"
#define TYPE "491T1871556524"
#define CONNID 2 // 配置程序 1, 主机程序 2

#ifdef ISRELEASE
#define NETEQUIPMENTNAME "enp1s0"
#define LOCALNETEQUIPMENT "enp2s0"
#else
#define NETEQUIPMENTNAME "wlp3s0"
#define LOCALNETEQUIPMENT "eno1"
#endif

// share Memory key
#define SHM_FAILED -1
#define SHMREQUESTKEY 538509593 // 0x20190119
#define SHMREPORTKEY 538509600  // 0x20190120
#define SHRDRIVERS 538509601    // 0x20190121
#define SHRDEVICE 538509602     // 0x20190122
#define SHRHOSTINFO 538513685   // 0x20191115
#define MAXSHMSIZE 1000
#define MAXSHMDRIVERSIZE 100
#define MAXSHMDEVICESIZE 1000

// Thread
#define MAX_THREAD 100

// Device Infomation
#define MAX_EP 32
#define MAX_REG 4
#define MAX_DEVICE 1000
#define MAX_CMD 32

// Driver Type
// 0 LOGO, 1 TCP, 2 MODBUS(RTU/TCP), 3 RS232, 4 485
#define LOGO 0
#define TCP 1
#define MODBUS 2
#define SERIALPORT 3
#define RS485 4

// Message Format
#define _HEX 0
#define _ASCII 1

// Buffer
#define BUFFERSIZE 8192 * 200
#define EPVALUESIZE 1024
#define EPINFOSIZE 2048

// Message Type
#define MsgREG 0
#define MsgSTATUS 1
#define MsgREP 2
#define MsgREQ 3
#define MsgREREG 4
#define MsgRESTATUS 5
#define MsgREFUN 6
#define MsgCHECKVER 7
#define MsgIE 1000
#define MsgREPTRIGGER 1001
#define MsgTRIGGERSCENE 1002
#define MsgTRIGGERSCENESTATUS 1003
#define MsgSCENESRESULT 1004
#define MsgSYNCSCENE 1005
#define MsgINITSCENE 1006
#define MsgHTTPREQ 1007
#define MsgASSISTRESULT 2000
#define MsgSECURITY 3000
#define MsgSYNCSECURITY 3001
#define MsgINITSECURITY 3002
#define MsgSECURITY2IE 3003
#define MsgSIMPLELOGIC 4000
#define MsgSYNCSIMPLELOGIC 4001
#define MsgINITSIMPLELOGIC 4002
#define MsgVIRTUALPRESENCE 5000
#define MsgSYNCVIRTUALPRESENCE 5001
#define MsgINITVIRTUALPRESENCE 5002

//Register Type
#define BI 0
#define BQ 1
#define WI 2
#define WQ 3

//Data & Data Type
#define LOGIC 0
#define NUMBER 1
#define STRING 2
#define OBJECT 3

#define MAXLIFETIME 180 * 1000

//

typedef struct
{
    int MsgType;
    int param1;
    void *param2;
    size_t param3;
    unsigned char *Data;
    long int pHandle;
} TransferData, *pTransferData;

// typedef struct
// {
//     char controllerId[128];
//     int regType; //0 bi, 1 bq, 2 wi, 3 wq
//     int regId;
// } Reg, *pReg;

// typedef struct
// {
//     int regType; //0 bi, 1 bq, 2 wi, 3 wq
//     int regId[50];
//     int readLength;
// } RegType, *pRegType;

// typedef struct
// {
//     int cmdType; //
//     char regValue[8];
//     char cmd[128];
// } RegsCmd, *pRegsCmd;

// typedef struct
// {
//     int regType; //0 bi, 1 bq, 2 wi, 3 wq
//     char regId[32];
//     int cmdNum;
//     RegsCmd regCmd[MAX_CMD];
// } RegsInfo, *pRegsInfo;

// typedef struct
// {
//     char epId[32];
//     int epsNum;
//     int regNum;
//     bool Enable;
//     RegsInfo ReadRegs[MAX_REG];
//     RegsInfo WriteRegs[MAX_REG];
// } EpsInfo, *pEpsInfo;

typedef struct
{
    char epInfo[32];
    char epValue[EPINFOSIZE];
} EpsInfo, *pEpsInfo;

typedef struct
{
    char DevId[32];
    char cDevId[128];
    char DevType[16]; // 设备接入时, 预设好的ID, 根据 Type调用 Read, Write转换函数
    char Ids[64];     // 设备 ID
    char DriverName[32];
    char pId[32];
    char pType[16];
    int devsNum;
    int epsNum;
    int DriverId;
    EpsInfo pEpsInfo;
} DevsInfo, *pDevsInfo;

typedef struct
{
    /* data */
    char IP[256];
    int Port;
    long Interval;
    long Timeout;
    long MinActTime;
} DrvOptions;

typedef struct
{
    int Id;
    char Type[16]; // 0 LOGO, 1 TCP, 2 MODBUS(RTU/TCP), 3 RS232, 4 485
    char Name[32];
    char File[256];
    int DriverNum;
    DrvOptions Options;
} DriversInfo, *pDriversInfo;

typedef struct
{
    char houseId[32];
    char uuid[128];
    char registerAddr[256];
    char registerAddr2[256];
    unsigned int registerPort;
    char loginAddr[256];
    unsigned int loginPort;
    char aesKey[128];
    char sysToken[128];
    char appId[128];
    char type[128];
    char loginToken[128];
    char refreshKey[128];
    int HeartbeatInterval;
    char localIP[16];
    char serverIP[16];
} HostInfo, *pHostInfo;

typedef struct
{
    char EpId[32];
    char EpValue[EPVALUESIZE];

} EpData, *pEpData;

typedef struct
{
    char DevId[32];
    char houseId[32];
    char uuid[128];
    char cDevId[128];
    char devType[16];
    char dataType[16]; // "status" "rep" "req" "sync" "reFun"
    int Repeat;
    long Timeout;
    int epNum;
    unsigned long data_timestamp;
    EpData epData;
} DevsData, *pDevsData;

typedef struct
{
    char DevId[32];
    char houseId[32];
    char uuid[128];
    char cDevId[128];
    char devType[16];
    char dataType[16]; // "status" "rep" "req" "sync" "reFun"
    int Repeat;
    long Timeout;
    int epNum;
    unsigned long data_timestamp;
    EpData *epData;
} DevsLocalData, *pDevsLocalData;

typedef struct
{
    char sceneId[32];
    char syncId[32];
    char reqId[128];
    int reqType; // 0 Sync All, 1 Sync single
} IntelligenceInfo, *pIntelligenceInfo;

typedef struct
{
    char securityId[32];
    char syncId[32];
    char reqId[128];
    int reqType; // 0 Sync All, 1 Sync single
} SecurityInfo, *pSecurityInfo;

typedef struct
{
    char deviceId[32];
    char devType[16];
    char syncId[32];
    char reqId[128];
    int reqType; // 0 Sync All, 1 Sync single
} SimpleLogicInfo, *pSimpleLogicInfo;

typedef struct
{
    char deviceId[32];
    char devType[16];
    char syncId[32];
    char reqId[128];
    int reqType; // 0 Sync All, 1 Sync single
} VirtualPresenceInfo, *pVirtualPresenceInfo;

typedef int (*SocketCB)(TransferData *, int);
typedef int (*DevsCB)(DevsData *, int);
typedef int (*UDPCB)(TransferData *, int);

#define GET_ARRAY_LEN(array, len)                 \
    {                                             \
        len = (sizeof(array) / sizeof(array[0])); \
    }

// 配置专用

// share Memory key
#define SHMCFGREQKEY 538513668 // 0x20191104
#define SHMCFGREPKEY 538513669 // 0x20191105
#define MAXSHMCFGSIZE 50

#define CFG_changeTypeRep "changeTypeRep"
#define CFG_autoSearch "autoSearch"
#define CFG_manualSearch "manualSearch"
#define CFG_searchRep "searchRep"
#define CFG_changeType "changeType"
#define CFG_masterPos "masterPos"
#define CFG_masterPosRep "masterPosRep"
#define CFG_slavePos "slavePos"
#define CFG_newDevRep "newDevRep"
#define CFG_controllerRep "controllerRep"
#define CFG_configStepRep "configStepRep"
#define CFG_setAttribute "setAttribute"
#define CFG_restartDriver "restartDriver"
#define CFG_repSearchEnd "repSearchEnd"
#define CFG_repAttribute "repAttribute"

typedef struct
{
    char Id[32];
    char Value[128];
} CfgParam, *pCfgParam;

typedef struct
{
    char DevId[32];
    char uuid[128];
    char pId[32];
    char pType[16];
    char devType[16];
    char dataType[16];
    char DriverName[32];
    int lastLevel;
    int total;
    int index;
    long Timeout;
    int paramNum;
    unsigned long data_timestamp;
    CfgParam paramData[16];
} CfgDevsData, *pCfgDevsData;

#endif