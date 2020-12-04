#include "../include/Common.h"

Common::Common()
{
}

Common::~Common()
{
}

int Common::Str2Hex(string str, unsigned char *data)
{
    int t, t1;
    int rlen = 0, len = str.length();
    //data.SetSize(len/2);
    for (int i = 0; i < len;)
    {
        char l, h = str[i];
        if (h == ' ')
        {
            i++;
            continue;
        }
        i++;
        if (i >= len)
            break;
        l = str[i];
        t = HexChar(h);
        t1 = HexChar(l);
        if ((t == 16) || (t1 == 16))
            break;
        else
            t = t * 16 + t1;
        i++;
        data[rlen] = (unsigned char)t;
        rlen++;
    }
    return rlen;
}

unsigned char Common::HexChar(unsigned char c)
{
    if ((c >= '0') && (c <= '9'))
        return c - 0x30;
    else if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 10;
    else if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 10;
    else
        return 0x10;
}

char *Common::GetMAC(char *Equipment)
{
    char *macAddr = NULL;
    macAddr = (char *)calloc(1, 128);
    vector<string> vs; //预先定义了几种可能的网卡类型
    vs.push_back(Equipment);
    string str;
    if ((GetNetwork(vs, str)))
    {
        memcpy(macAddr, str.c_str(), str.length());
        macAddr[str.length()] = '\0';
    }

    return macAddr;
}

char *Common::GetMAC()
{
    char *macAddr = NULL;
    macAddr = (char *)calloc(1, 128);
    vector<string> vs; //预先定义了几种可能的网卡类型
    vs.push_back("eth");
    vs.push_back("wlp3s");
    vs.push_back("lo");
    string str;
    if ((GetNetwork(vs, str)))
    {
        memcpy(macAddr, str.c_str(), str.length());
        macAddr[str.length()] = '\0';
    }

    return macAddr;
}

bool Common::GetNetwork(const vector<string> &vNetType, string &strip)
{
    for (size_t i = 0; i < vNetType.size(); i++)
    {
        for (char c = '0'; c <= '9'; ++c)
        {
            string strDevice = vNetType[i] + c; //根据网卡类型，遍历设备如eth0，eth1
            int fd;
            struct ifreq ifr;
            //使用UDP协议建立无连接的服务
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            strcpy(ifr.ifr_name, strDevice.c_str());
            //获取IP地址
            if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
            {
                close(fd);
                continue;
            }

            if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
            {
                close(fd);
                continue;
            }
            unsigned char macaddr[6];
            memcpy(macaddr, ifr.ifr_hwaddr.sa_data, 6);
            for (int k = 0; k < 6; k++)
            {
                char macbit[2];
                sprintf(macbit, "%02x", macaddr[k]);
                strip.append(macbit, 2);
                if (k < 5)
                    strip.append(":", 1);
            }
            // 将一个IP转换成一个互联网标准点分格式的字符串
            // printf("IP Address: %s\r\n", inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr));

            if (!strip.empty())
            {
                close(fd);
                return true;
            }
        }
    }
    return false;
}

EquipmentInfo *Common::GetNetwork(char *Equipment)
{
    EquipmentInfo *EqInfo = new EquipmentInfo;

    memset(EqInfo, 0, sizeof(EquipmentInfo));
    int fd;
    struct ifreq ifr;
    //使用UDP协议建立无连接的服务
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_name, Equipment);
    //获取IP地址
    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
    {
        close(fd);
        return NULL;
    }

    // copy IPv4 Address
    sprintf(EqInfo->IPv4, "%s", inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr));
    // sprintf(EqInfo->IPv4,"%02x", ifr.ifr_addr.sa_data);
    // copy IPv4 Address
    sprintf(EqInfo->IPv6, "%s", inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr));

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        close(fd);
        return NULL;
    }

    // copy MAC Address
    memcpy(EqInfo->macaddr, ifr.ifr_hwaddr.sa_data, 6);

    // copy Broadcast Address
    if (ioctl(fd, SIOCGIFBRDADDR, &ifr) < 0)
    {
        close(fd);
        return NULL;
    }

    sprintf(EqInfo->BoradCastAddr, "%s", inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_broadaddr))->sin_addr));

    return EqInfo;
}

