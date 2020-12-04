#include <sys/time.h>

#include "../include/SHMFIFO.h"

// 用于初始化信号量初值的联合体
union semun {
    int value;
};

SHMFIFOBase::SHMFIFOBase()
{
}

SHMFIFOBase::~SHMFIFOBase()
{
}

void SHMFIFOBase::Operator_P(int sem_id)
{
    struct sembuf buf[] = {{0, -1, SEM_UNDO}};
    semop(sem_id, buf, 1);
}

void SHMFIFOBase::Operator_V(int sem_id)
{
    struct sembuf buf[] = {{0, 1, SEM_UNDO}};
    semop(sem_id, buf, 1);
}

int SHMFIFOBase::shmfifo_init(int key, int blocks, int blksz, bool creat)
{
    int len = blocks * blksz + sizeof(shm_head);
    shm.shmid = shmget(key, len, 0);

    if (-1 == shm.shmid) // 打开失败
    {
        if (!creat)
        {
            perror("shmfifo_init error:");
            return -1;
        }
        shm.shmid = shmget(key, len, IPC_CREAT | 0666);
        if (-1 == shm.shmid) // 创建失败
        {
            perror("shmfifo_init error:");
            return -1;
        }

        // 创建成功 初始化属性
        shm.p_head = (shm_head *)shmat(shm.shmid, NULL, 0);
        memset(shm.p_head, 0, len);
        shm.p_head->rd_idx = 0;
        shm.p_head->wr_idx = 0;
        shm.p_head->ahead = 0;
        shm.p_head->blocks = blocks;
        shm.p_head->blksz = blksz;

        shm.p_payload = (void *)shm.p_head + sizeof(shm_head);

        shm.sem_mutex = semget(key, 1, IPC_CREAT | 0666);
        union semun su;
        su.value = 1;
        semctl(shm.sem_mutex, 0, SETVAL, su);
    }
    else // 共享内存存在
    {
        // 挂载共享内存
        shm.p_head = (shm_head *)shmat(shm.shmid, NULL, 0);

        shm.p_payload = (void *)shm.p_head + sizeof(shm_head);

        shm.sem_mutex = semget(key, 0, 0);
    }
    return 1;
}

int SHMFIFOBase::shmfifo_write(const void *buf, int write_index)
{
    Operator_P(shm.sem_mutex);
    int done_index = -1;
    void *write_place = NULL;
    // 寻址
    if (write_index < 0)
    {
        done_index = shm.p_head->wr_idx;
    }
    else if (write_index < shm.p_head->blocks)
    {
        done_index = write_index;
    }
    else
    {
        done_index = -1;
        Operator_V(shm.sem_mutex);
        return done_index;
    }
    write_place = shm.p_payload + done_index * shm.p_head->blksz;
    DevsData *dev = (DevsData *)write_place;
    // 写操作
    if (dev->data_timestamp == 0)
    {
        memcpy(write_place, buf, shm.p_head->blksz);
    }
    else
    {
        unsigned long realtime;
        struct timeval ts;
        gettimeofday(&ts, NULL);
        realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
        if ((dev->data_timestamp + dev->Timeout) < realtime)
        {
            memcpy(write_place, buf, shm.p_head->blksz);
        }
        else
        {
            done_index = -1;
        }
    }
    // 指针变化
    if (write_index < 0)
    {
        shm.p_head->wr_idx = (shm.p_head->wr_idx + 1) % (shm.p_head->blocks);
        if (0 == shm.p_head->wr_idx)
        {
            shm.p_head->ahead++;
        }
    }

    Operator_V(shm.sem_mutex);
    return done_index;
}

