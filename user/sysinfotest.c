#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

void sinfo(struct sysinfo *info) {
    if (sysinfo(info) < 0) {
        printf("FAIL: sysinfo failed");
        exit(1);
    }
}

//
// use sbrk() to count how many free physical memory pages there are.
//
int countfree() {
    uint64 sz0 = (uint64)sbrk(0);
    struct sysinfo info;
    int n = 0;

    while (1) {
        if ((uint64)sbrk(PGSIZE) == 0xffffffffffffffff) {
            break;
        }
        n += PGSIZE;
    }
    sinfo(&info);
    if (info.freemem != 0) {
        printf("FAIL: there is no free mem, but sysinfo.freemem=%ld\n",
               info.freemem);
        exit(1);
    }
    sbrk(-((uint64)sbrk(0) - sz0));
    return n;
}

void testmem() {
    struct sysinfo info;
    uint64 n = countfree();

    sinfo(&info);

    if (info.freemem != n) {
        printf("FAIL: free mem %ld (bytes) instead of %ld\n", info.freemem, n);
        exit(1);
    }

    if ((uint64)sbrk(PGSIZE) == 0xffffffffffffffff) {
        printf("sbrk failed");
        exit(1);
    }

    sinfo(&info);

    if (info.freemem != n - PGSIZE) {
        printf("FAIL: free mem %ld (bytes) instead of %ld\n", n - PGSIZE,
               info.freemem);
        exit(1);
    }

    if ((uint64)sbrk(-PGSIZE) == 0xffffffffffffffff) {
        printf("sbrk failed");
        exit(1);
    }

    sinfo(&info);

    if (info.freemem != n) {
        printf("FAIL: free mem %ld (bytes) instead of %ld\n", n, info.freemem);
        exit(1);
    }
}

void testcall() {
    struct sysinfo info;

    if (sysinfo(&info) < 0) {
        printf("FAIL: sysinfo failed\n");
        exit(1);
    }

    if (sysinfo((struct sysinfo *)0xeaeb0b5b00002f5e) != 0xffffffffffffffff) {
        printf("FAIL: sysinfo succeeded with bad argument\n");
        exit(1);
    }
}

void testproc() {
    struct sysinfo info;
    uint64 nproc;
    int status;
    int pid;

    sinfo(&info);
    nproc = info.nproc;

    pid = fork();
    if (pid < 0) {
        printf("sysinfotest: fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        sinfo(&info);
        if (info.nproc != nproc + 1) {
            printf("sysinfotest: FAIL nproc is %ld instead of %ld\n",
                   info.nproc, nproc + 1);
            exit(1);
        }
        exit(0);
    }
    wait(&status);

    // Kiểm tra mã thoát của tiến trình con:
    // Đảm bảo tiến trình con thực thi thành công (exit code == 0).
    // Nếu con gặp lỗi và gọi exit(1), cha bắt buộc phải báo lỗi và dừng kiểm thử ngay lập tức,
    // tránh tình trạng dương tính giả (false positive) khi con thất bại mà cha vẫn báo OK.
    if (status != 0) {
        printf("sysinfotest: FAIL child process failed with status %d\n", status);
        exit(1);
    }

    sinfo(&info);
    if (info.nproc != nproc) {
        printf("sysinfotest: FAIL nproc is %ld instead of %ld\n", info.nproc,
               nproc);
        exit(1);
    }
}

void testbad() {
    int pid = fork();
    int xstatus;

    if (pid < 0) {
        printf("sysinfotest: fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        sinfo(0x0);
        exit(0);
    }
    wait(&xstatus);
    if (xstatus == -1) // kernel killed child?
        exit(0);
    else {
        printf("sysinfotest: testbad succeeded %d\n", xstatus);
        exit(xstatus);
    }
}

int main(int argc, char *argv[]) {
    printf("sysinfotest: start\n");
    testcall();
    testmem();
    testproc();
    printf("sysinfotest: OK\n");
    exit(0);
}
