#ifndef XV6_USYSCALL_H
#define XV6_USYSCALL_H

//Cấu trúc dữ liệu dùng để ánh xạ trang chia sẻ PID từ kernel sang user
//Kernel ghi PID của tiến trình, user chỉ được đọc, k ghi
struct usyscall {
  int pid; //PID của tiến trình sở hữu trang đang được ánh xạ
};

#endif