char *Common::GetTimeStamp()
{
    char *timeStamp = NULL;
    timeStamp = (char *)calloc(1, 32);
    struct timeval ts;
    if (gettimeofday(&ts, NULL) == 0)
    {
        sprintf(timeStamp, "%ld\0", (ts.tv_sec * 1000 + ts.tv_usec / 1000));
    }

    // printf("\r\n%s\r\n",timeStamp);

    return timeStamp;
}

long Common::GetTimeStamp2Long()
{
    struct timeval ts;
    if (gettimeofday(&ts, NULL) == 0)
    {
        return (ts.tv_sec * 1000 + ts.tv_usec / 1000);
    }
    else
    {
        return -1;
    }
}

void *Common::MEM_Remalloc(void **Org, size_t OrgSize, size_t NewSize, size_t Pos, size_t size)
{
    void *more_memory = NULL;
    if (NewSize > 0)
    {
        more_memory = malloc(NewSize);
        memset(more_memory, 0, NewSize);
    }

    if (Org != NULL)
    {
        if (*Org != NULL)
        {
            /* code */
            if (NewSize > 0)
            {
                if (NewSize >= OrgSize)
                {
                    memcpy(more_memory, *Org, OrgSize);
                }
                else
                {
                    /* code */
                    if (Pos == 0) // frist
                    {
                        memcpy(more_memory, *Org + size, NewSize);
                    }
                    else if ((Pos + 1) * size == OrgSize) // end
                    {
                        memcpy(more_memory, *Org, NewSize);
                    }
                    else
                    {
                        /* code */
                        int position = Pos * size;
                        memcpy(more_memory, *Org, position);
                        memcpy(more_memory + position, *Org + (Pos + 1) * size, (OrgSize - (Pos + 1) * size));
                    }
                }
            }
            else
            {
            }

            free(*Org);
            *Org = NULL;
            // printf("Common MEM_Remalloc Re-Malloc 2 Memory: %p,%p,%d,%d,%d\r\n", more_memory, *Org, OrgSize, NewSize, sizeof(*(ModuleInfo *)more_memory));
        }
    }
    // printf("Common MEM_Remalloc: %p,%d,%d,%d\r\n", more_memory, OrgSize, NewSize, sizeof(*(ModuleInfo *)more_memory));
    return more_memory;
}

bool Common::isEqualMac(unsigned char *Org, unsigned char *Dst)
{
    bool Result = true;
    if (Dst != NULL && Org != NULL)
    {
        for (int i = 0; i < 6; i++)
        {
            if (Org[i] == Dst[i])
            {
                Result = true;
            }
            else
            {
                /* code */
                Result = false;
                break;
            }
        }
        /* code */
    }
    else
    {
        return false;
    }

    return Result;
}

bool Common::isEqualIP(unsigned char *Org, unsigned char *Dst)
{
    bool Result = true;
    if (Dst != NULL && Org != NULL)
    {
        for (int i = 0; i < 4; i++)
        {
            if (Org[i] == Dst[i])
            {
                Result = true;
            }
            else
            {
                /* code */
                Result = false;
                break;
            }
        }
        /* code */
    }
    else
    {
        return false;
    }

    return Result;
}

unsigned char *Common::GetCheckSum(unsigned char *data, int Start, int End, int Length)
{
    // int ArrLen = 0;
    // GET_ARRAY_LEN(data,ArrLen);
    // printf("Common::GetCheckSum Start: %d, End: %d, Length: %d\r\n", Start, End, Length);

    if (Start < 0 || Start > (Length - 1) || End > (Length - 1))
        return NULL;

    int Count = End - Start + 1;
    int Step = 0;

    unsigned char *Result = (unsigned char *)malloc(1);
    memset(Result, 0, sizeof(unsigned char));

    while (Step < Count)
    {
        /* code */
        // printf("Common::GetCheckSum Count: %d, Step: %d, data: 0x%02x\r\n", Count, Step, data[Start + Step]);
        *Result += data[Start + Step];
        *Result &= 0xff;
        Step++;
    };
    // printf("Common::GetCheckSum Count: %d, Step: %d, Result: 0x%02x\r\n", Count, Step, *Result);
    return Result;
}

