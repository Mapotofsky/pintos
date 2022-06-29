#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/*---------------my code-------------*/
#include <devices/shutdown.h>
#include <string.h>
#include <filesys/file.h>
#include <devices/input.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include "process.h"
#include "pagedir.h"
#include <threads/vaddr.h>
#include <filesys/filesys.h>
# define max_syscall 20
# define USER_VADDR_BOUND (void*) 0x08048000

static void (*syscalls[max_syscall])(struct intr_frame *);

void syscall_halt(struct intr_frame* f); /* syscall halt. */
void syscall_exit(struct intr_frame* f); /* syscall exit. */
void syscall_exec(struct intr_frame* f); /* syscall exec. */

void syscall_create(struct intr_frame* f);
void syscall_remove(struct intr_frame* f);
void syscall_open(struct intr_frame* f);
void syscall_read(struct intr_frame* f);
void syscall_write(struct intr_frame* f);
void syscall_close(struct intr_frame* f);
void syscall_seek(struct intr_frame* f);
void syscall_tell(struct intr_frame* f);
void syscall_wait(struct intr_frame* f);
void syscall_fsize(struct intr_frame* f);

static void syscall_handler (struct intr_frame *);
struct thread_file * find_f_name(int fd);
/*---------------my code-------------*/

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /*---------------my code-------------*/
  syscalls[SYS_HALT] = &syscall_halt;
  syscalls[SYS_EXIT] = &syscall_exit;
  syscalls[SYS_EXEC] = &syscall_exec;
  syscalls[SYS_WAIT] = &syscall_wait;
  syscalls[SYS_CREATE] = &syscall_create;
  syscalls[SYS_REMOVE] = &syscall_remove;
  syscalls[SYS_OPEN] = &syscall_open;
  syscalls[SYS_WRITE] = &syscall_write;
  syscalls[SYS_SEEK] = &syscall_seek;
  syscalls[SYS_TELL] = &syscall_tell;
  syscalls[SYS_CLOSE] =&syscall_close;
  syscalls[SYS_READ] = &syscall_read;
  syscalls[SYS_FILESIZE] = &syscall_fsize;
  /*---------------my code-------------*/
}

/*---------------my code-------------*/
// 3.1.5 Accessing User Memory
static int 
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}

// 判断指针指向的地址是否合法
void * 
ckptr2(const void *vaddr)
{
  // 是否为用户地址
  if (!is_user_vaddr(vaddr)) {
    exit_spe ();
  }
  // 检查页
  void *ptr = pagedir_get_page (thread_current()->pagedir, vaddr);
  if (!ptr) {
    exit_spe ();
  }
  // 检查内容
  uint8_t *check_byteptr = (uint8_t *) vaddr;
  for (uint8_t i = 0; i < 4; i++) {
    if (get_user(check_byteptr + i) == -1) {
      exit_spe ();
    }
  }

  return ptr;
}

void 
syscall_halt (struct intr_frame* f)
{
  shutdown_power_off();
}

void 
syscall_exit (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);  // 检测第一个参数的合法性
  *user_ptr++;
  thread_current()->st_exit = *user_ptr;
  thread_exit ();
}

void 
syscall_create(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  ckptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_create ((const char *)*user_ptr, *(user_ptr+1));
  release_lock_f ();
}

void 
syscall_exec (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);  // 检查地址
  ckptr2 (*(user_ptr + 1));  // 检查值
  *user_ptr++;
  f->eax = process_execute((char*)* user_ptr);
}

void 
syscall_open (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  ckptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  struct file *file_opened = filesys_open((const char *)*user_ptr);
  release_lock_f ();
  struct thread *t = thread_current();
  if (file_opened) {
    struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
    thread_file_temp->file = file_opened;
    thread_file_temp->fd = t->f_folder++;
    list_push_back (&t->files, &thread_file_temp->file_elem);
    f->eax = thread_file_temp->fd;
  }
  else {
    f->eax = -1;
  }
}

void 
syscall_wait (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  *user_ptr++;
  f->eax = process_wait(*user_ptr);
}

