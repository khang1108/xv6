#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user/user.h"

#define MAXPATH 512

/**
 * @brief Đệ quy tính tổng dung lượng thư mục.
 *
 * @param path Đường dẫn hiện tại.
 * @param a Cờ -a (in cả file).
 * @param s Cờ -s (summary, chỉ in tổng thư mục gốc).
 * @param is_root Cờ đánh dấu thư mục gốc (luôn được in ra).
 * @return Tổng dung lượng (bytes) của nhánh hiện tại.
 */
int du(char *path, int a, int s, int is_root)
{
  struct stat st;

  // Lấy thông tin file/thư mục. Trả về < 0 nếu lỗi.
  if (stat(path, &st) < 0)
  {
    fprintf(2, "du: cannot stat %s\n", path);
    return 0;
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
    return 0;
  }

  struct dirent de;
  int capacity = 16;
  int count = 0;

  // Cấp phát mảng động lưu tên các file/thư mục con (DIRSIZ = 14)
  char (*names)[DIRSIZ + 1] = malloc(capacity * (DIRSIZ + 1));
  if (!names)
  {
    close(fd);
    return 0;
  }

  // Đọc nội dung thư mục
  while (read(fd, &de, sizeof(de)) == sizeof(de))
  {
    if (de.inum == 0) // Bỏ qua slot trống
      continue;

    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) // Bỏ qua . và .. để tránh lặp vô tận
      continue;

    // Xử lí names nếu đầy
    if (count >= capacity)
    {
      int new_cap = capacity * 2;
      char (*new_names)[DIRSIZ + 1] = malloc(new_cap * (DIRSIZ + 1));
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

  // Đóng thư mục ngay để giải phóng FD, tránh vượt quá giới hạn 16 FD của xv6
  close(fd);

  int total_size = 0;

  // Duyệt qua từng file con để gọi đệ quy (Bottom-up)
  for (int i = 0; i < count; i++)
  {
    char buf[MAXPATH];
    if (strlen(path) + 1 + strlen(names[i]) >= sizeof(buf))
    {
      continue;
    }

    // Ghép đường dẫn: path + "/" + name
    strcpy(buf, path);
    char *p = buf + strlen(buf);
    *p++ = '/';
    strcpy(p, names[i]);

    // Gọi đệ quy và cộng dồn dung lượng
    total_size += du(buf, a, s, 0);
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
 * @return 0 nếu thành công, 1 nếu lỗi cú pháp.
 */
int main(int argc, char *argv[])
{
  char *path = ".";
  int a = 0;
  int s = 0;

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
    else if (argv[i][0] != '-')
    {
      path = argv[i];
    }
    else
    {
      fprintf(2, "usage: du [path] [-a] [-s]\n");
      exit(1);
    }
  }

  du(path, a, s, 1);

  exit(0);
}
