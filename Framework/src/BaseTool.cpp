#include "../include/BaseTool.h"

namespace NSBaseTool
{

    const char SPLIT[] = "--------------------\n";

    const char DEVTYPE_RS485[] = "649T1323456527";
    const char DEVTYPE_RS232[] = "5601589977682";

    const int KEY_DRIINFO = 0x20190121;
    const int KEY_DEVINFO = 0x20190122;
    const int KEY_HOSTINFO = 0x20191115;
    const int KEY_DEVREQ = 0x20190119;
    const int KEY_DEVREP = 0x20190120;
    const int KEY_CFGREQ = 0x20191104;
    const int KEY_CFGREP = 0x20191105;

    const int SMGT_DRIINFO = 100;
    const int SMGT_DEVINFO = 1000;
    const int SMGT_HOSTINFO = 1;
    const int SMGT_DEVDATA = 1000;
    const int SMGT_CFGDATA = 50;

    const char DEVDATATYPE_STATUS[] = "status";
    const char DEVDATATYPE_SYNC[] = "sync";
    const char DEVDATATYPE_CHANGE[] = "change";
    const char DEVDATATYPE_REQ[] = "req";
    const char DEVDATATYPE_REFUN[] = "reFun";

    const char CFG_AUTOSEARCH[] = "autoSearch";
    const char CFG_MANUALSEARCH[] = "manualSearch";
    const char CFG_SEARCHREP[] = "searchRep";
    const char CFG_REPSEARCHEND[] = "repSearchEnd";
    const char CFG_RESTARTDRIVER[] = "restartDriver";
    const char CFG_CHANGETYPE[] = "changeType";
    const char CFG_CHANGETYPEREP[] = "changeTypeRep";
    const char CFG_MASTERPOS[] = "masterPos";
    const char CFG_MASTERPOSREP[] = "masterPosRep";
    const char CFG_SLAVEPOS[] = "slavePos";
    const char CFG_NEWDEVREP[] = "newDevRep";
    const char CFG_CONTROLLERREP[] = "controllerRep";
    const char CFG_CONFIGSTEPREP[] = "configStepRep";
    const char CFG_SETATTRIBUTE[] = "setAttribute";
    const char CFG_REPATTRIBUTE[] = "repAttribute";
    const size_t STRSIZE_ID = 16;
    const size_t STRSIZE_MAC = 16;
    const size_t STRSIZE_DEVTYPE = sizeof(SDevData::devType);
    const size_t STRSIZE_UUID = sizeof(SDevData::uuid);

    // #define STRSIZE_ID 16
    const uint64_t INTERVAL_VOID = 10 * 1000;     // us
    const uint64_t INTERVAL_READSHM = 10 * 1000;  // us
    const uint64_t INTERVAL_WRITESHM = 50 * 1000; // us

    const uint64_t DURATION_CONNECTED_VALIDITY = 60 * 1000; // ms

    const uint32_t PORT_DEFAULT = 8100;
    const uint32_t PORT_BASE = 50000;

    bool SpellIsSame(const char *str1, const char *str2)
    {
        if (0 == strcasecmp(str1, str2))
            return true;
        else
            return false;
    }

    bool DoubleInRange(const char *str1, const char *str2, const double range)
    {
        if (fabs(atof(str1) - atof(str2)) < range)
            return true;
        else
            return false;
    }

    void StrNCpySafe(char *dest, const char *src, size_t destSize)
    {
        strncpy(dest, src, destSize);
        dest[destSize - 1] = '\0';
    }

    EStrRewrite StrRewrite(char *dest, const char *src, const double range, size_t destSize)
    {
        EStrRewrite ret;

        if (SpellIsSame(dest, ""))
            ret = INIT;
        else if (0 == range && false == SpellIsSame(dest, src))
            ret = ALTER;
        else if (range > 0 && false == DoubleInRange(dest, src, range))
            ret = ALTER;
        else
            ret = SAME;
        StrNCpySafe(dest, src, destSize);

        return ret;
    }

    void StrReplaceAll(std::string &src, const char *replaceSrc, const char *replaceDst)
    {
        size_t pos = 0;
        size_t findPos = 0;
        while (src.npos != (pos = src.find(replaceSrc, findPos)))
        {
            src.replace(pos, strlen(replaceSrc), replaceDst);
            findPos = pos + strlen(replaceDst);
        }
    }

    void StringEscape(std::string &str)
    {
        Json::Value jsonBuf = str;
        str = jsonBuf.toStyledString();
        int left = str.find_first_of("{");
        int right = str.find_last_of("}");
        str = str.substr(left, right - left + 1);
    }

