#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

/**
 * @brief Hàm này dùng để lọc các số còn lại được truyền tới
 *         từ pipe bên trái hay gọi là `left_fd`. Nó sẽ lọc ra 
 *         các số còn phù hợp và bỏ đi các bội của số nguyên tố
 *         đầu tiên.
 * @param left_fd là File Descriptor đại diện cho pipeline bên trái
 * @author Khang Phuc Nguyen (khang1108)
 * @date 4 Oct, 2026
 */
__attribute__((noreturn)) void filter(int left_fd){
    int p; 
    if(read(left_fd, &p, sizeof(int)) <= 0){
        close(left_fd);
        wait(0);
        exit(0);
    }
    printf("prime %d\n", p);

    // Tạo ống mới, phục vụ cho việc ghi
    int new_pipe[2];
    pipe(new_pipe);

    int pid = fork();

    if(pid == 0){
        // Vì đây là process con, nó chỉ việc đọc dữ liệu từ ống mới
        // Do đó, process con sẽ không bao giờ ghi.
        close(new_pipe[1]);
        close(left_fd); // Cần đóng pipe của cha

        filter(new_pipe[0]);
        wait(0);
        exit(0);
    }
    else{
        close(new_pipe[0]);

        int n;
        while(read(left_fd, &n, sizeof(int))){
            if(n % p != 0){
                write(new_pipe[1], &n, sizeof(int));
            }
        }
        close(new_pipe[1]);
        wait(0);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    int fd[2];
    pipe(fd);

    // Cần chia 2 process song song tránh tình trạng tràn bộ nhớ vì nó chỉ có 512 bytes
    // Nêu đẩy hết từ `2->280` gồm 1116 bytes -> bị tràn RAM
    int pid = fork();
    if(pid == 0){
        // Chỉ đọc không ghi
        close(fd[1]);
        filter(fd[0]);
    }
    else{
        close(fd[0]);
        for(int i = 2; i <= 280; i++){
            write(fd[1], &i, sizeof(int));
        }
        close(fd[1]);
        wait(0);
        exit(0);
    }
}