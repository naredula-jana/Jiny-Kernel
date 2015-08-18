#ifndef __INTERFACE_H__
#define __INTERFACE_H__
#include "common.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"

#include "jiny_api.h"
#define MAX_AUX_VEC_ENTRIES 25 

struct iovec {                    /* Scatter/gather array items */
    void  *iov_base;              /* Starting address */
    size_t iov_len;               /* Number of bytes to transfer */
};
/* Naming : SYS : system call
 *
 */
/* scheduling */
extern task_queue_t g_task_queue;

unsigned long ut_get_percpu_ns() ; /* get time stamp in nano seconds */
unsigned long ut_get_systemtime_ns();
int sc_sleep( long ticks); /* each tick is 100HZ or 10ms */
unsigned long SYS_sc_fork();
unsigned long SYS_sc_vfork();
unsigned long SYS_sc_clone( int clone_flags, void *child_stack, void *pid,  int(*fn)(void *, void *),  void **args) ;
int SYS_sc_exit(int status);
void sc_delete_task(struct task_struct *task);
int sc_task_stick_to_cpu(unsigned long pid, int cpu_id);
int SYS_sc_kill(unsigned long pid,unsigned long signal);
void sc_enable_nonpreemptive();
void sc_disable_nonpreemptive();

int Jcmd_threadlist_stat( uint8_t *arg1,uint8_t *arg2);

void sc_schedule();

/* ipc */
void ipc_check_waitqueues();
void ipc_release_resources(struct task_struct *task);

/* mm */
unsigned long mm_getFreePages(int gfp_mask, unsigned long order);
int mm_putFreePages(unsigned long addr, unsigned long order);
int mm_printFreeAreas(uint8_t *arg1,uint8_t *arg2);
void mm_slab_cache_free (kmem_cache_t *cachep, void *objp);
extern kmem_cache_t *kmem_cache_create(const uint8_t *, size_t,size_t, unsigned long,void (*)(void *, kmem_cache_t *, unsigned long),void (*)(void *, kmem_cache_t *, unsigned long));
void *mm_slab_cache_alloc (kmem_cache_t *cachep, int flags);
void *mm_malloc (size_t size, int flags);
void *ut_calloc(size_t size);
void mm_free (const void *objp);
extern void *vmalloc(int size, int flags);
void jslab_pagefault(unsigned long addr, unsigned long faulting_ip, struct fault_ctx *ctx);
void memleakHook_alloc(unsigned char *ptr, int size, int type, void *cachep);
int mm_set_debug_data(unsigned long addr, unsigned long data);
int mm_check_debug_data(unsigned long addr, unsigned long data);

unsigned long jalloc_page(int flags);
int jfree_page(unsigned long p);
#define alloc_page(flags) jalloc_page(flags)
#define free_page(p) jfree_page(p)
#define memset ut_memset
#define ut_free mm_free
#define ut_malloc(x) mm_malloc(x,0)
int vfree(addr_t addr);
unsigned long __va(unsigned long addr);
unsigned long __pa(unsigned long addr);

/* vm */
int Jcmd_vmaps_stat(char *arg1,char *arg2);
struct vm_area_struct *vm_findVma(struct mm_struct *mm,unsigned long addr, unsigned long len);
void * SYS_vm_mmap(unsigned long fd, unsigned long addr, unsigned long len,unsigned long prot, unsigned long flags, unsigned long pgoff);
unsigned long SYS_vm_brk(unsigned long addr);
int SYS_vm_munmap(unsigned long addr, unsigned long len);
int SYS_vm_mprotect(const void *addr, int len, int prot);
unsigned long vm_brk(unsigned long addr, unsigned long len);
unsigned long vm_mmap(struct file *fp, unsigned long addr, unsigned long len,unsigned long prot, unsigned long flags, unsigned long pgoff, const char *name);
int vm_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len);
unsigned long vm_setupBrk(unsigned long addr, unsigned long len);
unsigned long vm_dup_vmaps(struct mm_struct *src_mm,struct mm_struct *dest_mm);
unsigned long vm_create_kmap(unsigned char *name, unsigned long map_size, unsigned long prot, unsigned long flags, unsigned long pgoff);
void vm_vma_stat(struct vm_area_struct *vma, unsigned long vaddr,unsigned long faulting_ip, int write_fault,unsigned long optional);