    void StringEscapeList(std::string &str)
    {
        Json::Value jsonBuf = str;
        str = jsonBuf.toStyledString();
        size_t left = str.find_first_of("{");
        size_t right = str.find_last_of("}");
        if (str.npos != left && str.npos != right)
            str = str.substr(left, right - left + 1);
    }

    void StringRemoveEscape(std::string &str)
    {
        size_t strLen = str.length();
        char *strBuf = new char[strLen + 1];
        memset(strBuf, 0x00, strLen + 1);
        strncpy(strBuf, str.c_str(), strLen);
        size_t writeIndex = 0;

        for (size_t strIndex = 0; strIndex < strLen; strIndex++)
            if ('\\' == strBuf[strIndex])
            {
                switch (strBuf[strIndex + 1])
                {
                case 'a':
                    strBuf[writeIndex++] = '\a';
                    break;
                case 'b':
                    strBuf[writeIndex++] = '\b';
                    break;
                case 'f':
                    strBuf[writeIndex++] = '\f';
                    break;
                case 'n':
                    strBuf[writeIndex++] = '\n';
                    break;
                case 'r':
                    strBuf[writeIndex++] = '\r';
                    break;
                case 't':
                    strBuf[writeIndex++] = '\t';
                    break;
                case 'v':
                    strBuf[writeIndex++] = '\v';
                    break;
                case '\\':
                    strBuf[writeIndex++] = '\\';
                    break;
                case '\'':
                    strBuf[writeIndex++] = '\'';
                    break;
                case '\"':
                    strBuf[writeIndex++] = '\"';
                    break;
                case '\?':
                    strBuf[writeIndex++] = '\?';
                    break;
                case '0':
                    strBuf[writeIndex++] = '\0';
                    break;
                default:
                    strBuf[writeIndex++] = strBuf[strIndex];
                    break;
                }
                switch (strBuf[strIndex + 1])
                {
                case 'a':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                case 'v':
                case '\\':
                case '\'':
                case '\"':
                case '\?':
                case '0':
                    strIndex++;
                    break;
                default:
                    break;
                }
            }
            else
                strBuf[writeIndex++] = strBuf[strIndex];
        strBuf[writeIndex] = '\0';

        str = strBuf;
        delete[] strBuf;
    }

    void StringEscapeURL(std::string &url)
    {
        std::string result;
        char cBuf;
        uint8_t byteBuf;
        for (size_t strIndex = 0; strIndex < url.size(); strIndex++)
        {
            cBuf = url[strIndex];
            byteBuf = cBuf;
            if (byteBuf < 0x80 && ' ' != cBuf)
            {
                result += cBuf;
            }
            else
            {
                result += '%';
                result += HexToChUp(byteBuf >> 4);
                result += HexToChUp(byteBuf & 0x0F);
            }
        }
        url = result;
    }

    bool ChIsHex(const char ch)
    {
        if (('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F'))
            return true;
        else
            return false;
    }

    uint8_t ChToHex(const char ch)
    {
        uint8_t ret = 0x00;

        if ('0' <= ch && ch <= '9')
            ret = ch - 0x30;
        else if ('a' <= ch && ch <= 'f')
            ret = ch - 0x57;
        else if ('A' <= ch && ch <= 'F')
            ret = ch - 0x37;

        return ret;
    }

    char HexToChUp(uint8_t halfByte)
    {
        char ch = ' ';
        if (halfByte < 16)
        {
            if (halfByte < 10)
                ch = 0x30 + halfByte;
            else
                ch = 0x37 + halfByte;
        }

        return ch;
    }

    char HexToChLow(uint8_t halfByte)
    {
        char ch = ' ';
        if (halfByte < 16)
        {
            if (halfByte < 10)
                ch = 0x30 + halfByte;
            else
                ch = 0x57 + halfByte;
        }

        return ch;
    }

    size_t StrToHex(uint8_t *hex, size_t hexSize, const char *str)
    {
        size_t strIndex = 0;
        size_t hexIndex = 0;

        for (; strIndex < strlen(str) && hexIndex < hexSize; strIndex = strIndex + 2)
            hex[hexIndex++] = (ChToHex(str[strIndex]) << 4) + ChToHex(str[strIndex + 1]);
        return hexIndex;
    }

    size_t StrSplit(char *src, const char *split, char **sonList, size_t listSize)
    {
        if (NULL == src || 0 == strlen(src) ||
            NULL == split || 0 == strlen(split))
            return 0;
        char *pSon = NULL;
        char *srcLeft = NULL;
        size_t count = 0;
        pSon = strtok_r(src, split, &srcLeft);
        while (pSon)
        {
            if (count < listSize)
                sonList[count++] = pSon;
            else
                break;
            pSon = strtok_r(NULL, split, &srcLeft);
        }

        return count;
    }

