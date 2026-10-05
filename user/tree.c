#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

#define PATHSIZ 512

/**
 * @brief Hàm này dùng để liệt kê một đường dẫn bất kỳ nào đó. Có hỗ trợ
 * các tùy biến về độ sâu và cách hiển thị.
 * 
 * @param path đường dẫn muốn liệt kê
 * @param fd pipeline đọc dữ liệu của kernel
 * @param depth chỉ thị độ sâu muốn liệt kê
 * @param d hiển thị file hoặc không
 */
void tree(char* path, int level, int max_depth, int only_dir)
{
    // Base condition
    if(max_depth != -1 && level >= max_depth){
        return;
    }

    // Trong Unix/XV6 mỗi folder được lưu dưới dạng là một `dirent`
    // struct dirent {ushort inum; char name[DIRSIZ]};
    //      - inum: bằng 0 nếu ô này đã bị xóa hoặc empty
    //      - Tên file hoặc thư mục, MAX 14 char 
    struct dirent de;
    struct stat st;

    char buf[PATHSIZ + 1], *p;
    int fd;

    if((fd = open(path, O_RDONLY)) < 0){
        printf("tree: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        printf("tree: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if(level == 0){
        printf("%s\n", path);
    }

    switch(st.type){
        case T_DEVICE: case T_FILE:
            close(fd);
            break;
        case T_DIR:
            if(strlen(path) + DIRSIZ + 1 > sizeof(buf)) {
                printf("tree: path too long, cannot stat\n");
                break;
            }

            // Ghép tên file con hoặc folder con vào đường dẫn hiện tại
            strcpy(buf, path);
            p = buf + strlen(path); // Trỏ đến phần tử '\0'
            *p++ = '/';

            /**
             * Để in được format đẹp cho tree, ở đây áp dụng kỹ thuật là đọc mỗi lần 2 phần tử,
             * để nhận biết được xem là nó có phải là phần tử cuối hay chưa.
             * Giải pháp này tối ưu được bộ nhớ, tránh phải load hết lên RAM.
             */
            struct dirent prev_de;
            struct stat prev_stat;
            int has_prev = 0; 
            
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0) continue;
                if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;      

                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                
                // Kiểm tra xem đường dẫn con có đúng hay chưa. `
                struct stat st_child;
                if(stat(buf, &st_child) < 0){
                    continue;
                }
                if(only_dir && st_child.type != T_DIR) continue;
                
                if(has_prev){
                    for(int lv = 0; lv < level; lv++) printf("|    ");
                    printf("|-- %s\n", prev_de.name);

                    if(prev_stat.type == T_DIR){
                        memmove(p, prev_de.name, DIRSIZ);
                        p[DIRSIZ] = 0;
                        tree(buf, level + 1, max_depth, only_dir);
                    }
                }

                prev_de = de, prev_stat = st_child;
                has_prev = 1;
            }

            if(has_prev){
                for(int lv = 0; lv < level; lv++) printf("|    ");
                printf("└── %s\n", prev_de.name); 
                
                if(prev_stat.type == T_DIR){
                    memmove(p, prev_de.name, DIRSIZ);
                    p[DIRSIZ] = 0;
                    tree(buf, level + 1, max_depth, only_dir);
                }
            }
            close(fd);
    }
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
    
    tree(path, 0, depth, d);
    exit(0);
}