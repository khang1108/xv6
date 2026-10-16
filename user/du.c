#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user/user.h"

#define MAXPATH 512

/**
 * @brief Đệ quy tính tổng dung lượng thư mục.
 *
 * Quy ước: cộng st.size (bytes) của các mục T_FILE/T_DEVICE bên dưới;
 * bỏ qua st.size của chính thư mục, nên thư mục rỗng có tổng bằng 0.
 * Đây là tổng kích thước logic, không phải số khối đĩa như du POSIX/GNU.
 * Đóng FD trước khi đệ quy giữ số FD mở thêm ở O(1), nhưng mỗi tầng
 * vẫn dùng user stack. LAB_UTIL có USERSTACK = 2 trang (8 KiB), không
 * tự mở rộng; cây quá sâu vẫn có thể tràn stack. Đường dẫn cũng bị
 * giới hạn bởi buf và MAXPATH của các lời gọi hệ thống xv6.
 * Nếu bất kỳ mục con nào lỗi, dừng duyệt và truyền -1 về thư mục cha;
 * không in tổng chưa đầy đủ. Các dòng đã in cho nhánh hoàn tất vẫn được giữ.
 *
 * @param path Đường dẫn hiện tại.
 * @param a Cờ -a (in cả file).
 * @param s Cờ -s (summary, chỉ in tổng thư mục gốc).
 * @param is_root Cờ đánh dấu đường dẫn gốc (được in khi tính thành công).
 * @return Tổng dung lượng (bytes) của nhánh hiện tại, hoặc -1 nếu có lỗi.
 */
int du(char *path, int a, int s, int is_root)
{
  struct stat st;

  // Lấy thông tin file/thư mục. Trả về < 0 nếu lỗi.
  if (stat(path, &st) < 0)
  {
    fprintf(2, "du: cannot stat %s\n", path);
    return -1;
  }

  // Xử lý file hoặc device (không phải thư mục)
  if (st.type == T_FILE || st.type == T_DEVICE)
  {
    // Chỉ in ra nếu là file gốc được gọi, hoặc có cờ -a (và không có cờ -s)
    if (is_root)
    {
      printf("%d\t%s\n", (int)st.size, path);
    }
    else if (a && !s)
    {
      printf("%d\t%s\n", (int)st.size, path);
    }
    return (int)st.size;
  }

  // Mở thư mục để đọc các entry bên trong
  int fd = open(path, 0);
  if (fd < 0)
  {
    fprintf(2, "du: cannot open %s\n", path);
    return -1;
  }

  struct dirent de;
  int capacity = 16;
  int count = 0;

  // names là con trỏ tới mảng char[DIRSIZ + 1], không phải mảng con trỏ.
  // Các tên nằm liên tiếp trên heap, mỗi tên 15 byte (14 ký tự và '\0').
  char (*names)[DIRSIZ + 1] = malloc(capacity * (DIRSIZ + 1));
  if (!names)
  {
    fprintf(2, "du: cannot allocate memory for %s\n", path);
    close(fd);
    return -1;
  }

  // Chỉ EOF (0) kết thúc thành công; lỗi hoặc bản ghi thiếu byte phải báo lỗi.
  int bytes_read;
  while ((bytes_read = read(fd, &de, sizeof(de))) == sizeof(de))
  {
    if (de.inum == 0) // Bỏ qua slot trống
      continue;

    // Bỏ qua . và .. để tránh quay lại chính nó/cha gây tràn user stack.
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;

    // Xử lí names nếu đầy
    if (count >= capacity)
    {
      int new_cap = capacity * 2;
      char (*new_names)[DIRSIZ + 1] = malloc(new_cap * (DIRSIZ + 1));
      // Giữ names cũ cho đến khi cấp phát mới thành công để dọn dẹp khi lỗi.
      if (!new_names)
      {
        fprintf(2, "du: cannot allocate memory for %s\n", path);
        close(fd);
        free(names);
        return -1;
      }
      memmove(new_names, names, count * (DIRSIZ + 1));
      free(names);
      names = new_names;
      capacity = new_cap;
    }

    // Lưu tên file vào mảng
    memmove(names[count], de.name, DIRSIZ);
    names[count][DIRSIZ] = '\0';
    count++;
  }

  // Đóng trước khi đệ quy: FD thư mục không tích lũy theo độ sâu (NOFILE = 16).
  // Cách này không loại bỏ giới hạn độ sâu do user stack cố định.
  close(fd);
  if (bytes_read != 0)
  {
    fprintf(2, "du: cannot read directory %s\n", path);
    free(names);
    return -1;
  }

  // Không cộng st.size của thư mục; tổng chỉ đến từ các mục con.
  int total_size = 0;

  // Duyệt qua từng file con để gọi đệ quy (Bottom-up)
  for (int i = 0; i < count; i++)
  {
    char buf[MAXPATH];
    if (strlen(path) + 1 + strlen(names[i]) >= sizeof(buf))
    {
      fprintf(2, "du: path too long: %s/%s\n", path, names[i]);
      free(names);
      return -1;
    }

    // Ghép đường dẫn: path + "/" + name
    strcpy(buf, path);
    char *p = buf + strlen(buf);
    *p++ = '/';
    strcpy(p, names[i]);

    // Không cộng mã lỗi vào tổng và không in tổng của nhánh có mục bị lỗi.
    int child_size = du(buf, a, s, 0);
    if (child_size < 0)
    {
      free(names);
      return -1;
    }
    total_size += child_size;
  }

  free(names);

  // In tổng dung lượng của thư mục hiện tại
  if (is_root)
  {
    printf("%d\t%s\n", total_size, path);
  }
  else if (!s)
  {
    printf("%d\t%s\n", total_size, path);
  }

  return total_size;
}

/**
 * @brief Hàm chạy chính của chương trình du.
 *
 * @param argc Số lượng đối số truyền vào.
 * @param argv Mảng các chuỗi đối số.
 * @return 0 nếu tính thành công, 1 nếu lỗi cú pháp, tệp, I/O hoặc cấp phát.
 */
int main(int argc, char *argv[])
{
  char *path = ".";
  int a = 0;
  int s = 0;
  int has_path = 0;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-a") == 0)
    {
      a = 1;
    }
    else if (strcmp(argv[i], "-s") == 0)
    {
      s = 1;
    }
    else if (argv[i][0] != '-' && !has_path)
    {
      path = argv[i];
      has_path = 1;
    }
    else
    {
      fprintf(2, "usage: du [path] [-a] [-s]\n");
      exit(1);
    }
  }

  // Một nhánh lỗi làm cả lệnh thất bại, kể cả khi các nhánh trước đã in kết quả.
  int total_size = du(path, a, s, 1);
  exit(total_size < 0 ? 1 : 0);
}
