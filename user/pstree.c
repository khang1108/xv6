#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

void print_tree(
    int pid, int depth,
    struct ptreeinfo* procs, 
    int n
)
{
    for(int i = 0; i < n; i++){
        if(procs[i].pid == pid){
            for(int s = 0; s < depth * 2; s++){
                printf(" ");
            }
            printf("%d %s state=%d mem=%ld\n",
                procs[i].pid, procs[i].name,
                procs[i].state, procs[i].memsize);
            break;
        }
    }

    // Đệ quy để in cây.
    for(int j = 0; j < n; j++){
        if(procs[j].ppid == pid && procs[j].pid != pid){
            print_tree(procs[j].pid, depth + 1, procs, n);
        }
    }
}
int main(int argc, char* argv[])
{
    struct ptreeinfo procs[NPROC];
    int n = ptree(procs, NPROC);
    if(n < 0){
        fprintf(2, "pstree: fail to get processes\n");
        exit(1);
    }

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
        if(!has_parent){
            print_tree(procs[i].pid, 0, procs, n);
        }
    }
    exit(0);
}