#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int i;
    char* nargv[MAXARG]; // Tạo ra một mảng mới để lưu được các 
                        // các đối số mới khi ta gọi một `exec()`

    // Kiểm syntax
    if(argc < 3 || argv[1][0] < '0' || argv[1][0] > '9') {
        fprintf(2, "usage: trace [mask] [command]\n");
        exit(1);
    }

    // Gọi systemcall cho ID được truyền vào
    if(trace(atoi(argv[1])) < 0){
        fprintf(2, "trace: failed\n");
        exit(1);
    }

    for(i = 2; i < argc && i < MAXARG; i++){
        nargv[i - 2] = argv[i];
    }
    nargv[argc - 2] = 0; // Đặt giá trị cuối = '\0' pđể báo hiểu kết thúc.

    exec(nargv[0], nargv);
    printf("trace: exec failed\n");
    exit(0);
}