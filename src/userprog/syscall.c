#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_nr = *(int *) f->esp;

  switch (syscall_nr)
    {
    case SYS_HALT:
      shutdown_power_off ();
      break;

    case SYS_EXIT:
      {
        int status = *((int *) f->esp + 1);
        thread_current ()->exit_status = status;
        thread_exit ();
        break;
      }

    case SYS_WRITE:
      {
        int fd = *((int *) f->esp + 1);
        const void *buffer = (const void *) *((int *) f->esp + 2);
        unsigned size = *((unsigned *) f->esp + 3);

        if (fd == 1)
          {
            putbuf (buffer, size);
            f->eax = size;
          }
        break;
      }

    default:
      printf ("system call %d not implemented!\n", syscall_nr);
      thread_exit ();
    }
}