long Common::CreateSendData(unsigned char *inByte, int inLen, unsigned char *outByte, unsigned char *fun, int type)
{
    long len = 0;

    outByte[0] = 0x55;
    len++;

    memcpy(outByte + len, (const char *)&fun, sizeof(fun));
    // printf("\r\nfun: %02x\r\n",fun);
    len++;
    // printf("\r\len: %d\r\n",len);

    int iXmlSize = inLen;

    int iNSize = htonl(iXmlSize);

    memcpy(outByte + len, (const char *)&iNSize, sizeof(iNSize));
    len += sizeof(iNSize);
    // printf("\r\len: %d\r\n",len);
    // printf("\r\nCreateSendData:\r\n");

    if (iXmlSize > 0)
    {
        if (type == _HEX)
        {
            for (int i = 0; i < iXmlSize; i++)
            {
                outByte[len + i] = *inByte + i;
            }
        }
        else
        {
            memcpy(outByte + len, inByte, iXmlSize);
        }

        len += iXmlSize;
    }

    outByte[len] = 0xAA;
    len++;

    return len;
}

void Common::strsplit(char *src, const char *separator, char **dest, int *num)
{
    char *pNext;
    int count = 0;
    if (src == NULL || strlen(src) == 0)
        return;
    if (separator == NULL || strlen(separator) == 0)
        return;
    pNext = strtok(src, separator);
    while (pNext != NULL)
    {
        *dest++ = pNext;
        ++count;
        pNext = strtok(NULL, separator);
    }
    *num = count;
}

unsigned short Common::CRC16(unsigned char *buf, int buf_len)
{
    unsigned short crc16 = 0xFFFF;
    int move_count = 0;

    for (int i = 0; i < buf_len; i++)
    {
        crc16 = (((crc16 & 0x00FF) ^ buf[i]) & 0x00FF) + (crc16 & 0xFF00);
        for (move_count = 0; move_count < CRCBYTE_BIT_LEN; move_count++)
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

int Common::getpeermac(int sockfd, char *buf, char *arpDevName)
{
    int ret = -1;
    struct arpreq arPreq;
    struct sockaddr_in dstadd_in;
    socklen_t len = sizeof(sockaddr_in);
    memset(&arPreq, 0, sizeof(arpreq));
    memset(&dstadd_in, 0, sizeof(struct sockaddr_in));
    if (getpeername(sockfd, (struct sockaddr *)&dstadd_in, &len) < 0)
        printf("getpeername()\r\n");
    else
    {
        memcpy(&arPreq.arp_pa, &dstadd_in, sizeof(struct sockaddr_in));
        strcpy(arPreq.arp_dev,arpDevName);
        arPreq.arp_pa.sa_family = AF_INET;
        arPreq.arp_ha.sa_family = AF_UNSPEC;
        if (ioctl(sockfd, SIOCGARP, &arPreq) < 0)
            printf("ioctl SIOCGARP\r\n");
        else
        {
            unsigned char *ptr = (unsigned char *)arPreq.arp_ha.sa_data;
            ret = sprintf(buf, "%02x%02x%02x%02x%02x%02x", *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4), *(ptr + 5));
        }
    }
    return ret;
}

unsigned char Common::CRCNegativeCheckSum(unsigned char *buf,unsigned char buf_len)
{
 int i;
 int sum = 0;
 unsigned char CRCValue;
 for(i = 0; i < buf_len; i++) {
  sum += *(buf+i);
 }
 CRCValue = -sum;
 return CRCValue;
}

int Common::GetRand()
{
    srand( (unsigned)time( NULL ) );// srand()函数产生一个以当前时间开始的随机种子
    return rand()%RANDMAX;//RANDMAX为最大值，其随机域为0~MAX-1
}