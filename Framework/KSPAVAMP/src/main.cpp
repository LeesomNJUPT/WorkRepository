#include <signal.h>

#include "../include/Driver.h"

void recvSignal(int sig)
{
    printf(DRINAME ".out received signal: %d, exit PID: %d.\n", sig, getpid());
    _exit(0);
}

int main(int argc, const char *argv[])
{
    int argMaxDriNum = -1, argMaxDevNum = -1;
    if (4 == argc)                        
    {
        argMaxDriNum = MAXSHMDRIVERSIZE;
        argMaxDevNum = MAXSHMDEVICESIZE;
    }
    else if (6 == argc)                        
    {
        argMaxDriNum = atoi(argv[4]);
        argMaxDevNum = atoi(argv[5]);
    }
    if (argMaxDriNum <= 0 || argMaxDevNum <= 0)
    {
        printf(DRINAME "::main(): Error argument.\n");
        return -1;
    }
    CDriver driver(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), argMaxDriNum, argMaxDevNum);  
    //类初始化  (int argDriIndex, int argDevIndex, int argDevNum,int argMaxDriNum, int argMaxDevNum)

    driver.GetOrderStart();
    driver.GetConfigOrderStart();
    driver.HandleStart();
    signal(SIGSEGV, recvSignal);

    while (1)
    {
        sleep(16);            
    }

    return 0;
}