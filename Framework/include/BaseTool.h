#ifndef _BASE_TOOL_H
#define _BASE_TOOL_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <string>

#include "jsoncpp/json.h"
#include "openssl/md5.h"

namespace NSBaseTool
{
    extern const char SPLIT[];

    extern const char DEVTYPE_RS485[];
    extern const char DEVTYPE_RS232[];

    extern const int KEY_DRIINFO;
    extern const int KEY_DEVINFO;
    extern const int KEY_HOSTINFO;
    extern const int KEY_DEVREQ;
    extern const int KEY_DEVREP;
    extern const int KEY_CFGREQ;
    extern const int KEY_CFGREP;

    extern const int SMGT_DRIINFO;
    extern const int SMGT_DEVINFO;
    extern const int SMGT_HOSTINFO;
    extern const int SMGT_DEVDATA;
    extern const int SMGT_CFGDATA;

    struct SDriOption
    {
        char ip[256];
        int port;
        long devIntvl;
        long timeout;
        long busIntvl;
    };

    struct SDriInfo
    {
        int id;
        char type[16];
        char driName[32];
        char fileName[256];
        int driNum;
        SDriOption option;
    };

    struct SExtraInfo
    {
        char info[32];
        char value[2048];
    };

    struct SDevInfo
    {
        char devID[32];
        char cDevID[128];
        char devType[16];
        char ids[64];
        char driName[32];
        char pID[32];
        char pType[16];
        int devNum;
        int epNum;
        int driID;
        SExtraInfo extraInfo;
    };

    struct SHostInfo
    {
        char houseID[32];
        char uuid[128];
        char regAddr[256];
        char regAddr2[256];
        uint32_t regPort;
        char loginAddr[256];
        uint32_t loginPort;
        char aesKey[128];
        char sysToken[128];
        char appID[128];
        char type[128];
        char loginToken[128];
        char refreshKey[128];
        int heartbeatIntvl;
        char localIP[16];
        char serverIP[16];
    };

    struct SEPData
    {
        char epID[32];
        char epValue[1024];
    };

    struct SDevData
    {
        char devID[32];
        char houseID[32];
        char uuid[128];
        char cDevID[128];
        char devType[16];
        char dataType[16];
        int repeat;
        long timeout;
        int epNum;
        uint64_t timestamp;
        SEPData epData;
    };

    extern const char DEVDATATYPE_STATUS[];
    extern const char DEVDATATYPE_SYNC[];
    extern const char DEVDATATYPE_CHANGE[];
    extern const char DEVDATATYPE_REQ[];
    extern const char DEVDATATYPE_REFUN[];

    struct SParamData
    {
        char id[32];
        char value[128];
    };

    struct SCfgData
    {
        char devID[32];
        char uuid[128];
        char pID[32];
        char pType[16];
        char devType[16];
        char dataType[16];
        char driName[32];
        int lastLevel;
        int total;
        int index;
        long timeout;
        int paramNum;
        uint64_t timestamp;
        SParamData param[16];
    };

    extern const char CFG_AUTOSEARCH[];
    extern const char CFG_MANUALSEARCH[];
    extern const char CFG_SEARCHREP[];
    extern const char CFG_REPSEARCHEND[];
    extern const char CFG_RESTARTDRIVER[];
    extern const char CFG_CHANGETYPE[];
    extern const char CFG_CHANGETYPEREP[];
    extern const char CFG_MASTERPOS[];
    extern const char CFG_MASTERPOSREP[];
    extern const char CFG_SLAVEPOS[];
    extern const char CFG_NEWDEVREP[];
    extern const char CFG_CONTROLLERREP[];
    extern const char CFG_CONFIGSTEPREP[];
    extern const char CFG_SETATTRIBUTE[];
    extern const char CFG_REPATTRIBUTE[];

    extern const size_t STRSIZE_MAC;
    extern const size_t STRSIZE_DEVTYPE;
    extern const size_t STRSIZE_UUID;
    extern const size_t STRSIZE_ID;

    extern const uint64_t INTERVAL_VOID;     // us
    extern const uint64_t INTERVAL_READSHM;  // us
    extern const uint64_t INTERVAL_WRITESHM; // us

    extern const uint64_t DURATION_CONNECTED_VALIDITY; // ms

    extern const uint32_t PORT_DEFAULT;
    extern const uint32_t PORT_BASE;

    bool SpellIsSame(const char *str1, const char *str2);                           // 字符串拼写比对
    bool DoubleInRange(const char *str1, const char *str2, const double range);     // 字符串数值比对
    void StrNCpySafe(char *dest, const char *src, size_t destSize);                 // 字符串N拷贝
    size_t StrSplit(char *src, const char *split, char **sonList, size_t listSize); // 字符串分割

    enum EStrRewrite // 字符串改写
    {                //
        SAME,        // 一致
        INIT,        // 初始化
        ALTER        // 变更
    };
    EStrRewrite StrRewrite(char *dest, const char *src, const double range, size_t destSize); // 字符串改写
    void StrReplaceAll(std::string &src, const char *replaceSrc, const char *replaceDst);     // 字符串替换
    void StringEscape(std::string &str);                                                      // 字符串转义
    void StringRemoveEscape(std::string &str);                                                // 字符串去转义
    void StringEscapeURL(std::string &url);                                                   // 字符串URL编码
    void StringEscapeList(std::string &str);                                                  // 序列化

    bool ChIsHex(const char ch);                                    // 十六进制字符判定
    uint8_t ChToHex(const char ch);                                 // 十六进制字符转数值
    char HexToChUp(uint8_t halfByte);                               // 数值转十六进制字符大写
    char HexToChLow(uint8_t halfByte);                              // 数值转十六进制字符小写
    size_t StrToHex(uint8_t *hex, size_t hexSize, const char *str); // 字符串转十六进制数组

    uint64_t GetTimeStamp(); // 获取时间戳(ms)

    int GetAdapterName(const char *strPeerIP, char *adapterName); // 根据对端IP获取适配器名称

    struct SAdapterInfo                  // 适配器信息
    {                                    //
        char name[IFNAMSIZ];             // 名称
        uint8_t mac[IFHWADDRLEN];        // MAC地址
        char ip[INET_ADDRSTRLEN];        // IP地址
        char broadcast[INET_ADDRSTRLEN]; // 广播地址
        char subMask[INET_ADDRSTRLEN];   // 子网掩码
    };
    size_t GetAdapterInfo(SAdapterInfo **ifInfo); // 获取适配器信息

    uint8_t CheckSumAdd(uint8_t *src, size_t srcLen);            // 校验和加
    uint8_t CheckSumXor(uint8_t *src, size_t srcLen);            // 校验和异或
    uint16_t CheckSumCRC16(uint8_t *src, size_t srcLen);         // 校验和CRC16
    std::string MD5Compute(const uint8_t *data, size_t dataLen); // MD5

    std::string Base64Encode(const uint8_t *dataToEncode, size_t dataLen); // Base64编码
} // namespace NSBaseTool

#endif