    uint64_t GetTimeStamp()
    {
        struct timeval ts;
        gettimeofday(&ts, NULL);
        return ts.tv_sec * 1000 + ts.tv_usec / 1000;
    }

    int GetAdapterName(const char *strPeerIP, char *adapterName)
    {
        int ret = -1;
        SAdapterInfo *adapterInfo = NULL;
        size_t aptNum = GetAdapterInfo(&adapterInfo);
        for (size_t aptIndex = 0; aptIndex < aptNum; aptIndex++)
            if ((inet_addr(adapterInfo[aptIndex].ip) & inet_addr(adapterInfo[aptIndex].subMask)) ==
                (inet_addr(strPeerIP) & inet_addr(adapterInfo[aptIndex].subMask)))
            {
                strncpy(adapterName, adapterInfo[aptIndex].name, IFNAMSIZ);
                ret = 0;
            }

        delete[] adapterInfo;
        adapterInfo = NULL;

        return ret;
    }

    size_t GetAdapterInfo(SAdapterInfo **ifInfo)
    {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            perror("GetAdapterInfo()::socket()");

            close(fd);
            return 0;
        }
        // 获取接口数量
        ifconf ifTotal;
        size_t ifRealNum = 0;
        size_t ifBufNum = 1;
        do
        {
            if (ifBufNum == ifRealNum)
            {
                ifBufNum *= 2;
            }
            ifreq ifBuf[ifBufNum] = {0};
            ifTotal.ifc_buf = (caddr_t)ifBuf;
            ifTotal.ifc_len = sizeof(ifreq) * ifBufNum;
            if (0 != ioctl(fd, SIOCGIFCONF, &ifTotal) || (ifRealNum = ifTotal.ifc_len / sizeof(ifreq)) <= 0)
            {
                printf("GetAdapterInfo(): Get interface number error.\n");

                close(fd);
                return 0;
            }
        } while (ifBufNum == ifRealNum);
        // 获取接口信息
        *ifInfo = new SAdapterInfo[ifRealNum];
        ifreq ifList[ifRealNum] = {0};
        ifTotal.ifc_buf = (caddr_t)ifList;
        ifTotal.ifc_len = sizeof(ifreq) * ifRealNum;
        if (0 == ioctl(fd, SIOCGIFCONF, &ifTotal))
        {
            for (int ifIndex = 0; ifIndex < ifRealNum; ifIndex++)
            {
                // ignore the interface that not up or not run
                if (0 != ioctl(fd, SIOCGIFFLAGS, &ifList[ifIndex]))
                {
                    printf("GetAdapterInfo()::Interface not up or not run: %s.\n", strerror(errno));
                    continue;
                }
                // get the mac of this interface
                if (0 == ioctl(fd, SIOCGIFHWADDR, &ifList[ifIndex]))
                    memcpy((*ifInfo)[ifIndex].mac, ifList[ifIndex].ifr_hwaddr.sa_data, IFHWADDRLEN);
                else
                {
                    printf("GetAdapterInfo()::Get interface mac: %s.\n", strerror(errno));
                    continue;
                }
                // get the ip of this interface
                if (0 == ioctl(fd, SIOCGIFADDR, &ifList[ifIndex]))
                    strncpy((*ifInfo)[ifIndex].ip, inet_ntoa(((sockaddr_in *)&ifList[ifIndex].ifr_addr)->sin_addr), INET_ADDRSTRLEN);
                else
                {
                    printf("GetAdapterInfo()::Get interface ip: %s.\n", strerror(errno));
                    continue;
                }
                // get the broadcast of this interface
                if (0 == ioctl(fd, SIOCGIFBRDADDR, &ifList[ifIndex]))
                    strncpy((*ifInfo)[ifIndex].broadcast, inet_ntoa(((sockaddr_in *)&ifList[ifIndex].ifr_broadaddr)->sin_addr), INET_ADDRSTRLEN);
                else
                {
                    printf("GetAdapterInfo()::Get interface broadcast: %s.\n", strerror(errno));
                    continue;
                }
                // get the subMask of this interface
                if (0 == ioctl(fd, SIOCGIFNETMASK, &ifList[ifIndex]))
                    strncpy((*ifInfo)[ifIndex].subMask, inet_ntoa(((sockaddr_in *)&ifList[ifIndex].ifr_netmask)->sin_addr), INET_ADDRSTRLEN);
                else
                {
                    printf("GetAdapterInfo()::Get interface subMask: %s.\n", strerror(errno));
                    continue;
                }
                // get the name of this interface
                strncpy((*ifInfo)[ifIndex].name, ifList[ifIndex].ifr_name, IFNAMSIZ);
            }
        }
        else
            printf("GetAdapterInfo()::Get interface name: %s.\n", strerror(errno));

