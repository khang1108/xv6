#ifndef _PTREE_H_
#define _PTREE_H_

struct ptreeinfo {
  int pid;
  int ppid; // Parent
  int state;
  uint64 memsize;
  char name[16];
};

#endif
