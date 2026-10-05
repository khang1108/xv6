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
int du_recursive(char *path, int a, int s, int is_root)
{
  struct stat st;
  
  if (stat(path, &st) < 0) {
    fprintf(2, "du: cannot stat %s\n", path);
    return 0;
  }

  if (st.type == T_FILE || st.type == T_DEVICE) {
    if (is_root) {
      printf("%d\t%s\n", (int)st.size, path);
    } else if (a && !s) {
      printf("%d\t%s\n", (int)st.size, path);
    }
    return (int)st.size;
  }

  int fd = open(path, 0);
  if (fd < 0) {
    fprintf(2, "du: cannot open %s\n", path);
    return 0;
  }

  struct dirent de;
  int capacity = 16;
  int count = 0;
  
  char (*names)[DIRSIZ + 1] = malloc(capacity * (DIRSIZ + 1));
  if (!names) {
    close(fd);
    return 0;
  }

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;

    if (count >= capacity) {
      int new_cap = capacity * 2;
      char (*new_names)[DIRSIZ + 1] = malloc(new_cap * (DIRSIZ + 1));
      memmove(new_names, names, count * (DIRSIZ + 1));
      free(names);
      names = new_names;
      capacity = new_cap;
    }

    memmove(names[count], de.name, DIRSIZ);
    names[count][DIRSIZ] = '\0';
    count++;
  }
  
  close(fd);

  int total_size = 0; 

  for (int i = 0; i < count; i++) {
    char buf[MAXPATH];
    if (strlen(path) + 1 + strlen(names[i]) >= sizeof(buf)) {
      continue;
    }

    strcpy(buf, path);
    char *p = buf + strlen(buf);
    *p++ = '/';
    strcpy(p, names[i]);

    total_size += du_recursive(buf, a, s, 0);
  }

  free(names);

  if (is_root) {
    printf("%d\t%s\n", total_size, path);
  } else if (!s) {
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

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0) {
      a = 1;
    } else if (strcmp(argv[i], "-s") == 0) {
      s = 1;
    } else if (argv[i][0] != '-') {
      path = argv[i];
    } else {
      fprintf(2, "usage: du [path] [-a] [-s]\n");
      exit(1);
    }
  }

  du_recursive(path, a, s, 1);
  
  exit(0);
}
