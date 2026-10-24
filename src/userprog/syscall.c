#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
static void check_valid_ptr (const void *vaddr);
static void check_valid_buffer (const void *buffer, unsigned size);
static void check_valid_string (const char *str);
static void get_args (struct intr_frame *f, int *args, int n);

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Terminates the current user process with the given status. */
void
sys_exit (int status)
{
  struct thread *cur = thread_current ();
  cur->exit_status = status;
  thread_exit ();
}

/* Validates that a pointer points to user memory and is mapped in the page table. */
static void
check_valid_ptr (const void *vaddr)
{
  if (vaddr == NULL || !is_user_vaddr (vaddr) ||
      pagedir_get_page (thread_current ()->pagedir, vaddr) == NULL)
    {
      sys_exit (-1);
    }
}

/* Validates that an entire buffer of `size` bytes lies in valid mapped user memory. */
static void
check_valid_buffer (const void *buffer, unsigned size)
{
  if (size == 0)
    return;

  if (buffer == NULL)
    sys_exit (-1);

  uintptr_t start = (uintptr_t) buffer;
  uintptr_t end = start + size - 1;

  /* Check for 32-bit address wraparound. */
  if (end < start)
    sys_exit (-1);

  /* Verify starting and ending addresses. */
  check_valid_ptr ((const void *) start);
  check_valid_ptr ((const void *) end);

  /* Verify every intermediate page boundary. */
  uintptr_t page;
  for (page = (start & ~PGMASK) + PGSIZE; page <= end; page += PGSIZE)
    {
      check_valid_ptr ((const void *) page);
    }
}

/* Validates that a null-terminated string lies entirely in valid mapped user memory. */
static void
check_valid_string (const char *str)
{
  if (str == NULL)
    sys_exit (-1);

  while (true)
    {
      check_valid_ptr (str);
      if (*str == '\0')
        break;
      str++;
    }
}

/* Safely retrieves N arguments from the user stack into ARGS array. */
static void
get_args (struct intr_frame *f, int *args, int n)
{
  check_valid_buffer ((int *) f->esp + 1, n * sizeof (int));
  for (int i = 0; i < n; i++)
    {
      args[i] = *((int *) f->esp + 1 + i);
    }
}

