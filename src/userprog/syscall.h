#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

#define MAX_FD 128

extern struct lock filesys_lock;

void syscall_init (void);
void sys_exit (int status);

#endif /* userprog/syscall.h */
