#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/stat.h"

/* @brief: Sao chép nội dung file nguồn sang file đích
Lệnh: cp src dst
File nguồn mở chỉ đọc, file đích được tạo mới nếu không tồn tại hoặc xóa nội 
dung cũ để ghi đè
*/

/**
 * @brief Sao chép dữ liệu theo buffer từ nguồn sang đích
 * Báo lỗi nếu mở, đọc hoặc ghi thất bại
 * @param src đường dẫn file nguồn
 * @param dst đường dẫn file đích
 * @return Không trả về; thành công gọi exit(0), lỗi gọi exit(1).
 */
__attribute__((noreturn)) void cp(char* src, char* dst)
{
    int source = open(src, O_RDONLY);
    if (source < 0){
        fprintf(2, "cp: cannot open %s\n", src);
        exit(1);
    }

    //Kiểm tra loại nguồn và đích là thư mục hay file, nếu là thư mục thì báo lỗi
    struct stat source_stat;
    if (fstat(source, &source_stat) < 0){
        fprintf(2, "cp: cannot stat %s\n", src);
        close(source);
        exit(1);
    }
    if (source_stat.type == T_DIR){
        fprintf(2, "cp: source is a directory: %s\n", src);
        close(source);
        exit(1);
    }

    struct stat destination_stat;
    if (stat(dst, &destination_stat) == 0){
        if (destination_stat.type == T_DIR){
            fprintf(2, "cp: destination is a directory: %s\n", dst);
            close(source);
            exit(1);
        }

        //Kiểm tra xem nguồn và đích có phải cùng một file không, nếu có thì báo lỗi
        if (source_stat.dev == destination_stat.dev &&
            source_stat.ino == destination_stat.ino){
            fprintf(2, "cp: source and destination are the same file\n");
            close(source);
            exit(1);
        }
    }

    /*
    O_WRONLY: mở để ghi
    O_CREATE: tạo file nếu chưa tồn tại
    O_TRUNC: xóa nội dung cũ để sao chép ghi đè hoàn toàn
    */
    int destination = open(dst, O_WRONLY | O_CREATE | O_TRUNC);
    if (destination < 0){
        fprintf(2, "cp: cannot open %s\n", dst);
        close(source);
        exit(1);
    }

    char buffer[512];
    int bytes_read;
    //Sao chép dữ liệu cho tới khi gặp EOF
    while ((bytes_read = read(source, buffer, sizeof(buffer))) > 0){
        int total_written = 0;
        //Chỉ ghi những gì đã đọc được vì lần đọc cuối có thể ít hơn 512 bytes
        //Nếu write ghi được ít byte hơn số byte yêu cầu, tiếp tục đọc trước khi chuyển sang 
        // buffer mới
        while (total_written < bytes_read){
            int bytes_written = write(destination, buffer + total_written, bytes_read - total_written);
            if (bytes_written <= 0){
                fprintf(2, "cp: cannot write %s\n", dst);
                close(source);
                close(destination);
                exit(1);
            }
            total_written += bytes_written;
        }
    }
    //read() trả về 0 là EOF -> hợp lệ
    //Lỗi trả về -1
    if (bytes_read < 0){
        fprintf(2, "cp: cannot read %s\n", src);
        close(source);
        close(destination);
        exit(1);
    }
    close(source);
    close(destination);
    exit(0);
}

/**
 * @brief Kiểm tra đối số và gọi hàm sao chép file
 * @param args số lượng đối số, cụ thể là 3
 * @param argv danh sách đối số, argv[1] là nguồn, argv[2] là đích
 * @return Không trả về; đối số sai gọi exit(1), còn cp() kết thúc
 * tiến trình sau khi xử lý sao chép
 */
int main(int args, char* argv[])
{
    //Kiểm tra xem có đủ số lượng đối số không
    if (args != 3){
        fprintf(2, "usage: cp src dst\n");
        exit(1);
    }

    cp(argv[1], argv[2]);
}
