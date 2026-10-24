#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

struct child_entry {
  tid_t tid;                     /* Child's thread ID / pid */
  int exit_status;               /* Exit code (-1 by default or if killed) */
  bool has_waited;               /* True if parent already called wait */
  bool load_success;             /* True if ELF load was successful */
  char *fn_copy;                 /* Full command line string */
  int ref_cnt;                   /* Reference count (freed when 0) */

  struct semaphore sema_load;    /* Signals when load() completes */
  struct semaphore sema_exit;    /* Signals when process terminates */
  struct list_elem elem;         /* Element for parent's child_list */
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