int SHMFIFOBase::shmfifo_read(void *buf, int read_index)
{
    Operator_P(shm.sem_mutex);
    int done_index = -1;
    void *read_place = NULL;
    // 顺序读时读指针不能超前于写指针
    if (read_index < 0)
    {
        if (shm.p_head->rd_idx >= (shm.p_head->wr_idx + shm.p_head->ahead * shm.p_head->blocks))
        {
            done_index = -1;
            Operator_V(shm.sem_mutex);
            return done_index;
        }
    }
    // 寻址
    if (read_index < 0)
    {
        done_index = shm.p_head->rd_idx;
    }
    else if (read_index < shm.p_head->blocks)
    {
        done_index = read_index;
    }
    else
    {
        done_index = -1;
        Operator_V(shm.sem_mutex);
        return done_index;
    }
    read_place = shm.p_payload + done_index * shm.p_head->blksz;
    DevsData *dev = (DevsData *)read_place;
    // 读操作
    if (dev->data_timestamp != 0)
    {
        if (read_index < 0)
        {
            memcpy(buf, read_place, shm.p_head->blksz);
            dev->data_timestamp = 0;
        }
        else
        {
            unsigned long realtime;
            struct timeval ts;
            gettimeofday(&ts, NULL);
            realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
            if ((dev->data_timestamp + dev->Timeout) > realtime)
            {
                memcpy(buf, read_place, shm.p_head->blksz);
                dev->data_timestamp = 0;
            }
            else
            {
                dev->data_timestamp = 0;
                done_index = -1;
            }
        }
    }
    else
    {
        done_index = -1;
    }
    // 指针变化
    if (read_index < 0)
    {
        shm.p_head->rd_idx = (shm.p_head->rd_idx + 1) % (shm.p_head->blocks);
        if (0 == shm.p_head->rd_idx)
        {
            shm.p_head->ahead--;
        }
    }

    Operator_V(shm.sem_mutex);
    return done_index;
}

int SHMFIFOBase::shmfifo_cfg_write(const void *buf, int write_index)
{
    Operator_P(shm.sem_mutex);
    int done_index = -1;
    void *write_place = NULL;
    // 寻址
    if (write_index < 0)
    {
        done_index = shm.p_head->wr_idx;
    }
    else if (write_index < shm.p_head->blocks)
    {
        done_index = write_index;
    }
    else
    {
        done_index = -1;
        Operator_V(shm.sem_mutex);
        return done_index;
    }
    write_place = shm.p_payload + done_index * shm.p_head->blksz;
    CfgDevsData *dev = (CfgDevsData *)write_place;
    // 写操作
    if (dev->data_timestamp == 0)
    {
        memcpy(write_place, buf, shm.p_head->blksz);
    }
    else
    {
        unsigned long realtime;
        struct timeval ts;
        gettimeofday(&ts, NULL);
        realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
        if ((dev->data_timestamp + dev->Timeout) < realtime)
        {
            memcpy(write_place, buf, shm.p_head->blksz);
        }
        else
        {
            done_index = -1;
        }
    }
    // 指针变化
    if (write_index < 0)
    {
        shm.p_head->wr_idx = (shm.p_head->wr_idx + 1) % (shm.p_head->blocks);
        if (0 == shm.p_head->wr_idx)
        {
            shm.p_head->ahead++;
        }
    }

    Operator_V(shm.sem_mutex);
    return done_index;
}

int SHMFIFOBase::shmfifo_cfg_read(void *buf, int read_index)
{
    Operator_P(shm.sem_mutex);
    int done_index = -1;
    void *read_place = NULL;
    // 顺序读时读指针不能超前于写指针
    if (read_index < 0)
    {
        if (shm.p_head->rd_idx >= (shm.p_head->wr_idx + shm.p_head->ahead * shm.p_head->blocks))
        {
            done_index = -1;
            Operator_V(shm.sem_mutex);
            return done_index;
        }
    }
    // 寻址
    if (read_index < 0)
    {
        done_index = shm.p_head->rd_idx;
    }
    else if (read_index < shm.p_head->blocks)
    {
        done_index = read_index;
    }
    else
    {
        done_index = -1;
        Operator_V(shm.sem_mutex);
        return done_index;
    }
    read_place = shm.p_payload + done_index * shm.p_head->blksz;
    CfgDevsData *dev = (CfgDevsData *)read_place;
    // 读操作
    if (dev->data_timestamp != 0)
    {
        if (read_index < 0)
        {
            memcpy(buf, read_place, shm.p_head->blksz);
            dev->data_timestamp = 0;
        }
        else
        {
            unsigned long realtime;
            struct timeval ts;
            gettimeofday(&ts, NULL);
            realtime = ts.tv_sec * 1000 + ts.tv_usec / 1000;
            if ((dev->data_timestamp + dev->Timeout) > realtime)
            {
                memcpy(buf, read_place, shm.p_head->blksz);
                dev->data_timestamp = 0;
            }
            else
            {
                dev->data_timestamp = 0;
                done_index = -1;
            }
        }
    }
    else
    {
        done_index = -1;
    }
    // 指针变化
    if (read_index < 0)
    {
        shm.p_head->rd_idx = (shm.p_head->rd_idx + 1) % (shm.p_head->blocks);
        if (0 == shm.p_head->rd_idx)
        {
            shm.p_head->ahead--;
        }
    }

    Operator_V(shm.sem_mutex);
    return done_index;
}

