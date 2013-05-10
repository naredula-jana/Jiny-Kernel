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
extern int ipc_register_waitqueue(wait_queue_t *waitqueue, char *name);
extern int ipc_unregister_waitqueue(wait_queue_t *waitqueue);
int ipc_wakeup_waitqueue(wait_queue_t *waitqueue);
int ipc_waiton_waitqueue(wait_queue_t *waitqueue, unsigned long ticks);
int sc_sleep( long ticks); /* each tick is 100HZ or 10ms */
unsigned long SYS_sc_fork();
unsigned long SYS_sc_clone( int clone_flags, void *child_stack, void *pid, void *ctid,  void *args) ;
int SYS_sc_exit(int status);
void sc_delete_task(struct task_struct *task);
int SYS_sc_kill(unsigned long pid,unsigned long signal);
void SYS_sc_execve(unsigned char *file,unsigned char **argv,unsigned char **env);
int Jcmd_threadlist_stat( char *arg1,char *arg2);
unsigned long sc_createKernelThread(int (*fn)(void *),unsigned char *argv,unsigned char *name);
void sc_schedule();

/* ipc */
void ipc_check_waitqueues();

/* mm */
unsigned long mm_getFreePages(int gfp_mask, unsigned long order);
int mm_putFreePages(unsigned long addr, unsigned long order);
int mm_printFreeAreas(char *arg1,char *arg2);
void mm_slab_cache_free (kmem_cache_t *cachep, void *objp);
extern kmem_cache_t *kmem_cache_create(const char *, size_t,size_t, unsigned long,void (*)(void *, kmem_cache_t *, unsigned long),void (*)(void *, kmem_cache_t *, unsigned long));
void *mm_slab_cache_alloc (kmem_cache_t *cachep, int flags);
void *mm_malloc (size_t size, int flags);
void mm_free (const void *objp);
#define alloc_page() mm_getFreePages(0, 0)
#define memset ut_memset
#define ut_free mm_free
#define ut_malloc(x) mm_malloc(x,0)

/* vm */
int Jcmd_vmaps_stat(char *arg1,char *arg2);
struct vm_area_struct *vm_findVma(struct mm_struct *mm,unsigned long addr, unsigned long len);
long SYS_vm_mmap(unsigned long fd, unsigned long addr, unsigned long len,unsigned long prot, unsigned long flags, unsigned long pgoff);
unsigned long SYS_vm_brk(unsigned long addr);
int SYS_vm_munmap(unsigned long addr, unsigned long len);
int SYS_vm_mprotect(const void *addr, int len, int prot);
unsigned long vm_brk(unsigned long addr, unsigned long len);
unsigned long vm_mmap(struct file *fp, unsigned long addr, unsigned long len,unsigned long prot, unsigned long flags, unsigned long pgoff);
int vm_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len);
unsigned long vm_setupBrk(unsigned long addr, unsigned long len);
unsigned long vm_dup_vmaps(struct mm_struct *src_mm,struct mm_struct *dest_mm);

/* page cache */
int pc_init(unsigned char *start_addr,unsigned long len);
int Jcmd_pagecache_stat(char *arg1,char *arg2);
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
//struct inode *fs_getInode(char *filename);
unsigned long fs_putInode(struct inode *inode);
int Jcmd_ls(char *arg1,char *arg2);
struct file *fs_open(unsigned char *filename,int mode,int flags);
int fs_close(struct file *file);
struct page *fs_genericRead(struct inode *inode,unsigned long offset);
long fs_read(struct file *fp ,unsigned char *buff ,unsigned long len);
unsigned long fs_fadvise(struct inode *inode,unsigned long offset, unsigned long len,int advise);
unsigned long fs_lseek(struct file *fp ,unsigned long offset, int whence);
unsigned long fs_loadElfLibrary(struct file  *file,unsigned long tmp_stack, unsigned long stack_len,unsigned long aux_addr);
long fs_write(struct file *file,unsigned char *buff ,unsigned long len);
unsigned long fs_fdatasync(struct file *file);
int fs_stat(struct file *file, struct fileStat *stat);

long SYS_fs_writev(int fd, const struct iovec *iov, int iovcnt);
long SYS_fs_readv(int fd, const struct iovec *iov, int iovcnt);
unsigned long SYS_fs_open(char *filename,int mode,int flags);
unsigned long SYS_fs_lseek(unsigned long fd ,unsigned long offset, int whence);
long SYS_fs_write(unsigned long fd ,unsigned char *buff ,unsigned long len);
long SYS_fs_read(unsigned long fd ,unsigned char *buff ,unsigned long len);
unsigned long SYS_fs_close(unsigned long fd);
unsigned long SYS_fs_fdatasync(unsigned long fd );
unsigned long SYS_fs_fadvise(unsigned long fd,unsigned long offset, unsigned long len,int advise);

