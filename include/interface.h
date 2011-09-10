#ifndef __INTERFACE_H__
#define __INTERFACE_H__
#include "common.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "ipc.h"

#define MAX_AUX_VEC_ENTRIES 25 

struct iovec {
     void  *iov_base;    /* Starting address */
     size_t iov_len;     /* Number of bytes to transfer */
};

/* Naming : SYS : system call
 *
 */
/* scheduling */
int sc_register_waitqueue(struct wait_struct *waitqueue);
int sc_unregister_waitqueue(struct wait_struct *waitqueue);
int sc_wakeUp(struct wait_struct *waitqueue,struct task_struct * p);
int sc_wait(struct wait_struct *waitqueue,int ticks);
int sc_sleep(int ticks); /* each tick is 100HZ or 10ms */
unsigned long SYS_sc_fork();
unsigned long SYS_sc_clone(int (*fn)(void *), void *child_stack,int flags,void *args);
int SYS_sc_exit(int status);
int SYS_sc_kill(unsigned long pid,unsigned long signal);
unsigned long SYS_sc_execve(unsigned char *file,unsigned char **argv,unsigned char *env);
int sc_threadlist( char *arg1,char *arg2);
unsigned long sc_createKernelThread(int (*fn)(void *),unsigned char *argv,unsigned char *name);
void sc_schedule();

/* mm */
unsigned long mm_getFreePages(int gfp_mask, unsigned long order);
int mm_putFreePages(unsigned long addr, unsigned long order);
int mm_printFreeAreas(char *arg1,char *arg2);

void kmem_cache_free (kmem_cache_t *cachep, void *objp);
extern kmem_cache_t *kmem_cache_create(const char *, long,long, unsigned long,void (*)(void *, kmem_cache_t *, unsigned long),void (*)(void *, kmem_cache_t *, unsigned long));
void *kmem_cache_alloc (kmem_cache_t *cachep, int flags);
void *mm_malloc (long size, int flags);
void mm_free (const void *objp);
#define alloc_page() mm_getFreePages(0, 0)
#define memset ut_memset

#define ut_free mm_free
#define ut_malloc(x) mm_malloc(x,0)

/* vm */
int vm_printMmaps(char *arg1,char *arg2);
struct vm_area_struct *vm_findVma(struct mm_struct *mm,unsigned long addr, unsigned long len);
long SYS_vm_mmap(unsigned long fd, unsigned long addr, unsigned long len,unsigned long prot, unsigned long flags, unsigned long pgoff);
unsigned long SYS_vm_brk(unsigned long addr);
int SYS_vm_munmap(unsigned long addr, unsigned long len);
int SYS_vm_mprotect(const void *addr, int len, int prot);
unsigned long vm_brk(unsigned long addr, unsigned long len);
long vm_mmap(struct file *fp, unsigned long addr, unsigned long len,unsigned long prot, unsigned long flags, unsigned long pgoff);
int vm_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len);

/* page cache */
int pc_init(unsigned char *start_addr,unsigned long len);
int pc_stats(char *arg1,char *arg2);
int pc_pageDirted(struct page *p);
int pc_pagecleaned(struct page *page);
struct page *pc_getInodePage(struct inode *inode,unsigned long offset);
unsigned long pc_getVmaPage(struct vm_area_struct *vma,unsigned long offset);
int pc_insertPage(struct inode *inode,struct page *page);
int pc_removePage(struct page *page);
int pc_putFreePage(struct page *page);
page_struct_t *pc_getFreePage();

/*vfs */
unsigned long fs_registerFileSystem(struct filesystem *fs);
struct inode *fs_getInode(char *filename);
unsigned long fs_putInode(struct inode *inode);
unsigned long fs_printInodes(char *arg1,char *arg2);
unsigned long fs_open(char *filename,int mode,int flags);
struct page *fs_genericRead(struct inode *inode,unsigned long offset);
ssize_t fs_read(struct file *fp ,unsigned char *buff ,unsigned long len);
unsigned long fs_fadvise(struct inode *inode,unsigned long offset, unsigned long len,int advise);
unsigned long fs_lseek(struct file *fp ,unsigned long offset, int whence);
unsigned long fs_loadElfLibrary(struct file  *file,unsigned long tmp_stack, unsigned long stack_len,unsigned long aux_addr);
ssize_t fs_write(struct file *file,unsigned char *buff ,unsigned long len);
unsigned long fs_fdatasync(struct file *file);

ssize_t SYS_fs_writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t SYS_fs_readv(int fd, const struct iovec *iov, int iovcnt);
unsigned long SYS_fs_open(char *filename,int mode,int flags);
unsigned long SYS_fs_lseek(unsigned long fd ,unsigned long offset, int whence);
ssize_t SYS_fs_write(unsigned long fd ,unsigned char *buff ,unsigned long len);
ssize_t SYS_fs_read(unsigned long fd ,unsigned char *buff ,unsigned long len);
unsigned long SYS_fs_close(unsigned long fd);
unsigned long SYS_fs_fdatasync(unsigned long fd );
unsigned long SYS_fs_fadvise(unsigned long fd,unsigned long offset, unsigned long len,int advise);

/* Utilities */
void ut_showTrace(unsigned long *stack_top);
int ut_strcmp(char *str1, char *str2);
void ut_printf (const char *format, ...);
void ut_memcpy(unsigned char *dest, unsigned char *src, long len);
void ut_memset(unsigned char *dest, unsigned char val, long len);
int ut_memcmp(unsigned char *m1, unsigned char *m2,int len);
char *ut_strcpy(char *dest, const char *src);
char *ut_strncpy(char *dest, const char *src,int n);
char *ut_strcat(char *dest, const char *src);
int ut_strlen(const char * s);
unsigned long ut_atol(char *p);
unsigned int ut_atoi(char *p);
int ut_sprintf(char * buf, const char *fmt, ...);
int ut_snprintf(char * buf, size_t size, const char *fmt, ...);

/* architecture depended */
void ar_registerInterrupt(uint8_t n, isr_t handler,char *name);
unsigned long  ar_scanPtes(unsigned long start_addr, unsigned long end_addr,struct addr_list *addr_list);
int ar_pageTableCopy(struct mm_struct *src_mm,struct mm_struct *dest_mm);
int ar_pageTableCleanup(struct mm_struct *mm,unsigned long addr, unsigned long length);
int ar_flushTlbGlobal();
void flush_tlb(unsigned long dir);
int ar_updateCpuState(int cpuid);
unsigned long ar_archSetUserFS(unsigned long addr);
void ar_setupTssStack(unsigned long stack);
int ar_addInputKey(unsigned char c);
#endif
