/**
 * @file tree.c
 * @brief Hiển thị cây thư mục bằng các system call của xv6.
 *
 * Mỗi thư mục được đọc hai lượt để biết entry nào là con cuối cùng. Cách này
 * cho phép in đúng các nhánh của cây mà không phải giữ một mảng tên lớn trong
 * stack nhỏ của xv6. Một file descriptor riêng được dùng cho lượt đếm vì xv6
 * không cung cấp lseek() cho user program.
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"

#define UNLIMITED_DEPTH -1
#define MAX_TREE_DEPTH 64

/**
 * @brief In hướng dẫn sử dụng chương trình.
 *
 * @return Không trả về giá trị.
 */
static void
print_usage(void)
{
  printf("usage: tree [path] [-L depth] [-d]\n");
}

/**
 * @brief Chuyển một chuỗi chữ số thành độ sâu không âm.
 *
 * @param value Chuỗi cần chuyển đổi.
 * @param depth Nơi lưu kết quả chuyển đổi.
 * @return 0 nếu chuỗi hợp lệ, -1 nếu chuỗi rỗng hoặc chứa ký tự không phải số.
 */
static int
parse_depth(char *value, int *depth)
{
  if(value[0] == '\0')
    return -1;

  for(int i = 0; value[i] != '\0'; i++){
    if(value[i] < '0' || value[i] > '9')
      return -1;
  }

  *depth = atoi(value);
  return 0;
}

/**
 * @brief Đọc entry hợp lệ tiếp theo từ một thư mục.
 *
 * Hàm bỏ qua entry chưa sử dụng, "." và "..". Tên được sao chép sang buffer
 * có thêm ký tự kết thúc vì de.name có thể chiếm đủ DIRSIZ byte.
 *
 * @param fd File descriptor của thư mục đang đọc.
 * @param name Buffer có ít nhất DIRSIZ + 1 byte để nhận tên entry.
 * @return 1 nếu đọc được entry, 0 khi hết thư mục, -1 nếu read() thất bại hoặc
 *         trả về một entry không đầy đủ.
 */
static int
read_next_entry(int fd, char *name)
{
  struct dirent entry;
  int bytes_read;

  while((bytes_read = read(fd, &entry, sizeof(entry))) == sizeof(entry)){
    if(entry.inum == 0)
      continue;

    memmove(name, entry.name, DIRSIZ);
    name[DIRSIZ] = '\0';

    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    return 1;
  }

  return bytes_read == 0 ? 0 : -1;
}

/**
 * @brief Ghép đường dẫn cha và tên entry thành đường dẫn con.
 *
 * @param child_path Buffer nhận đường dẫn con, có kích thước MAXPATH.
 * @param parent_path Đường dẫn thư mục cha.
 * @param name Tên entry con.
 * @return 0 nếu ghép thành công, -1 nếu đường dẫn vượt quá MAXPATH.
 */
static int
build_child_path(char *child_path, char *parent_path, char *name)
{
  uint parent_length = strlen(parent_path);
  uint name_length = strlen(name);
  int needs_slash = parent_length == 0 || parent_path[parent_length - 1] != '/';

  if(parent_length + needs_slash + name_length + 1 > MAXPATH)
    return -1;

  strcpy(child_path, parent_path);
  char *next = child_path + parent_length;

  if(needs_slash)
    *next++ = '/';

  strcpy(next, name);
  return 0;
}

/**
 * @brief Kiểm tra một entry có được hiển thị theo tùy chọn hiện tại không.
 *
 * Descriptor chỉ tồn tại trong lúc kiểm tra để lượt đếm không làm rò rỉ tài
 * nguyên. Lỗi open() hoặc fstat() khiến entry không được tính vào output.
 *
 * @param path Đường dẫn đầy đủ của entry.
 * @param only_dir Khác 0 nếu chỉ hiển thị thư mục.
 * @return 1 nếu entry được hiển thị, ngược lại trả về 0.
 */
static int
is_visible_entry(char *path, int only_dir)
{
  struct stat entry_stat;
  int entry_fd = open(path, O_RDONLY);

  if(entry_fd < 0)
    return 0;

  if(fstat(entry_fd, &entry_stat) < 0){
    close(entry_fd);
    return 0;
  }

  close(entry_fd);
  return !only_dir || entry_stat.type == T_DIR;
}

/**
 * @brief Đếm số entry sẽ được hiển thị trong một thư mục.
 *
 * Kết quả được dùng để nhận biết entry cuối cùng mà không lưu toàn bộ danh
 * sách tên trên stack của mỗi lời gọi đệ quy.
 *
 * @param path Đường dẫn thư mục cần đếm.
 * @param only_dir Khác 0 nếu chỉ đếm thư mục.
 * @return Số entry được hiển thị, hoặc -1 nếu không thể mở/đọc thư mục.
 */
static int
count_visible_entries(char *path, int only_dir)
{
  char name[DIRSIZ + 1];
  char child_path[MAXPATH];
  int entry_count = 0;
  int read_status;
  int scan_fd = open(path, O_RDONLY);

  if(scan_fd < 0){
    printf("tree: cannot open %s\n", path);
    return -1;
  }

  while((read_status = read_next_entry(scan_fd, name)) == 1){
    if(build_child_path(child_path, path, name) < 0)
      continue;

    if(is_visible_entry(child_path, only_dir))
      entry_count++;
  }

  close(scan_fd);

  if(read_status < 0){
    printf("tree: cannot read %s\n", path);
    return -1;
  }

  return entry_count;
}