/* Utilities */
void ut_showTrace(unsigned long *stack_top);
int ut_strcmp(unsigned char *str1, unsigned char *str2);
void ut_printf (const char *format, ...);
void ut_memcpy(unsigned char *dest, unsigned char *src, long len);
void ut_memset(unsigned char *dest, unsigned char val, long len);
int ut_memcmp(unsigned char *m1, unsigned char *m2,int len);
unsigned char *ut_strcpy(unsigned char *dest, const unsigned char *src);
unsigned char *ut_strncpy(unsigned char *dest, const unsigned char *src,int n);
unsigned char *ut_strcat(unsigned char *dest, const unsigned char *src);
unsigned char *ut_strstr(unsigned char *s1,unsigned char *s2);
int ut_strlen(const unsigned char * s);
unsigned long ut_atol(unsigned char *p);
unsigned int ut_atoi(unsigned char *p);
int ut_sprintf(char * buf, const char *fmt, ...);
int ut_snprintf(char * buf, size_t size, const char *fmt, ...);

/* architecture depended */
void ar_registerInterrupt(uint8_t n, isr_t handler,char *name, void *private_data);
unsigned long  ar_scanPtes(unsigned long start_addr, unsigned long end_addr,struct addr_list *addr_list, struct addr_list *page_dirty_list);
int ar_dup_pageTable(struct mm_struct *src_mm,struct mm_struct *dest_mm);
int ar_pageTableCleanup(struct mm_struct *mm,unsigned long addr, unsigned long length);
int ar_flushTlbGlobal();
void flush_tlb(unsigned long dir);
int ar_updateCpuState(struct task_struct *p);
int ar_archSetUserFS(unsigned long addr);
void ar_setupTssStack(unsigned long stack);
int ar_addInputKey(int device_id,unsigned char c);

/**************** misc functions ********/
int getmaxcpus();
int apic_send_ipi_vector(int cpu, uint8_t vector);

unsigned char dr_kbGetchar();
void ut_putchar(int c);
int read_apic_isr(int isr);
void local_apic_send_eoi(void);

/**************  init functions *********/
int init_kernel(unsigned long end_addr);
void init_memory(unsigned long phy_end_addr);
void kmem_cache_init(void);
void kmem_cache_sizes_init(void);
void init_descriptor_tables();
int init_driver_keyboard();
void init_tasking();
void init_serial();
int init_symbol_table();
void init_vfs();
int init_smp_force(int ncpus);
void init_syscall(int cpuid);
int init_networking();

/**************************  Networking ***/
#define NETWORK_PROTOCOLSTACK 1
#define NETWORK_DRIVER 2
int registerNetworkHandler(int type, int (*callback)(unsigned char *buf, unsigned int len, void *private_data), void *private_data);
int netif_rx(unsigned char *data, unsigned int len);
int netif_tx(unsigned char *data, unsigned int len);
/************************ socket layer interface ********/
struct sockaddr {
	uint16_t family;
	uint16_t sin_port;
	uint32_t addr;
	char     sin_zero[8];  // zero this if you want to

};
struct Socket_API{
	void* (*open)(int type);
	int (*bind)(void *conn,struct sockaddr *s);
	void* (*accept)(void *conn);
	int (*listen)(void *conn,int len);
	int (*connect)(void *conn, uint32_t *addr, uint16_t port);
    int (*write)(void *conn, unsigned char *buff, unsigned long len);
    int (*read)(void *conn, unsigned char *buff, unsigned long len);
	int (*close)(void *conn);
};
int register_to_socketLayer(struct Socket_API *api);
int socket_close(struct file *file);
int socket_read(struct file *file, unsigned char *buff, unsigned long len);
int socket_write(struct file *file, unsigned char *buff, unsigned long len);
int SYS_socket(int family,int type, int z);
int SYS_listen(int fd,int length);
int SYS_accept(int fd);
int SYS_bind(int fd, struct sockaddr  *addr, int len);
int SYS_connect(int fd, struct sockaddr  *addr, int len);
unsigned long SYS_sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, int addrlen);
int SYS_recvfrom(int fd);
#endif
