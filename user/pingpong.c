#include "kernel/types.h"
#include "user/user.h"

/* @brief: Trao đổi 1 byte giữa tiến trình cha và con
Sử dụng 2 pipe để tách riêng hai chiều truyền dữ liệu
*/

/* @brief: Tạo các pipe phục vụ trao đổi dữ liệu giữa cha và con
- @param args số lượng tham số dòng lệnh, không có
- @param argv danh sách tham số dòng lệnh, không có
- @return Không trả về, thành công thì exit(0), lỗi thì exit(1)
*/

int main(int args, char* argv[]){
    int parent_to_child[2]; // Cha gửi con nhận
    int child_to_parent[2]; // Con gửi cha nhận
    //[0]: đầu đọc
    //[1]: đầu ghi

    char byte = 'p';
    if (pipe(parent_to_child) < 0){
        fprintf(2, "pingpong: cannot create parent_to_child\n");
        exit(1);
    }

    if (pipe(child_to_parent) < 0){
        fprintf(2, "pingpong: cannot create child_to_parent\n");
        // Do pipe đàu tiên đã được tạo, nên phải đóng trước khi thoát
        close(parent_to_child[0]);
        close(parent_to_child[1]);
        exit(1);
    }

    // Tạo fork()
    int pid = fork();

    if (pid < 0){
        //Lúc này tạo con thất bại
        fprintf(2, "pingpong: cannot fork\n");
        //Đóng các pipe rồi trả về lỗi
        close(parent_to_child[0]);
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        close(child_to_parent[1]);
        exit(1);
    }

    if (pid == 0){
        //Con nhận được dữ liệu từ cha và gửi phản hồi
        //Giữ lại parent_to_child[0], đọc dữ liệu từ cha
        //Giữ lại child_to_parent[1], ghi phản hồi cho cha
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        //Đọc 1 byte và lưu vào biến byte
        if (read(parent_to_child[0], &byte, 1) != 1){
            fprintf(2, "pingpong: child  cannot reveiced byte\n");
            exit(1);
        }
        //Con chỉ nhận 1 byte nên không giữ đầu đọc nữa
        close(parent_to_child[0]);
        printf("%d: received ping\n", getpid());

        // Gửi lại byte vừa nhận qua pipe phản hồi 
        if (write(child_to_parent[1], &byte, 1) != 1){
            fprintf(2, "pingpong: child cannot send byte\n");
            exit(1);
        }
        close(child_to_parent[1]);
        //Thành công gửi tiến trình con và exit(0)
        exit(0);
    }

    //Cha nhận phản hồi của tiến trình con và đọc
    close(parent_to_child[0]);
    close(child_to_parent[1]);

    if (write(parent_to_child[1], &byte, 1) != 1){
        fprintf(2, "pingpong: parent cannot send byte\n");
        exit(1);
    }
    //Đóng đầu ghi của cha vì đã gửi đủ dữ liệu
    close(parent_to_child[1]);
    //Chờ một byte phản hồi từ con
    int received = read(child_to_parent[0], &byte, 1);
    close(child_to_parent[0]);
    
    if (received == 1){
        printf("%d: received pong\n", getpid());
    }else{
        fprintf(2, "pingpong: parent cannot receive byte\n");
    }

    //Chờ tiến trình con kết thúc và thu hồi lại
    int status_child;
    if (wait(&status_child) < 0){
        fprintf(2, "pingpong: cannot wait for child\n");
        exit(1);
    }
    //Thành công khi cha nhận 1 byte và tiến trình con trả về 0
    if (received != 1 || status_child != 0){
        exit(1);
    }
    exit(0);
}
