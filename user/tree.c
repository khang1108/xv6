#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

/**
 * @brief Hàm này dùng để liệt kê một đường dẫn bất kỳ nào đó. Có hỗ trợ
 * các tùy biến về độ sâu và cách hiển thị.
 * 
 * @param path đường dẫn muốn liệt kê
 * @param fd pipeline đọc dữ liệu của kernel
 * @param depth chỉ thị độ sâu muốn liệt kê
 * @param d hiển thị file hoặc không
 */
void tree(int fd, char* path, int depth, int d)
{

}

int main(int args, char* argv[])
{
    char *path = ".";
    int depth = -1, d = 0;

    for(int i = 1; i < args; i++){
        if(strcmp(argv[i], "-d") == 0){
            d = 1;
        } else if(strcmp(argv[i], "-L") == 0){
            if(i + 1 < args){
                depth = atoi(argv[++i]);
            }
            else{
                printf("tree: missing depth argument for -L\n");
                exit(0);
            }
        } else if(argv[i][0] != '-'){
            path = argv[i];
        }
        else {
            printf("usage: tree [path] [-L depth] [-d]\n");
            exit(1);
        }
    }
    
    int fd;
    if(fd = open(path, O_RDONLY) < 0){
        printf("tree: cannot open %s\n", path);
        exit(1);
    }
    tree(fd, path, depth, d);
    close(fd);
    exit(0);
}