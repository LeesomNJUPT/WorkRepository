#ifndef _QUEUE_H
#define _QUEUE_H

#include <string.h>
#include <pthread.h>
#include <vector>

template <typename T>
class CQueue
{
private:
    std::vector<T> element;
    pthread_mutex_t operateMutex;

public:
    CQueue();
    ~CQueue();

    bool WriteToQueue(T *pElemIn);
    bool ReadFromQueue(T *pElemOut);
};

template <typename T>
CQueue<T>::CQueue()
{
    pthread_mutex_init(&operateMutex, NULL);
}

template <typename T>
CQueue<T>::~CQueue()
{
    pthread_mutex_destroy(&operateMutex);
}

template <typename T>
bool CQueue<T>::WriteToQueue(T *pElemIn)
{
    if (NULL == pElemIn)
        return false;

    pthread_mutex_lock(&operateMutex);

    element.push_back(*pElemIn);

    pthread_mutex_unlock(&operateMutex);
    return true;
}

template <typename T>
bool CQueue<T>::ReadFromQueue(T *pElemOut)
{
    pthread_mutex_lock(&operateMutex);

    if (element.empty() || NULL == pElemOut)
    {
        pthread_mutex_unlock(&operateMutex);
        return false;
    }

    typename std::vector<T>::iterator it = element.begin();
    memcpy(pElemOut, &(*it), sizeof(T));
    element.erase(it);

    pthread_mutex_unlock(&operateMutex);
    return true;
}

#endif