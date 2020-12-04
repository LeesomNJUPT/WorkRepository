#ifndef _MLGW_CONFIG_H
#define _MLGW_CONFIG_H

#include "UDPBase.h"

// 名称长度
#define NETWORK_INTERFACE_NAME_LENGTH 32 // 网络接口名称长度
#define MAC_LENGTH 16                    // MAC地址长度
// 通信参数
#define CONFIG_PORT 48899 // 默认配置端口
#define REACT_TIMEOUT 200 // 响应超时(ms)
// 命令执行返回值
#define RET_OK 0       // 命令执行成功
#define RET_TIMEOUT -1 // 命令响应超时
#define RET_ERROR -2   // 命令执行错误
// 数据位
#define DATABIT_5 5 // 数据位 5
#define DATABIT_6 6 // 数据位 6
#define DATABIT_7 7 // 数据位 7
#define DATABIT_8 8 // 数据位 8
// 停止位
#define STOPBIT_1 1 // 停止位 1
#define STOPBIT_2 2 // 停止位 2
// 校验位
#define CHK_NONE 0  // 无校验位
#define CHK_EVEN 1  // 偶校验
#define CHK_ODD 2   // 奇校验
#define CHK_MASK 3  // 1校验
#define CHK_SPACE 4 // 0校验
// 协议
#define PRT_TCPS 0 // TCP server
#define PRT_TCPC 1 // TCP client
#define PRT_UDPS 2 // UDP server
#define PRT_UDPC 3 // UDP client
#define PRT_HTPC 4 // HTTP client

enum ESTATE      // 配置状态枚举
{                // *
    NONE,        // 无动作
    ML_SEND,     // 发送Morelinks
    ML_RECV,     // 接收Morelinks响应
    MAC_SEND,    // 发送MAC
    MAC_RECV,    // 接收MAC响应
    UART_SEND,   // 发送UART
    UART_RECV,   // 接收UART响应
    SOCK_SEND,   // 发送SOCK
    SOCK_RECV,   // 接收SOCK响应
    REBOOT_SEND, // 发送REBOOT
    REBOOT_RECV  // 接收REBOOT响应
};

class CMLGWConfig
{
private:
    char interface[NETWORK_INTERFACE_NAME_LENGTH]; // 网络接口名称
    char IP[INET_ADDRSTRLEN];                      // IP地址
    char mac[MAC_LENGTH];                          // MAC地址
    ESTATE configState;                            // 配置状态
    UDPBase udp;                                   // UDP通信基类

    int ConfigMorelinks();                                    // 命令 Morelinks
    int ResponseHandle(char *IP, char *data, size_t dataLen); // 响应处理

    static CMLGWConfig *pStaticConfig;                    // 静态指针
    static int UDPRecvData(TransferData *data, int type); // UDP接收数据回调处理

public:
    CMLGWConfig(char *argInterface, uint32_t localPort, char *argGatewayIP);
    ~CMLGWConfig();

    int GetMAC(char *macBuf);                                            // 获取MAC地址
    int ConfigMAC();                                                     // 命令 MAC
    int ConfigUART(long baudRate, int dataBit, int stopBit, int parity); // 命令 UART
    int ConfigSOCK(int protocol, char *IP, long port);                   // 命令 SOCK
    int ConfigReboot();                                                  // 命令 Reboot
};

#endif