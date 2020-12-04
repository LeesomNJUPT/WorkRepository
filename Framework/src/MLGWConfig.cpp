#include "../include/MLGWConfig.h"
CMLGWConfig *CMLGWConfig::pStaticConfig = NULL;

CMLGWConfig::CMLGWConfig(char *argInterface, uint32_t localPort, char *argGatewayIP)
{
    strncpy(interface, argInterface, NETWORK_INTERFACE_NAME_LENGTH);
    interface[NETWORK_INTERFACE_NAME_LENGTH - 1] = '\0';
    strncpy(IP, argGatewayIP, INET_ADDRSTRLEN);
    IP[INET_ADDRSTRLEN - 1] = '\0';
    memset(mac, 0, MAC_LENGTH);
    configState = NONE;
    udp.Initialize(interface, localPort, UDPRecvData);

    pStaticConfig = this;
}

CMLGWConfig::~CMLGWConfig()
{
}

int CMLGWConfig::ConfigMorelinks()
{
    configState = ML_SEND;
    char sendData[] = "Morelinks";
    udp.PushToWriteList(IP, CONFIG_PORT, (uint8_t *)sendData, strlen(sendData));
    usleep(REACT_TIMEOUT * 1000);
    int ret;
    if (ML_RECV == configState)
        ret = RET_OK;
    else if (ML_SEND == configState)
        ret = RET_TIMEOUT;
    else
        ret = RET_ERROR;

    return ret;
}

int CMLGWConfig::ConfigMAC()
{
    ConfigMorelinks();
    if (ML_RECV != configState)
    {
        configState = NONE;
        return RET_ERROR;
    }
    configState = MAC_SEND;
    char sendData[] = "AT+MAC\r";
    udp.PushToWriteList(IP, CONFIG_PORT, (uint8_t *)sendData, strlen(sendData));
    usleep(REACT_TIMEOUT * 1000);
    int ret;
    if (MAC_RECV == configState)
        ret = RET_OK;
    else if (MAC_SEND == configState)
        ret = RET_TIMEOUT;
    else
        ret = RET_ERROR;

    configState = NONE;
    return ret;
}

int CMLGWConfig::ConfigUART(long baudRate, int dataBit, int stopBit, int parity)
{
    ConfigMorelinks();
    if (ML_RECV != configState)
    {
        configState = NONE;
        return RET_ERROR;
    }
    configState = UART_SEND;

    char sendData[256] = {0};
    char strParity[16] = {0};
    switch (parity)
    {
    case CHK_NONE:
        sprintf(strParity, "NONE");
        break;
    case CHK_EVEN:
        sprintf(strParity, "EVEN");
        break;
    case CHK_ODD:
        sprintf(strParity, "ODD");
        break;
    case CHK_MASK:
        sprintf(strParity, "MASK");
        break;
    case CHK_SPACE:
        sprintf(strParity, "SPACE");
        break;
    default:
    {
        configState = NONE;
        return RET_ERROR;
    }
    break;
    }
    sprintf(sendData, "AT+UART=%ld,%d,%d,%s,NFC\r\n",
            baudRate, dataBit, stopBit, strParity);

    udp.PushToWriteList(IP, CONFIG_PORT, (uint8_t *)sendData, strlen(sendData));
    usleep(REACT_TIMEOUT * 1000);
    int ret;
    if (UART_RECV == configState)
        ret = RET_OK;
    else if (UART_SEND == configState)
        ret = RET_TIMEOUT;
    else
        ret = RET_ERROR;

    configState = NONE;
    return ret;
}

int CMLGWConfig::ConfigSOCK(int protocol, char *IP, long port)
{
    ConfigMorelinks();
    if (ML_RECV != configState)
    {
        configState = NONE;
        return RET_ERROR;
    }
    configState = SOCK_SEND;

    char sendData[256] = {0};
    char strProtocol[16] = {0};
    switch (protocol)
    {
    case PRT_TCPS:
        sprintf(strProtocol, "TCPS");
        break;
    case PRT_TCPC:
        sprintf(strProtocol, "TCPC");
        break;
    case PRT_UDPS:
        sprintf(strProtocol, "UDPS");
        break;
    case PRT_UDPC:
        sprintf(strProtocol, "UDPC");
        break;
    case PRT_HTPC:
        sprintf(strProtocol, "HTPC");
        break;
    default:
    {
        configState = NONE;
        return RET_ERROR;
    }
    break;
    }
    sprintf(sendData, "AT+SOCK=%s,%s,%ld\r",
            strProtocol, IP, port);

    udp.PushToWriteList(IP, CONFIG_PORT, (uint8_t *)sendData, strlen(sendData));
    usleep(REACT_TIMEOUT * 1000);
    int ret;
    if (SOCK_RECV == configState)
        ret = RET_OK;
    else if (SOCK_SEND == configState)
        ret = RET_TIMEOUT;
    else
        ret = RET_ERROR;

    configState = NONE;
    return ret;
}

int CMLGWConfig::ConfigReboot()
{
    ConfigMorelinks();
    if (ML_RECV != configState)
    {
        configState = NONE;
        return RET_ERROR;
    }
    configState = REBOOT_SEND;
    char sendData[] = "AT+Z\r";
    udp.PushToWriteList(IP, CONFIG_PORT, (uint8_t *)sendData, strlen(sendData));
    usleep(REACT_TIMEOUT * 1000);
    int ret;
    if (REBOOT_RECV == configState)
        ret = RET_OK;
    else if (REBOOT_SEND == configState)
        ret = RET_TIMEOUT;
    else
        ret = RET_ERROR;

    configState = NONE;
    return ret;
}

int CMLGWConfig::GetMAC(char *macBuf)
{
    if (!strcasecmp(mac, ""))
        return RET_ERROR;

    strncpy(macBuf, mac, MAC_LENGTH);
    return RET_OK;
}

int CMLGWConfig::ResponseHandle(char *IP, char *data, size_t dataLen)
{
    switch (configState)
    {
    case ML_SEND:
        if (strstr(data, IP))
            configState = ML_RECV;
        break;
    case MAC_SEND:
        if (strstr(data, "+OK"))
        {
            configState = MAC_RECV;
            char *macHead = strstr(data, "=") + 1;
            for (int macIndex = 0; macIndex < 12; macIndex++)
            {
                mac[macIndex] = macHead[macIndex];
                if (('A' <= mac[macIndex]) &&
                    (mac[macIndex] <= 'Z'))
                    mac[macIndex] = mac[macIndex] + 0x20;
            }
        }
        break;
    case UART_SEND:
        if (strstr(data, "+OK"))
            configState = UART_RECV;
        break;
    case SOCK_SEND:
        if (strstr(data, "+OK"))
            configState = SOCK_RECV;
        break;
    case REBOOT_SEND:
        if (strstr(data, "+OK"))
            configState = REBOOT_RECV;
        break;
    default:
        break;
    }

    return 0;
}

int CMLGWConfig::UDPRecvData(TransferData *data, int type)
{
    switch (type)
    {
    case NOTIFY:
    {
        pStaticConfig->ResponseHandle((char *)data->param2, (char *)data->Data, data->param3);
    }
    break;
    case NETWORKSTATUS:
    {
    }
    break;
    default:
        break;
    }

    return 0;
}