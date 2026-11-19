#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


//! New definition
// System call trace: lưu mặt nạ bitmask theo dõi syscall vào trường trace_mask của tiến trình hiện hành
uint64 sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}

// System call ptree: thu thập danh sách tối đa max tiến trình đang hoạt động đổ về vùng đệm uaddr của user
uint64 sys_ptree(void)
{
  uint64 uaddr;
  int max;

  argaddr(0,  &uaddr);
  argint(1, &max);

  return get_ptree(uaddr, max);
}

// System call sysinfo: thu thập thông tin tài nguyên hệ thống (freemem, nproc) và sao chép ra không gian user
uint64 sys_sysinfo(void)
{
  uint64 uaddr;
  argaddr(0, &uaddr);

  struct sysinfo info;
  info.freemem = count_freemem();
  info.nproc = count_proc();

  struct proc* p = myproc();
  if(copyout(p->pagetable, uaddr, (char*)&info, sizeof(info)) < 0){
    return -1;
  }
  return 0;
}

uint64 sys_pgaccess(void)
{
  uint64 base, mask_addr;
  int len;
  
  argaddr(0, &base);
  argint(1, &len);
  argaddr(2, &mask_addr);

  if(len < 0 || len > 32) return -1;

  struct proc *p = myproc();
  uint curMask = 0; // Dùng để lưu lại các page đang được bật

  for(int i = 0; i < len; i++){
    uint64 first_addr_pagetable = base + i * PGSIZE;

    if(first_addr_pagetable >= MAXVA) return -1;

    // Tìm Page Table Entry (PTE) thông qua việc walk trên pagetable
    // đây là ô nhớ 64bit dùng để mô tả 1 trang bộ nhớ 4096 bytes
    // 54 bit cao chứa địa chỉ trang vật lý, 10 bit thấp chứa các bit cờ
    pte_t *pte = walk(p->pagetable, first_addr_pagetable, 0);

    // Vị trí bit :   7     6     5     4   |   3     2     1     0
    // Tên cờ     :   D     A     G     U   |   X     W     R     V

    // Kiểm tra nếu PTE_V và PTE_A được bật thì đánh dấu bit 
    // tương ứng cho bảng trang trong curMask và tắt bit PTE_A đi 
    // ta cần tắt bit PTE_A để reset chu kỳ tránh lần đọc sau bị báo trùng.
    if(pte != 0 && (*pte & PTE_V) && (*pte & PTE_A)){
      curMask |= (1U << i);
      *pte &= ~PTE_A;
    }
  }

  if(copyout(p->pagetable, mask_addr, (char*)&curMask, 
      sizeof(curMask)) < 0) return -1;

  return 0;
}