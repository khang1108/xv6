#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"
#include <limits.h>

#define PATHSIZ 128

struct child {
    char name[DIRSIZ + 1];
    int is_dir;
};

/**
 * @brief Hàm dùng để parse giá trị số nguyên từ chuỗi.
 *        Nó cũng kiểm tra ký tự không phải số và overflow.
 *
 * @param s là ký tự cần parse
 * @return int -1 nếu có lỗi, còn không thì là giá trị số
 * @author Khang Phuc Nguyen (khang1108)
 * @date 15 Oct, 2026
 */
int parse(char *s) {
    int depth = 0;

    // Nếu chuỗi rỗng
    if (*s == '\0') {
        return -1;
    }

    for (; *s; s++) {
        if (*s < '0' || *s > '9') {
            return -1;
        }

        int digit = *s - '0';
        if (depth > (INT_MAX - digit) / 10) {
            return -1;
        }
        depth = depth * 10 + digit;
    }

    return depth;
}

/**
 * @brief Hàm này dùng để liệt kê một đường dẫn bất kỳ nào đó. Có hỗ trợ
 * các tùy biến về độ sâu và cách hiển thị.
 *
 * Đóng FD thư mục trước khi đệ quy giúp số FD mở thêm ở O(1), nhưng
 * mỗi tầng vẫn chiếm user stack. LAB_UTIL có USERSTACK = 2 trang
 * (8 KiB), không tự mở rộng; cây quá sâu vẫn có thể tràn stack.
 * Bộ đệm PATHSIZ + 1 và MAXPATH của syscall cũng giới hạn đường dẫn.
 * -L chỉ giới hạn số tầng duyệt, không mở rộng các tài nguyên này.
 *
 * @param path đường dẫn muốn liệt kê
 * @param level mức hiện tại trong cây
 * @param max_depth độ sâu tối đa, -1 là không giới hạn bằng tùy chọn -L
 * @param only_dir chỉ hiển thị thư mục khi khác 0
 * @param ancestor_has_next mỗi mức tổ tiên có còn entry phía sau hay không
 */