/* page cache */
int pc_init(uint8_t *start_addr,unsigned long len);
int Jcmd_pagecache_stat(char *arg1,char *arg2);
int pc_pageDirted(struct page *p);
int pc_pagecleaned(struct page *page);
int pc_check_valid_addr(uint8_t *addr, int len);

unsigned long fs_getVmaPage(struct vm_area_struct *vma,unsigned long offset);
int pc_deletePage(struct page *page);
int pc_putFreePage(struct page *page);
page_struct_t *pc_get_dirty_page();
int pc_put_page(struct page *page);
int pc_get_page(struct page *page);
int pc_is_freepages_available(void);
page_struct_t *pc_getFreePage(int clear_flag);
uint8_t *pc_page_to_ptr(struct page *p);
int pc_housekeep(void);

/*vfs */
//unsigned long fs_registerFileSystem(filesystem *fs, unsigned char *mnt_pnt);
unsigned long fs_putInode(void *fs_inode);
int Jcmd_ls(uint8_t *arg1,uint8_t *arg2);
struct file *fs_open(uint8_t *filename,int mode,int flags);
int fs_close(struct file *file);
long fs_read(struct file *fp ,uint8_t *buff ,unsigned long len);
unsigned long fs_fadvise(void *inode,unsigned long offset, unsigned long len,int advise);
unsigned long fs_lseek(struct file *fp ,unsigned long offset, int whence);
//unsigned long fs_loadElfLibrary(struct file  *file,unsigned long tmp_stack, unsigned long stack_len,unsigned long aux_addr);
unsigned long fs_elf_load(struct file *file,unsigned long tmp_stack, unsigned long stack_len, unsigned long aux_addr);
unsigned long fs_elf_check_prepare(struct file *file,unsigned char **argv, unsigned char **env,unsigned long *t_argc, unsigned long *t_argv,unsigned long  *stack_len, unsigned long *aux_addr,unsigned char **elf_interpreter, unsigned long *tmp_stackp);
int fs_write(struct file *file,uint8_t *buff ,unsigned long len);
unsigned long fs_fdatasync(struct file *file);
int fs_stat(struct file *file, struct fileStat *stat);
unsigned long fs_readdir(struct file *file, struct dirEntry *dir_ent, int len, int *offset);
struct file *fs_dup(struct file *old_filep, struct file *new_filep);
int fs_set_flags(struct file *fp, int flags);
int fs_get_type(struct file *fp);
void *fs_get_stat(void *inodep, int type);
unsigned char *fs_get_filename(void *inodep);
void *fs_get_inode_vma_list(void *inodep);
uint64_t fs_get_size(void *inodep);
int fs_set_size(void *inodep, unsigned long size);
void fs_set_private(void *inodep, int fid);
int fs_get_private(void *inodep);
int fs_get_inode_type(void *inodep);
int fs_set_offset(void *inodep, unsigned long offset);
void fs_set_inode_used(void *inodep);
int fs_get_inode_flags(void *inodep);

long SYS_fs_writev(int fd, const struct iovec *iov, int iovcnt);
long SYS_fs_readv(int fd, const struct iovec *iov, int iovcnt);
unsigned long SYS_fs_access(char *filename, int mode);
unsigned long SYS_fs_open(char *filename,int mode,int flags);
unsigned long SYS_fs_lseek(unsigned long fd ,unsigned long offset, int whence);
int SYS_fs_write(unsigned long fd ,uint8_t *buff ,unsigned long len);
int SYS_fs_read(unsigned long fd ,uint8_t *buff ,unsigned long len);
unsigned long SYS_fs_close(unsigned long fd);
unsigned long SYS_fs_fdatasync(unsigned long fd );
unsigned long SYS_fs_fadvise(unsigned long fd,unsigned long offset, unsigned long len,int advise);

