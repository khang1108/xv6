#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

/**
 * @brief Đọc một dòng từ file. Tự động mở rộng buffer nếu dòng quá dài.
 *
 * Hàm này đọc từng byte cho đến khi gặp ký tự '\n' hoặc EOF.
 * Ký tự '\n' sẽ được lưu vào buffer, và chuỗi sẽ được kết thúc bằng '\0'.
 *
 * @param fd File descriptor của file đang đọc.
 * @param buf Con trỏ trỏ tới mảng dữ liệu. Có thể bị thay đổi nếu mảng được cấp phát lại.
 * @param cap Con trỏ lưu sức chứa tối đa hiện tại của mảng buf.
 * @param has_newline Con trỏ lưu trạng thái (1 nếu dòng kết thúc bằng '\n', 0 nếu không có).
 * @return Số lượng byte thực tế đã đọc được (lớn hơn 0). Trả về 0 nếu EOF, -1 nếu có lỗi (malloc hoặc read).
 */
int readline(int fd, char **buf, int *cap, int *has_newline)
{
    if (*cap == 0)
    {
        *cap = 128;
        *buf = malloc(*cap);
        if (!*buf)
            return -1;
    }

    int len = 0;
    char c;
    *has_newline = 0;

    while (read(fd, &c, 1) == 1)
    {
        if (len + 1 >= *cap)
        {
            int new_cap = *cap * 2;
            char *new_buf = malloc(new_cap);
            if (!new_buf)
                return -1;

            memmove(new_buf, *buf, len);
            free(*buf);

            *buf = new_buf;
            *cap = new_cap;
        }

        (*buf)[len++] = c;
        if (c == '\n')
        {
            *has_newline = 1;
            break;
        }
    }

    (*buf)[len] = '\0';
    return len;
}

/**
 * @brief In kết quả so sánh của một dòng ra màn hình.
 *
 * Nếu dòng bị EOF trước, in thông báo EOF.
 * Đồng thời kiểm tra và cảnh báo nếu dòng cuối cùng của file thiếu ký tự '\n'.
 *
 * @param prefix Tiền tố in ra ("<" đối với file 1, ">" đối với file 2).
 * @param name Tên của file đang in.
 * @param line_num Số thứ tự của dòng hiện tại (bắt đầu từ 1).
 * @param line Nội dung của dòng. Nếu line == 0, nghĩa là file đã hết (EOF).
 * @param has_nl Cờ (1 hoặc 0) báo hiệu xem dòng này có ký tự '\n' ở cuối không.
 * @return Không có giá trị trả về.
 */
void print_line(char *prefix, char *name, int line_num, char *line, int has_nl)
{
    if (line == 0)
    {
        printf("%s:%d: %s EOF\n", name, line_num, prefix);
    }
    else
    {
        // Biến line đã chứa sẵn ký tự '\n' ở cuối nên không cần thêm \n
        printf("%s:%d: %s %s", name, line_num, prefix, line);
        if (!has_nl)
        {
            printf("\n");
            printf("diff: %s:%d: no newline at end of file\n", name, line_num);
        }
    }
}

/**
 * @brief Chương trình diff cơ bản cho xv6.
 *
 * So sánh hai file văn bản theo từng dòng và in ra sự khác biệt.
 * Hỗ trợ cờ -q để ngắt sớm khi vừa phát hiện khác biệt.
 *
 * @param argc Số lượng đối số dòng lệnh.
 * @param argv Mảng chứa các đối số dòng lệnh.
 * @return 0 nếu hai file giống nhau hoàn toàn, 1 nếu khác nhau, 2 nếu có lỗi xảy ra.
 */
int main(int argc, char *argv[])
{
    int quiet = 0;
    char *f1 = 0, *f2 = 0;

    // Phân tích tham số
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-q") == 0)
            quiet = 1;
        else if (!f1)
            f1 = argv[i];
        else if (!f2)
            f2 = argv[i];
    }

    if (!f1 || !f2)
    {
        fprintf(2, "usage: diff file1 file2 [-q]\n");
        exit(1);
    }

    int fd1 = open(f1, O_RDONLY);
    int fd2 = open(f2, O_RDONLY);
    if (fd1 < 0)
    {
        fprintf(2, "diff: cannot open %s\n", f1);
        exit(1);
    }
    if (fd2 < 0)
    {
        fprintf(2, "diff: cannot open %s\n", f2);
        exit(1);
    }

    struct stat st1, st2;
    if (fstat(fd1, &st1) < 0 || st1.type != T_FILE ||
        fstat(fd2, &st2) < 0 || st2.type != T_FILE)
    {
        fprintf(2, "diff: must be files\n");
        exit(1);
    }

    int cap1 = 0, cap2 = 0;
    char *buf1 = 0, *buf2 = 0;
    int has_nl1 = 0, has_nl2 = 0;
    int line_num = 1, diff_found = 0;

    while (1)
    {
        int len1 = readline(fd1, &buf1, &cap1, &has_nl1);
        int len2 = readline(fd2, &buf2, &cap2, &has_nl2);

        if (len1 < 0 || len2 < 0)
        {
            fprintf(2, "diff: read error\n");
            diff_found = 2;
            break;
        }

        if (len1 == 0 && len2 == 0)
            break; // Cả hai đều kết thúc

        int is_diff = 0;

        // Khác nhau về độ dài chuỗi hoặc trạng thái dấu Enter
        if (len1 != len2 || has_nl1 != has_nl2)
        {
            is_diff = 1;
        }
        // Kiểm tra nội dung chuỗi (nếu độ dài bằng nhau)
        else if (len1 > 0 && strcmp(buf1, buf2) != 0)
        {
            is_diff = 1;
        }

        if (is_diff)
        {
            diff_found = 1;
            if (quiet)
            {
                printf("diff: files differ\n");
                break;
            }
            print_line("<", f1, line_num, len1 > 0 ? buf1 : 0, has_nl1);
            print_line(">", f2, line_num, len2 > 0 ? buf2 : 0, has_nl2);
        }
        line_num++;
    }

    if (buf1)
        free(buf1);
    if (buf2)
        free(buf2);
    close(fd1);
    close(fd2);

    exit(diff_found);
}
