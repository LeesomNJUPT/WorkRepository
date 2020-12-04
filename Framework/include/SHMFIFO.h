#ifndef __SHMFIFO_H__
#define __SHMFIFO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "global.h"

// 消息长度
#define MSG_SZ 20

// 消息结构体
struct mymsg
{
  long int mtype;
  char mtext[MSG_SZ];
};

// 头信息结构体
typedef struct shm_head
{
  int rd_idx; // 读索引
  int wr_idx; // 写索引
  int ahead;  // 写领先读计数
  int blocks; // 块总数
  int blksz;  // 块大小
} shm_head;

// 消息队列&共享内存&信号量结构体
typedef struct shm_fifo
{
  shm_head *p_head; // 头信息结构体指针
  void *p_payload;  // 负载起始地址
  int msgid;        // 消息队列描述符
  int shmid;        // 共享内存描述符
  int sem_mutex;    // 互斥信号量描述符
} shm_fifo;

class SHMFIFOBase
{
public:
  SHMFIFOBase();
  virtual ~SHMFIFOBase();

  // 初始化共享内存&信号量
  int shmfifo_init(int key, int blocks, int blksz, bool creat);
  // 写数据含时间戳
  int shmfifo_write(const void *buf, int write_index);
  // 读数据含时间戳
  int shmfifo_read(void *buf, int read_index);
  // 读配置数据含时间戳
  int shmfifo_cfg_write(const void *buf, int write_index);
  // 写配置数据含时间戳
  int shmfifo_cfg_read(void *buf, int read_index);
  // 写数据
  int shmfifo_simple_write(const void *buf, int write_index);
  // 读数据
  int shmfifo_simple_read(void *buf, int read_index);
  // 销毁共享内存&信号量
  void shmfifo_destroy(bool destroy);

  // 初始化消息队列
  int msgque_init(int key, bool creat);
  // 发送消息
  int msgque_send(const void *buf, unsigned long msgsz, bool wait);
  // 接收消息
  int msgque_recv(void *buf, unsigned long msgsz, long msgtype, bool wait);
  // 销毁消息队列
  void msgque_destroy();

private:
  shm_fifo shm;

  // P操作
  void Operator_V(int sem_id);
  // V操作
  void Operator_P(int sem_id);
};

#endif // __SHMFIFO_H__