/* Utilities */
void ut_getBackTrace(unsigned long *rbp, unsigned long task_addr, backtrace_t *bt);
int perf_stat_rip_hit(unsigned long rip);
unsigned long get_kvm_time_fromboot();
void apic_set_task_priority(uint8_t prio);
void apic_reenable();
void *get_keyboard_device(int device_type,int file_type);
void _ipc_delete_from_waitqueues(struct task_struct *task);

/* architecture depended */
void ar_registerInterrupt(uint8_t n, isr_t handler,char *name, void *private_data);
unsigned long  ar_scanPtes(unsigned long start_addr, unsigned long end_addr,struct addr_list *addr_list, struct addr_list *page_dirty_list);
int ar_dup_pageTable(struct mm_struct *src_mm,struct mm_struct *dest_mm);
int ar_pageTableCleanup(struct mm_struct *mm,unsigned long addr, unsigned long length);
int ar_flushTlbGlobal();
void flush_tlb(unsigned long dir);
void flush_tlb_entry(unsigned long  vaddr);
unsigned long  ar_modifypte(unsigned long addr, struct mm_struct *mm, uint8_t rw);
int ar_updateCpuState(struct task_struct *next, struct task_struct *prev);
int ar_archSetUserFS(unsigned long addr);
void ar_setupTssStack(unsigned long stack);
int ar_addInputKey(int device_id,uint8_t c);
int ar_check_valid_address(unsigned long addr, int len);

/**************** misc functions ********/
int getmaxcpus();
int apic_send_ipi_vector(int cpu, uint8_t vector);
void apic_disable_partially();

int read_apic_isr(int isr);
void local_apic_send_eoi(void);

/**************  init functions *********/
int init_kernel();
int init_memory(unsigned long unused);
int init_descriptor_tables();
int init_driver_keyboard();

/**************************  Networking ***/
#define NETWORK_PROTOCOLSTACK 1
#define NETWORK_DRIVER 2
int registerNetworkHandler(int type, int (*callback)(uint8_t *buf, unsigned int len, void *private_data), void *private_data, unsigned char *mac);
int netif_rx(uint8_t *data, unsigned int len);
int netif_tx(uint8_t *data, unsigned int len);
int netif_rx_enable_polling(void *private_data, int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts));
/************************ socket layer interface ********/
struct sockaddr {
	uint16_t family;
	uint16_t sin_port;
	uint32_t addr;
	char     sin_zero[8];  // zero this if you want to
};
#define socklen_t int
struct msghdr {
    void         *msg_name;       /* optional address */
    socklen_t     msg_namelen;    /* size of address */
    struct iovec *msg_iov;        /* scatter/gather array */
    size_t        msg_iovlen;     /* # elements in msg_iov */
    void         *msg_control;    /* ancillary data, see below */
    socklen_t     msg_controllen; /* ancillary data buffer len */
    int           msg_flags;      /* flags on received message */
};
struct Socket_API{
	void* (*open)(int type);
	int (*bind)(void *conn,struct sockaddr *s,int sock_type);
	int (*get_addr)(void *conn,struct sockaddr *s);
	void* (*accept)(void *conn);
	void* (*check_data)(void *conn);
	int (*listen)(void *conn,int len);
	int (*connect)(void *conn, uint32_t *addr, uint16_t port);
    int (*write)(void *conn, uint8_t *buff, unsigned long len, int sock_type,uint32_t daddr, uint16_t dport);
    int (*read)(void *conn, uint8_t *buff, unsigned long len);
    int (*read_from)(void *conn, uint8_t *buff, unsigned long len,uint32_t *addr, uint16_t port);
	int (*close)(void *conn,int sock_type);
	int (*network_status)(void *arg1,void *arg2);
};

