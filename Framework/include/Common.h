#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>

#include "global.h"
#include "jsoncpp/json.h"

using namespace std;

#define BLOCKSIZE 512
// #define BUFFERSIZE 8192
#define CRCBYTE_BIT_LEN 8
#define RANDMAX 100

typedef struct
{
  char IPv4[INET_ADDRSTRLEN];
  char IPv6[INET6_ADDRSTRLEN];
  unsigned char macaddr[6];
  char BoradCastAddr[INET_ADDRSTRLEN];
} EquipmentInfo, *pEquipmentInfo;

class Common
{
public:
  Common();
  virtual ~Common();

  long CreateSendData(unsigned char *inByte, int inLen, unsigned char *outByte, unsigned char *fun, int type);
  int Str2Hex(string str, unsigned char *data);
  unsigned char HexChar(unsigned char c);
  char *GetMAC(char *Equipment);
  char *GetMAC();
  char *GetTimeStamp();
  long GetTimeStamp2Long();
  EquipmentInfo *GetNetwork(char *Equipment);
  void *MEM_Remalloc(void **Org, size_t OrgSize, size_t NewSize, size_t Pos = 0, size_t size = 0);
  bool isEqualMac(unsigned char *Org, unsigned char *Dst);
  bool isEqualIP(unsigned char *Org, unsigned char *Dst);
  unsigned char *GetCheckSum(unsigned char *data, int Start = 0, int End = 0, int Length = 0);
  void strsplit(char *src, const char *separator, char **dest, int *num);
  unsigned short CRC16(unsigned char *buf, int buf_len);
  int getpeermac(int sockfd, char *buf, char *arpDevName);
  unsigned char CRCNegativeCheckSum(unsigned char *buf, unsigned char buf_len);
  int GetRand();

private:
  bool GetNetwork(const vector<string> &vNetType, string &strip);
};

#endif