int tree(char *path, int level, int max_depth, int only_dir,
          char *ancestor_has_next) {
    // Trong Unix/XV6 mỗi folder được lưu dưới dạng là một `dirent`
    // struct dirent {ushort inum; char name[DIRSIZ]};
    //      - inum: bằng 0 nếu ô này đã bị xóa hoặc empty
    //      - Tên file hoặc thư mục, MAX 14 char
    struct dirent de;
    struct stat st;

    char buf[PATHSIZ + 1], *p;
    int fd;
	int status = 0;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "tree: cannot open %s\n", path);
        return -1;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "tree: cannot stat %s\n", path);
        close(fd);
        return -1;
    }

    if (level == 0) {
        fprintf(1, "%s\n", path);
    }

    // Base condition
    if (max_depth != -1 && level >= max_depth) {
        close(fd);
        return status;
    }

    switch (st.type) {
    case T_DEVICE:
    case T_FILE:
        close(fd);
        break;
    case T_DIR:
		// +2 dành cho / và '\0'
		if(strlen(path) + 2 > sizeof(buf)){
			fprintf(2, "tree: path too long: %s\n", path);
			close(fd);
			return -1;
		}
        // Ghép tên file con hoặc folder con vào đường dẫn hiện tại
        strcpy(buf, path);
        p = buf + strlen(path); // Trỏ đến phần tử '\0'
        *p++ = '/';

        // `count` để lưu số lượng entry hiện tại.
        // `capacity` để lưu trữ dung lượng hiện tại của mảng `children`.
        // `children` để lưu trữ danh sách các file hoặc thư mục con.
        int count = 0, capacity = 8;
        struct child *children = malloc(capacity * sizeof(struct child));
        if (children == 0) {
            fprintf(2, "tree: cannot allocate memory\n");
            close(fd);
            exit(1);
        }

        int bytes_read;
        while ((bytes_read = read(fd, &de, sizeof(de))) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

			char name[DIRSIZ + 1];
			memmove(name, de.name, DIRSIZ);
			name[DIRSIZ] = 0;

			int name_len = strlen(name);
			if(strlen(path) + 1 + name_len + 1 > sizeof(buf)){
				fprintf(2, "tree: path too long: %s/%s\n", path, name);
				status = -1;
				continue; // Chỉ bỏ qua con bị tràn, không bỏ cả thư mục
			}

			// copy de.name to a buffer.
            // Vì DIRSIZ = 14, nên là nếu tên đúng 14 ký tự thì sẽ không có '\0'
            // -> lỗi an toàn là copy ra biến khác rồi thêm '\0'
            memmove(p, name, name_len);
            p[name_len] = 0;
			
            // Kiểm tra xem đường dẫn con có đúng hay chưa. `
            struct stat st_child;
            if (stat(buf, &st_child) < 0) {
				fprintf(2, "tree: cannot stat %s\n", buf);
				status = -1;
                continue;
            }
            if (only_dir && st_child.type != T_DIR)
                continue;

            if (count == capacity) {
                // Mảng động bị đầy cần khai báo thêm
                capacity *= 2;
                struct child *new_children =
                    malloc(capacity * sizeof(struct child));
                if (new_children == 0) {
                    fprintf(2, "tree: cannot allocate memory\n");
                    close(fd);
                    exit(1);
                }

                memmove(new_children, children, count * sizeof(*children));
                free(children);
                children = new_children;
            }
            children[count].is_dir = st_child.type == T_DIR;
            memmove(children[count].name, de.name, DIRSIZ);
            children[count].name[DIRSIZ] = 0;
            count++;
        }
        if (bytes_read != 0) {
            fprintf(2, "tree: read failed at %s\n", path);
			status = -1;
        }
        // Giải phóng FD trước các lời gọi con; user stack vẫn tăng theo độ sâu.
        close(fd);

        for (int i = 0; i < count; i++) {
            int is_last = (i == count - 1);
            // Chỉ nối đường dọc nếu tổ tiên còn entry phía sau.
            for (int lv = 0; lv < level; lv++) {
                fprintf(1, "%s", ancestor_has_next[lv] ? "│   " : "    ");
            }
            fprintf(1, "%s%s\n", is_last ? "└── " : "├── ", children[i].name);
            if (children[i].is_dir &&
                (max_depth == -1 || level + 1 < max_depth)) {
                // Nếu là thư mục và max_depth cho phép đệ quy
                // Nối tên con vào đường dẫn hiện tại
                strcpy(p, children[i].name);
                ancestor_has_next[level] = !is_last;
                // Gọi đệ quy
                if(tree(buf, level + 1, max_depth, only_dir, ancestor_has_next) < 0) {
					status = -1;
				}
            }
        }
        free(children);
    }
	return status;
}

int main(int args, char *argv[]) {
    char *path = ".";
    int depth = -1, d = 0;
    int has_path = 0;
    // Số mức của đường dẫn hợp lệ luôn nhỏ hơn PATHSIZ.
    char ancestor_has_next[PATHSIZ] = {0};

    for (int i = 1; i < args; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            d = 1;
        } else if (strcmp(argv[i], "-L") == 0) {
            if (i + 1 >= args) {
                fprintf(2, "tree: missing depth argument for -L\n");
                exit(1);
            }
            depth = parse(argv[++i]);
            if (depth < 0) {
                fprintf(2, "tree: invalid depth for -L\n");
                exit(1);
            }
        } else if (argv[i][0] == '-' || has_path) {
            fprintf(2, "usage: tree [path] [-L depth] [-d]\n");
            exit(1);
        } else {
            path = argv[i];
            has_path = 1;
        }
    }

    int status = tree(path, 0, depth, d, ancestor_has_next);
    exit(status < 0 ? 1: 0);
}