void 
syscall_remove(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  ckptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_remove ((const char *)*user_ptr);
  release_lock_f ();
}

void 
syscall_seek(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 5);
  *user_ptr++;
  struct thread_file *file_temp = find_f_name (*user_ptr);
  if (file_temp) {
    acquire_lock_f ();
    file_seek (file_temp->file, *(user_ptr+1));
    release_lock_f ();
  }
}

void 
syscall_write (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 7);
  ckptr2 (*(user_ptr + 6));
  *user_ptr++;
  int temp2 = *user_ptr;
  const char * buffer = (const char *)*(user_ptr + 1);
  off_t size = *(user_ptr + 2);
  if (temp2 != 1) {
    struct thread_file * thread_file_temp = find_f_name (*user_ptr);
    if (thread_file_temp) {
      acquire_lock_f ();
      // 返回写入的字节数
      f->eax = file_write (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else {
      // 无法写入
      f->eax = 0;
    }
  }
  else {
    // 打印到控制台 fd1
    putbuf(buffer, size);
    f->eax = size;
  }
}

void 
syscall_fsize (struct intr_frame* f) {
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file * thread_file_temp = find_f_name (*user_ptr);
  if (thread_file_temp) {
    acquire_lock_f ();
    f->eax = file_length (thread_file_temp->file);
    release_lock_f ();
  } 
  else {
    f->eax = -1;
  }
}

void 
syscall_tell (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file *thread_file_temp = find_f_name (*user_ptr);
  if (thread_file_temp) {
    acquire_lock_f ();
    f->eax = file_tell (thread_file_temp->file);
    release_lock_f ();
  }
  else {
    f->eax = -1;
  }
}

void 
syscall_close (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  ckptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file * opened_file = find_f_name (*user_ptr);
  if (opened_file) {
    acquire_lock_f ();
    file_close (opened_file->file);
    release_lock_f ();
    list_remove (&opened_file->file_elem);
    free (opened_file);
  }
}

bool 
is_valid_pointer (void* esp,uint8_t argc){
  for (uint8_t i = 0; i < argc; ++i) {
    if((!is_user_vaddr (esp)) || 
      (pagedir_get_page (thread_current()->pagedir, esp) == NULL)) {
      return false;
    }
  }
  return true;
}

/*systemcall read*/
void 
syscall_read (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  *user_ptr++;
  int fd = *user_ptr;
  int i;
  uint8_t * buffer = (uint8_t*)*(user_ptr + 1);
  off_t size = *(user_ptr + 2);
  if (!is_valid_pointer (buffer + size, 1) || !is_valid_pointer (buffer, 1)) {
    exit_spe ();
  }

  if (fd == 0) {
    // 标准输入
    for (i = 0; i < size; i++)
      buffer[i] = input_getc();
    f->eax = size;
  }
  else {
    struct thread_file * thread_file_temp = find_f_name (*user_ptr);
    if (thread_file_temp) {
      acquire_lock_f ();
      f->eax = file_read (thread_file_temp->file, buffer, size);
      release_lock_f ();
    }
    else {
      f->eax = -1;
    }
  }
}

/*gain the system handler*/
static void
syscall_handler (struct intr_frame *f UNUSED)
{
  /*---------------my code-------------*/
  int * p = f->esp;
  ckptr2 (p + 1);
  int type = *(int *)f->esp;
  if(type >= max_syscall || type <= 0) {
    exit_spe ();
  }
  syscalls[type](f);
  /*---------------my code-------------*/
}

void 
exit_spe (void)
{
  thread_current()->st_exit = -1;
  thread_exit ();
}

struct thread_file * 
find_f_name (int file_id)
{
  struct list_elem *e;
  struct thread_file * thread_file_temp = NULL;
  struct list *files = &thread_current ()->files;
  for (e = list_begin (files); e != list_end (files); e = list_next (e)) {
    thread_file_temp = list_entry (e, struct thread_file, file_elem);
    if (file_id == thread_file_temp->fd)
      return thread_file_temp;
  }
  return false;
}
/*---------------my code-------------*/