static void
syscall_handler (struct intr_frame *f) 
{
  /* Validate that the stack pointer is valid for reading the syscall number. */
  check_valid_buffer (f->esp, sizeof (int));
  int syscall_nr = *(int *) f->esp;
  int args[3];

  switch (syscall_nr)
    {
    case SYS_HALT:
      shutdown_power_off ();
      break;

    case SYS_EXIT:
      get_args (f, args, 1);
      sys_exit (args[0]);
      break;

    case SYS_EXEC:
      get_args (f, args, 1);
      check_valid_string ((const char *) args[0]);
      f->eax = process_execute ((const char *) args[0]);
      break;

    case SYS_WAIT:
      get_args (f, args, 1);
      f->eax = process_wait ((tid_t) args[0]);
      break;

    case SYS_CREATE:
      get_args (f, args, 2);
      check_valid_string ((const char *) args[0]);
      lock_acquire (&filesys_lock);
      f->eax = filesys_create ((const char *) args[0], (unsigned) args[1]);
      lock_release (&filesys_lock);
      break;

    case SYS_REMOVE:
      get_args (f, args, 1);
      check_valid_string ((const char *) args[0]);
      lock_acquire (&filesys_lock);
      f->eax = filesys_remove ((const char *) args[0]);
      lock_release (&filesys_lock);
      break;

    case SYS_OPEN:
      {
        get_args (f, args, 1);
        check_valid_string ((const char *) args[0]);
        lock_acquire (&filesys_lock);
        struct file *f_obj = filesys_open ((const char *) args[0]);
        if (f_obj == NULL)
          {
            lock_release (&filesys_lock);
            f->eax = -1;
            break;
          }
        struct thread *cur = thread_current ();
        int fd = -1;
        for (int i = 2; i < MAX_FD; i++)
          {
            if (cur->fd_table[i] == NULL)
              {
                fd = i;
                cur->fd_table[i] = f_obj;
                break;
              }
          }
        if (fd == -1)
          file_close (f_obj);
        lock_release (&filesys_lock);
        f->eax = fd;
        break;
      }

    case SYS_FILESIZE:
      {
        get_args (f, args, 1);
        int fd = args[0];
        if (fd < 2 || fd >= MAX_FD)
          {
            f->eax = -1;
            break;
          }
        struct thread *cur = thread_current ();
        struct file *f_obj = cur->fd_table[fd];
        if (f_obj == NULL)
          {
            f->eax = -1;
            break;
          }
        lock_acquire (&filesys_lock);
        f->eax = file_length (f_obj);
        lock_release (&filesys_lock);
        break;
      }

    case SYS_READ:
      {
        get_args (f, args, 3);
        int fd = args[0];
        void *buffer = (void *) args[1];
        unsigned size = (unsigned) args[2];

        check_valid_buffer (buffer, size);
        if (size == 0)
          {
            f->eax = 0;
            break;
          }
        if (fd == 0)
          {
            uint8_t *buf = (uint8_t *) buffer;
            for (unsigned i = 0; i < size; i++)
              buf[i] = input_getc ();
            f->eax = size;
            break;
          }
        if (fd < 2 || fd >= MAX_FD)
          {
            f->eax = -1;
            break;
          }
        struct thread *cur = thread_current ();
        struct file *f_obj = cur->fd_table[fd];
        if (f_obj == NULL)
          {
            f->eax = -1;
            break;
          }
        lock_acquire (&filesys_lock);
        f->eax = file_read (f_obj, buffer, size);
        lock_release (&filesys_lock);
        break;
      }

    case SYS_WRITE:
      {
        get_args (f, args, 3);
        int fd = args[0];
        const void *buffer = (const void *) args[1];
        unsigned size = (unsigned) args[2];

        check_valid_buffer (buffer, size);
        if (size == 0)
          {
            f->eax = 0;
            break;
          }
        if (fd == 1)
          {
            putbuf (buffer, size);
            f->eax = size;
            break;
          }
        if (fd < 2 || fd >= MAX_FD)
          {
            f->eax = -1;
            break;
          }
        struct thread *cur = thread_current ();
        struct file *f_obj = cur->fd_table[fd];
        if (f_obj == NULL)
          {
            f->eax = -1;
            break;
          }
        lock_acquire (&filesys_lock);
        f->eax = file_write (f_obj, buffer, size);
        lock_release (&filesys_lock);
        break;
      }

    case SYS_SEEK:
      {
        get_args (f, args, 2);
        int fd = args[0];
        unsigned position = (unsigned) args[1];
        if (fd >= 2 && fd < MAX_FD)
          {
            struct thread *cur = thread_current ();
            struct file *f_obj = cur->fd_table[fd];
            if (f_obj != NULL)
              {
                lock_acquire (&filesys_lock);
                file_seek (f_obj, position);
                lock_release (&filesys_lock);
              }
          }
        break;
      }

    case SYS_TELL:
      {
        get_args (f, args, 1);
        int fd = args[0];
        if (fd < 2 || fd >= MAX_FD)
          {
            f->eax = -1;
            break;
          }
        struct thread *cur = thread_current ();
        struct file *f_obj = cur->fd_table[fd];
        if (f_obj == NULL)
          {
            f->eax = -1;
            break;
          }
        lock_acquire (&filesys_lock);
        f->eax = file_tell (f_obj);
        lock_release (&filesys_lock);
        break;
      }

    case SYS_CLOSE:
      {
        get_args (f, args, 1);
        int fd = args[0];
        if (fd >= 2 && fd < MAX_FD)
          {
            struct thread *cur = thread_current ();
            struct file *f_obj = cur->fd_table[fd];
            if (f_obj != NULL)
              {
                lock_acquire (&filesys_lock);
                file_close (f_obj);
                cur->fd_table[fd] = NULL;
                lock_release (&filesys_lock);
              }
          }
        break;
      }

    default:
      sys_exit (-1);
    }
}
