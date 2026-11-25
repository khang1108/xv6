#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/riscv.h"
#include "kernel/types.h"
#include "user/user.h"

void ugetpid_test();
void pgaccess_test();

char *testname = "???";

void err(char *why) {
    printf("pgtbltest: %s failed: %s, pid=%d\n", testname, why, getpid());
    exit(1);
}

int main(int argc, char *argv[]) {
    ugetpid_test();
    pgaccess_test();
    printf("pgtbltest: all tests succeeded\n");
    exit(0);
}

void ugetpid_test() {
    int i;

    printf("ugetpid_test starting\n");
    testname = "ugetpid_test";

    if (getpid() != ugetpid())
        err("mismatched parent PID");

    for (i = 0; i < 64; i++) {
        int ret = fork();
        if (ret < 0)
            err("fork");
        if (ret != 0) {
            wait(&ret);
            if (ret != 0)
                exit(1);
            continue;
        }
        if (getpid() != ugetpid())
            err("mismatched PID");
        exit(0);
    }
    printf("ugetpid_test: OK\n");
}

void pgaccess_test() {
    char *buf;
    unsigned int abits;

    printf("pgaccess_test starting\n");
    testname = "pgaccess_test";

    buf = malloc(32 * PGSIZE);
    if (buf == 0)
        err("malloc");

    // Xóa PTE_A còn sau quá tình cấp phát ở ban đầu.
    if (pgaccess(buf, 32, &abits) < 0)
        err("pgaccess initial clear");

    abits = 0;
    buf[PGSIZE * 1] += 1;
    buf[PGSIZE * 2] += 1;
    buf[PGSIZE * 30] += 1;

    if (pgaccess(buf, 32, &abits) < 0)
        err("pgaccess failed");
    if (abits != ((1U << 1) | (1U << 2) | (1U << 30)))
        err("incorrect access bits set");

    // Goi lai ngay sau do de xac nhan kernel da xoa PTE_A ve 0.
    if (pgaccess(buf, 32, &abits) < 0)
        err("pgaccess second call");
    if (abits != 0)
        err("PTE_A was not cleared");

    // Kiem tra hai trang bien 0 va 31.
    buf[PGSIZE * 0] += 1;
    buf[PGSIZE * 31] += 1;
    if (pgaccess(buf, 32, &abits) < 0)
        err("pgaccess boundary call");
    if (abits != ((1U << 0) | (1U << 31)))
        err("incorrect boundary access bits");

    // Kiem tra cac tham so khong hop le.
    if (pgaccess(buf, -1, &abits) >= 0)
        err("negative len accepted");
    if (pgaccess(buf, 33, &abits) >= 0)
        err("len > 32 accepted");
    if (pgaccess(buf, 4, (void *)0xffffffffffffffffL) >= 0)
        err("invalid mask pointer accepted");

    free(buf);
    printf("pgaccess_test: OK\n");
}