/**
 * @brief In một node cùng các nhánh nối với tổ tiên của nó.
 *
 * @param name Tên node cần in.
 * @param level Số tổ tiên nằm giữa node và đường dẫn gốc.
 * @param is_last Khác 0 nếu node là con cuối cùng của thư mục cha.
 * @param ancestor_is_last Trạng thái con cuối cùng của từng node tổ tiên.
 * @return Không trả về giá trị.
 */
static void
print_node(char *name, int level, int is_last, int *ancestor_is_last)
{
  for(int i = 0; i < level; i++){
    if(ancestor_is_last[i])
      printf("    ");
    else
      printf("│   ");
  }

  printf(is_last ? "└── %s\n" : "├── %s\n", name);
}

/**
 * @brief Duyệt và hiển thị các entry con của một thư mục.
 *
 * Hàm đếm trước các entry nhìn thấy để chọn đúng ký hiệu cho node cuối, sau đó
 * đọc descriptor chính để in và đệ quy theo chiều sâu.
 *
 * @param fd File descriptor của thư mục đang duyệt.
 * @param path Đường dẫn của thư mục đang duyệt.
 * @param depth Số mức con còn được phép duyệt; -1 nghĩa là không giới hạn.
 * @param only_dir Khác 0 nếu chỉ hiển thị thư mục.
 * @param level Số tổ tiên giữa entry sắp in và đường dẫn gốc.
 * @param ancestor_is_last Trạng thái con cuối cùng của từng node tổ tiên.
 * @return Không trả về giá trị.
 */
static void
tree_recursive(int fd, char *path, int depth, int only_dir, int level,
               int *ancestor_is_last)
{
  struct stat child_stat;
  char name[DIRSIZ + 1];
  char child_path[MAXPATH];
  int visible_index = 0;
  int read_status;

  if(depth == 0)
    return;

  int visible_count = count_visible_entries(path, only_dir);
  if(visible_count < 0)
    return;

  while((read_status = read_next_entry(fd, name)) == 1){
    if(build_child_path(child_path, path, name) < 0){
      printf("tree: path too long: %s/%s\n", path, name);
      continue;
    }

    int child_fd = open(child_path, O_RDONLY);
    if(child_fd < 0){
      printf("tree: cannot open %s\n", child_path);
      continue;
    }

    if(fstat(child_fd, &child_stat) < 0){
      printf("tree: cannot stat %s\n", child_path);
      close(child_fd);
      continue;
    }

    if(only_dir && child_stat.type != T_DIR){
      close(child_fd);
      continue;
    }

    int is_last = visible_index == visible_count - 1;
    visible_index++;
    print_node(name, level, is_last, ancestor_is_last);

    if(child_stat.type == T_DIR){
      if(level >= MAX_TREE_DEPTH){
        printf("tree: directory nesting too deep: %s\n", child_path);
      } else {
        int next_depth = depth == UNLIMITED_DEPTH
                           ? UNLIMITED_DEPTH
                           : depth - 1;
        ancestor_is_last[level] = is_last;
        tree_recursive(child_fd, child_path, next_depth, only_dir, level + 1,
                       ancestor_is_last);
      }
    }

    close(child_fd);
  }

  if(read_status < 0)
    printf("tree: cannot read %s\n", path);
}

/**
 * @brief Hiển thị đường dẫn gốc và bắt đầu quá trình duyệt cây.
 *
 * @param fd File descriptor đã mở của đường dẫn gốc.
 * @param path Đường dẫn gốc cần hiển thị.
 * @param depth Số mức con tối đa; -1 nghĩa là không giới hạn.
 * @param only_dir Khác 0 nếu chỉ hiển thị thư mục.
 * @return Không trả về giá trị.
 */
static void
tree(int fd, char *path, int depth, int only_dir)
{
  struct stat root_stat;
  int ancestor_is_last[MAX_TREE_DEPTH] = {0};

  if(fstat(fd, &root_stat) < 0){
    printf("tree: cannot stat %s\n", path);
    return;
  }

  if(only_dir && root_stat.type != T_DIR)
    return;

  printf("%s\n", path);

  if(root_stat.type == T_DIR)
    tree_recursive(fd, path, depth, only_dir, 0, ancestor_is_last);
}

/**
 * @brief Phân tích tham số, mở đường dẫn gốc và chạy chương trình tree.
 *
 * @param argc Số lượng phần tử trong argv.
 * @param argv Các tham số dòng lệnh theo cú pháp tree [path] [-L depth] [-d].
 * @return Không trả về vì xv6 kết thúc chương trình bằng exit().
 */
int
main(int argc, char *argv[])
{
  char *root_path = ".";
  int max_depth = UNLIMITED_DEPTH;
  int only_dir = 0;
  int path_was_set = 0;

  for(int i = 1; i < argc; i++){
    if(strcmp(argv[i], "-d") == 0){
      only_dir = 1;
    } else if(strcmp(argv[i], "-L") == 0){
      if(i + 1 >= argc || parse_depth(argv[i + 1], &max_depth) < 0){
        printf("tree: invalid depth for -L\n");
        print_usage();
        exit(1);
      }
      i++;
    } else if(argv[i][0] == '-'){
      print_usage();
      exit(1);
    } else if(path_was_set){
      print_usage();
      exit(1);
    } else {
      root_path = argv[i];
      path_was_set = 1;
    }
  }

  int root_fd = open(root_path, O_RDONLY);
  if(root_fd < 0){
    printf("tree: cannot open %s\n", root_path);
    exit(1);
  }

  tree(root_fd, root_path, max_depth, only_dir);
  close(root_fd);
  exit(0);
}
