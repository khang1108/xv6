#include "kernel/types.h"
#include "user/user.h"

int 
main(void){
    //Lấy pid qua system call thông thường
    int pid = getpid();
    //Lấy pid nhanh thông qua usyscall
    int upid = ugetpid();

    printf("getpid()  = %d\n", pid);
    printf("ugetpid() = %d\n", upid);

    if (pid != upid){
        printf("ERROR: pid and upid unmatched!\n");
        exit(1);
    }

    printf("PID matched: %d\n", pid);
    exit(0);
}