int SHMFIFOBase::shmfifo_simple_write(const void *buf, int write_index)
{
    Operator_P(shm.sem_mutex);
    int done_index = -1;
    void *write_place = NULL;
    // 寻址
    if (write_index < 0)
    {
        done_index = shm.p_head->wr_idx;
    }
    else if (write_index < shm.p_head->blocks)
    {
        done_index = write_index;
    }
    else
    {
        done_index = -1;
        Operator_V(shm.sem_mutex);
        return done_index;
    }
    write_place = shm.p_payload + done_index * shm.p_head->blksz;
    // 写操作
    memcpy(write_place, buf, shm.p_head->blksz);
    // 指针变化
    if (write_index < 0)
    {
        shm.p_head->wr_idx = (shm.p_head->wr_idx + 1) % (shm.p_head->blocks);
        if (0 == shm.p_head->wr_idx)
        {
            shm.p_head->ahead++;
        }
    }
    Operator_V(shm.sem_mutex);
    return done_index;
}

int SHMFIFOBase::shmfifo_simple_read(void *buf, int read_index)
{
    Operator_P(shm.sem_mutex);
    int done_index = -1;
    void *read_place = NULL;
    // 顺序读时读指针不能超前于写指针
    if (read_index < 0)
    {
        if (shm.p_head->rd_idx >= (shm.p_head->wr_idx + shm.p_head->ahead * shm.p_head->blocks))
        {
            done_index = -1;
            Operator_V(shm.sem_mutex);
            return done_index;
        }
    }
    // 寻址
    if (read_index < 0)
    {
        done_index = shm.p_head->rd_idx;
    }
    else if (read_index < shm.p_head->blocks)
    {
        done_index = read_index;
    }
    else
    {
        done_index = -1;
        Operator_V(shm.sem_mutex);
        return done_index;
    }
    read_place = shm.p_payload + done_index * shm.p_head->blksz;
    // 读操作
    memcpy(buf, read_place, shm.p_head->blksz);
    // 指针变化
    if (read_index < 0)
    {
        shm.p_head->rd_idx = (shm.p_head->rd_idx + 1) % (shm.p_head->blocks);
        if (0 == shm.p_head->rd_idx)
        {
            shm.p_head->ahead--;
        }
    }
    Operator_V(shm.sem_mutex);
    return done_index;
}

void SHMFIFOBase::shmfifo_destroy(bool destroy)
{
    shmdt(shm.p_head);
    // 销毁
    if (destroy)
    {
        shmctl(shm.shmid, IPC_RMID, 0);
        semctl(shm.sem_mutex, 0, IPC_RMID);
    }
}

int SHMFIFOBase::msgque_init(int key, bool creat)
{
    shm.msgid = msgget(key, 0);

    if (-1 == shm.msgid) // 打开失败
    {
        if (!creat)
        {
            perror("msgque_init error:");
            return -1;
        }
        shm.msgid = msgget(key, IPC_CREAT | 0666);
        if (-1 == shm.msgid) // 创建失败
        {
            perror("msgque_init error:");
            return -1;
        }
    }
    return 1;
}

int SHMFIFOBase::msgque_send(const void *buf, unsigned long msgsz, bool wait)
{
    int ret = 1;
    if (wait)
    {
        ret = msgsnd(shm.msgid, buf, msgsz, 0);
    }
    else
    {
        ret = msgsnd(shm.msgid, buf, msgsz, IPC_NOWAIT);
    }
    return ret;
}

int SHMFIFOBase::msgque_recv(void *buf, unsigned long msgsz, long msgtype, bool wait)
{
    int ret = 1;
    if (wait)
    {
        ret = msgrcv(shm.msgid, buf, msgsz, msgtype, 0);
    }
    else
    {
        ret = msgrcv(shm.msgid, buf, msgsz, msgtype, IPC_NOWAIT);
    }
    return ret;
}

void SHMFIFOBase::msgque_destroy()
{
    msgctl(shm.msgid, IPC_RMID, NULL);
}