int register_to_socketLayer(struct Socket_API *api);
int socket_close(struct file *file);
int socket_read(struct file *file, uint8_t *buff, unsigned long len);
int socket_write(struct file *file, uint8_t *buff, unsigned long len);
int SYS_socket(int family,int type, int z);
int SYS_listen(int fd,int length);
int SYS_accept(int fd);
int SYS_bind(int fd, struct sockaddr  *addr, int len);
int SYS_connect(int fd, struct sockaddr  *addr, int len);
unsigned long SYS_sendto(int sockfd,  void *buf, size_t len, int flags,
                struct sockaddr *dest_addr, int addrlen);
int SYS_recvfrom(int sockfd,  void *buf, size_t len, int flags,  struct sockaddr *dest_addr, int addrlen);
int SYS_sendmsg();
int SYS_recvmsg(int sockfd, struct msghdr *msg, int flags);
/* Utilities */
void ut_showTrace(unsigned long *stack_top);
int ut_strcmp(uint8_t *str1, uint8_t *str2);
void ut_printf (const char *format, ...);
void ut_memcpy(uint8_t *dest, uint8_t *src, long len);
void ut_memset(uint8_t *dest, uint8_t val, long len);
int ut_memcmp(uint8_t *m1, uint8_t *m2,int len);
uint8_t *ut_strcpy(uint8_t *dest, const uint8_t *src);
uint8_t *ut_strncpy(uint8_t *dest, const uint8_t *src,int n);
uint8_t *ut_strcat(uint8_t *dest, const uint8_t *src);
uint8_t *ut_strstr(uint8_t *s1,uint8_t *s2);
int ut_count_obj_add(unsigned char *name, int sz);
int ut_count_obj_free(int id);
int ut_strlen(const uint8_t * s);
int ut_mod_symbol_execute(int type, char *name, char *argv1, char *argv2);
unsigned char *ut_get_symbol(addr_t addr);
unsigned long ut_mod_get_symbol_addr(unsigned char *name);
enum{
	FORMAT_HEX=1,
	FORMAT_DECIMAL=2
};
unsigned long ut_atol(uint8_t *p, int format);
unsigned int ut_atoi(uint8_t *p, int format);
int ut_sprintf(uint8_t * buf, const uint8_t *fmt, ...);
int ut_snprintf(uint8_t * buf, size_t size, const char *fmt, ...);
void ut_log(const char *format, ...);

unsigned long ut_get_symbol_addr(unsigned char *name);
int ut_symbol_execute(int type, unsigned char *name, uint8_t *argv1,uint8_t *argv2);
int ut_symbol_show(int type);
int strace_wait(void);
int ut_get_wallclock(unsigned long *sec, unsigned long *usec);
void ut_putchar(uint8_t c);
void udelay(int loops);
int Jcmd_mem(char *arg1, char *arg2);

/* scheduling */
unsigned long sc_createKernelThread(int (*fn)(void *, void *),void **argv,uint8_t *name, unsigned long flags);
void SYS_sc_execve(uint8_t *file,uint8_t **argv,uint8_t **env);
void **sc_get_thread_argv();
void sc_set_fsdevice(unsigned int in, unsigned int out);

uint8_t dr_kbGetchar(int device_id, int peek_data);
int pagetable_walk(int level,unsigned long ptable_addr, int print);

void *ipc_mutex_create(char *name);
int ipc_mutex_lock(void *p, int line);
int ipc_mutex_unlock(void *p, int line);
int ipc_mutex_destroy(void *p);
#define mutexCreate ipc_mutex_create
#define mutexLock(p)      do { ipc_mutex_lock((void *)p,__LINE__); } while (0)
#define mutexUnLock(p)      do { ipc_mutex_unlock((void *)p,__LINE__); } while (0)
#define mutexDestroy ipc_mutex_destroy

/*************************************** virtio interfaces ******************************/
static inline void ar_prefetch0(volatile void *p)
{
	asm volatile ("prefetcht0 %[p]" : [p] "+m" (*(volatile char *)p));
}
struct struct_mbuf{
		unsigned char *buf;
		int len;
};


#endif