        close(fd);
        return ifRealNum;
    }

    uint8_t CheckSumAdd(uint8_t *src, size_t srcLen)
    {
        uint8_t sum = 0x00;
        for (size_t index = 0; index < srcLen; index++)
            sum += src[index];
        return sum;
    }

    uint8_t CheckSumXor(uint8_t *src, size_t srcLen)
    {
        uint8_t sum = 0x00;
        for (size_t index = 0; index < srcLen; index++)
            sum = sum ^ src[index];
        return sum;
    }

    uint16_t CheckSumCRC16(uint8_t *src, size_t srcLen)
    {
        uint16_t crc16 = 0xFFFF;
        uint8_t shiftCount = 0;

        for (int srcIndex = 0; srcIndex < srcLen; srcIndex++)
        {
            crc16 = (((crc16 & 0x00FF) ^ src[srcIndex]) & 0x00FF) + (crc16 & 0xFF00);
            for (shiftCount = 0; shiftCount < 8; shiftCount++)
                if (crc16 & 0x0001)
                {
                    crc16 = crc16 >> 1;
                    crc16 = crc16 ^ 0xA001;
                }
                else
                    crc16 = crc16 >> 1;
        }

        return crc16;
    }

    std::string MD5Compute(const uint8_t *data, size_t dataLen)
    {
        uint8_t md5[MD5_DIGEST_LENGTH] = {0};
        MD5_CTX ctx;
        if (0 == MD5_Init(&ctx))
        {
            printf("MD5Compute(): MD5_Init() failed.\n");
            return "";
        }
        std::string ret;
        MD5_Update(&ctx, data, dataLen);
        MD5_Final(md5, &ctx);
        for (uint8_t md5Index = 0; md5Index < MD5_DIGEST_LENGTH; md5Index++)
        {
            ret += HexToChLow(md5[md5Index] >> 4);
            ret += HexToChLow(md5[md5Index] & 0x0F);
        }
        return ret;
    }

    const char Base64ConvTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "0123456789+/";
    std::string Base64Encode(const uint8_t *dataToEncode, size_t dataLen)
    {
        std::string ret;
        uint8_t encodeSrc[3] = {0};
        uint8_t encodeDest[4] = {0};

        for (size_t dataIndex = 0; dataIndex < dataLen && (dataIndex + 2) < dataLen; dataIndex = dataIndex + 3)
        {
            // 三字节一组
            encodeSrc[0] = dataToEncode[dataIndex];
            encodeSrc[1] = dataToEncode[dataIndex + 1];
            encodeSrc[2] = dataToEncode[dataIndex + 2];
            // 六位一组
            encodeDest[0] = (encodeSrc[0] & 0b11111100) >> 2;
            encodeDest[1] = ((encodeSrc[0] & 0b00000011) << 4) | ((encodeSrc[1] & 0b11110000) >> 4);
            encodeDest[2] = ((encodeSrc[1] & 0b00001111) << 2) | ((encodeSrc[2] & 0b11000000) >> 6);
            encodeDest[3] = encodeSrc[2] & 0b00111111;
            // 查表转换
            ret += Base64ConvTable[encodeDest[0]];
            ret += Base64ConvTable[encodeDest[1]];
            ret += Base64ConvTable[encodeDest[2]];
            ret += Base64ConvTable[encodeDest[3]];
        }
        uint8_t remainder = dataLen % 3;
        if (0 != remainder)
        {
            memset(encodeSrc, 0x00, 3);
            for (uint8_t remIndex = 0; remIndex < remainder; remIndex++)
            {
                encodeSrc[remIndex] = dataToEncode[dataLen - remainder + remIndex];
            }
            // 六位一组
            encodeDest[0] = (encodeSrc[0] & 0b11111100) >> 2;
            encodeDest[1] = ((encodeSrc[0] & 0b00000011) << 4) | ((encodeSrc[1] & 0b11110000) >> 4);
            encodeDest[2] = ((encodeSrc[1] & 0b00001111) << 2) | ((encodeSrc[2] & 0b11000000) >> 6);
            encodeDest[3] = encodeSrc[2] & 0b00111111;
            // 查表转换
            ret += Base64ConvTable[encodeDest[0]];
            ret += Base64ConvTable[encodeDest[1]];
            if (1 == remainder)
                ret += '=';
            else
                ret += Base64ConvTable[encodeDest[2]];
            ret += '=';
        }

        return ret;
    }
} // namespace NSBaseTool
