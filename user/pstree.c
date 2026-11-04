#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

// Thuật toán duyệt cây theo chiều sâu (DFS - Pre-order) để hiển thị cấu trúc phả hệ.
// Thụt lề (depth * 2) khoảng trắng để biểu diễn mức độ phân cấp cha - con.
void print_tree(
    int pid, int depth,
    struct ptreeinfo* procs, 
    int n
)
{
    // Tìm thông tin tiến trình hiện tại theo PID và in ra màn hình
    for(int i = 0; i < n; i++){
        if(procs[i].pid == pid){
            // Thụt lề theo độ sâu của cây
            for(int s = 0; s < depth * 2; s++){
                printf(" ");
            }
            // Định dạng xuất theo yêu cầu: <pid> <name> state=<state> mem=<memsize>
            printf("%d %s state=%d mem=%ld\n",
                procs[i].pid, procs[i].name,
                procs[i].state, procs[i].memsize);
            break;
        }
    }

    // Duyệt tìm tất cả các tiến trình con (có ppid == pid hiện tại) và gọi đệ quy
    for(int j = 0; j < n; j++){
        if(procs[j].ppid == pid && procs[j].pid != pid){
            print_tree(procs[j].pid, depth + 1, procs, n);
        }
    }
}

int main(int argc, char* argv[])
{
    static struct ptreeinfo procs[NPROC]; //* Dùng static để tránh bị tràn stack khi đệ quy quá sâu. 
    // Gọi system call ptree để lấy danh sách tiến trình đang hoạt động từ kernel
    int n = ptree(procs, NPROC);
    if(n < 0){
        fprintf(2, "pstree: fail to get processes\n");
        exit(1);
    }

    // Tìm các tiến trình gốc không có cha (ppid == 0 hoặc cha không nằm trong danh sách)
    // để bắt đầu duyệt cây từ gốc với depth = 0
    for(int i = 0; i < n; i++){
        int has_parent = 0;

        if(procs[i].ppid != 0){
            for(int j = 0; j < n; j++){
                if(procs[j].pid == procs[i].ppid){
                    has_parent = 1;
                    break;
                }
            }
        }
        // Nếu là tiến trình gốc (tiêu biểu là initproc), kích hoạt duyệt DFS từ đây
        if(!has_parent){
            print_tree(procs[i].pid, 0, procs, n);
        }
    }
    exit(0);
}