#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int i;
    char* nargv[MAXARG]; // Tạo ra một mảng mới để lưu được các 
                        // các đối số mới khi ta gọi một `exec()`

    // Kiểm tra số lượng tham số tối thiểu: cần ít nhất mask và lệnh cần chạy
    if(argc < 3) {
        fprintf(2, "usage: trace [mask] [command]\n");
        exit(1);
    }

    // Kiểm tra tính hợp lệ của toàn bộ chuỗi mask: bắt buộc tất cả ký tự phải là chữ số.
    // Việc này ngăn chặn các trường hợp nhập sai cú pháp như "32abc" (vốn sẽ bị lọt
    // nếu chỉ kiểm tra ký tự đầu tiên argv[1][0]).
    for(char* p = argv[1]; *p; p++) {
        if(*p < '0' || *p > '9') {
            fprintf(2, "trace: invalid mask '%s', must be an integer\n", argv[1]);
            exit(1);
        }
    }

    // Gọi systemcall cho ID được truyền vào
    if(trace(atoi(argv[1])) < 0){
        fprintf(2, "trace: failed\n");
        exit(1);
    }

    // Trích xuất các đối số của lệnh cần chạy vào mảng nargv
    for(i = 2; i < argc && i < MAXARG; i++){
        nargv[i - 2] = argv[i];
    }
    nargv[argc - 2] = 0; // Đặt phần tử kết thúc của mảng argv thành NULL

    // Thực thi chương trình đích. Nếu exec thành công, không gian nhớ của tiến trình
    // sẽ được thay thế hoàn toàn và luồng điều khiển sẽ không quay lại đây.
    // Nếu exec thất bại (chẳng hạn không tìm thấy tệp), chương trình in thông báo lỗi
    // và bắt buộc phải trả về mã lỗi 1 (exit(1)) để thông báo cho shell,
    // tránh trường hợp trả về 0 (exit(0)) gây hiểu lầm là thực thi thành công.
    exec(nargv[0], nargv);
    fprintf(2, "trace: exec %s failed\n", nargv[0]);
    exit(1);
}