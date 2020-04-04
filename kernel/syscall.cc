/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/syscall.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "file.hh"
extern "C"{
#include "common.h"
#include "isr.h"

#define EINVAL -22
struct __sysctl_args {
	unsigned long *name; /* integer vector describing variable  TODO: using long instead of integer to store the strings */
	int nlen; /* length of this vector */
	void *oldval; /* 0 or address where to store old value */
	size_t *oldlenp; /* available room for old value,
	 overwritten by actual size of old value */
	void *newval; /* 0 or address of new value */
	size_t newlen; /* size of new value */
};

struct timespec {
    long   tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};
struct timeval {
    long         tv_sec;     /* seconds */
    long    tv_usec;    /* microseconds */
};

unsigned long SYS_sysctl(struct __sysctl_args *args );
unsigned long SYS_getdents(unsigned int fd, uint8_t *user_buf,int size);
unsigned long SYS_fork();
int g_conf_syscall_debug=0;
long SYS_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags,unsigned long fd, unsigned long off);
unsigned long snull(unsigned long *args);
unsigned long SYS_uname(unsigned long *args);

unsigned long SYS_futex(unsigned long *a);
unsigned long SYS_arch_prctl(unsigned long code,unsigned long addr);

typedef long int __time_t;
long int SYS_time(__time_t *time);

unsigned long SYS_fs_fstat(int fd, struct stat *buf);
unsigned long SYS_fs_stat(const char *path, struct stat *buf);

int SYS_fs_dup2(int fd1, int fd2);
int SYS_fs_dup(int fd1);
unsigned long SYS_fs_unlink(uint8_t *path);
unsigned long SYS_fs_readlink(uint8_t *path, char *buf, int bufsiz);
unsigned long SYS_fs_mkdir(unsigned char *name, int mode);
int SYS_getpeername(int sockfd, struct sockaddr *addr, int *addrlen);
int SYS_getsockname(int sockfd, struct sockaddr *addr, int *addrlen);
/*unsigned long SYS_rt_sigaction();
unsigned long SYS_getuid();
unsigned long SYS_getgid();
unsigned long SYS_setsid();
unsigned long SYS_setuid(unsigned long uid) ;
unsigned long SYS_setgid(unsigned long gid) ;
unsigned long SYS_setpgid(unsigned long pid, unsigned long gid);
unsigned long SYS_geteuid() ;
unsigned long SYS_sigaction();
unsigned long SYS_rt_sigprocmask();
unsigned long SYS_ioctl(int d, int request, unsigned long *addr);*/
unsigned long SYS_getpid();
unsigned long SYS_getppid();
//unsigned long SYS_getpgrp();
unsigned long SYS_exit_group();
unsigned long SYS_wait4(int pid, void *status,  unsigned long  option, void *rusage);
unsigned long SYS_set_tid_address(unsigned long tid_address);


struct timezone {
	int tz_minuteswest; /* minutes west of Greenwich */
	int tz_dsttime; /* type of DST correction */
};
/*****************************   POLL related ****************************/
#define POLLIN          0x0001
#define POLLPRI         0x0002
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLHUP         0x0010
#define POLLNVAL        0x0020
struct pollfd {
	int fd; /* file descriptor */
	short events; /* requested events */
	short revents; /* returned events */
};
unsigned long SYS_poll_PART(struct pollfd *fds, int nfds, struct timeval *timeout);
/*************************************************************************/

unsigned long SYS_nanosleep(const struct timespec *req, struct timespec *rem);
unsigned long SYS_getcwd(uint8_t *buf, int len);
unsigned long SYS_chroot_PART(uint8_t *filename);
unsigned long SYS_chdir(uint8_t *filename);
unsigned long SYS_fs_fcntl(int fd, int cmd, int args);
//unsigned long SYS_setsockopt(int sockfd, int level, int optname,
//		const void *optval, int optlen);
unsigned long SYS_gettimeofday(time_t *tv, struct timezone *tz);
unsigned long SYS_pipe(unsigned int *fds);
unsigned long SYS_alarm(int seconds);
int SYS_sync();
int SYS_select(int nfds, int *readfds, int *writefds, int *exceptfds, struct timeval *timeout);

typedef struct {
	void *func;
} syscalltable_t;


void count_sycall_partial(){
	if (g_current_task->curr_syscall_id < MAX_SYSCALL && g_current_task->stats.syscalls!=0){
		g_current_task->stats.syscalls[g_current_task->curr_syscall_id].partial_calls++;
	}
}

#define UTSNAME_LENGTH	65 
/* Structure describing the system and machine.  */
struct utsname
  {
    /* Name of the implementation of the operating system.  */
    uint8_t sysname[UTSNAME_LENGTH];
    /* Name of this node on the network.  */
    uint8_t nodename[UTSNAME_LENGTH];
    /* Current release level of this implementation.  */
    uint8_t release[UTSNAME_LENGTH];
    /* Current version level of this release.  */
    uint8_t version[UTSNAME_LENGTH];
    /* Name of the hardware type the system is running on.  */
    uint8_t machine[UTSNAME_LENGTH];
  };

struct utsname g_utsname;
//uname({sysname="Linux", nodename="njana-desk", release="2.6.35-22-generic", version="#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010", machine="x86_64"}) = 0
static int init_utsname() {
	ut_strcpy(g_utsname.sysname, (uint8_t *) "Linux");
	ut_strcpy(g_utsname.nodename, (uint8_t *) "njana-desk");
	ut_strcpy(g_utsname.release, (uint8_t *) "2.6.35-22-generic");
	ut_strcpy(g_utsname.version,
			(uint8_t *) "#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010");
	ut_strcpy(g_utsname.machine, (uint8_t *) "x86_64");
	return 1;
}
static int init_uts_done = 0;

struct sysinfo {
               long uptime;             /* Seconds since boot */
               unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
               unsigned long totalram;  /* Total usable main memory size */
               unsigned long freeram;   /* Available memory size */
               unsigned long sharedram; /* Amount of shared memory */
               unsigned long bufferram; /* Memory used by buffers */
               unsigned long totalswap; /* Total swap space size */
               unsigned long freeswap;  /* swap space still available */
               unsigned short procs;    /* Number of current processes */
               unsigned long totalhigh; /* Total high memory size */
               unsigned long freehigh;  /* Available high memory size */
               unsigned int mem_unit;   /* Memory unit size in bytes */
               char _f[20-2*sizeof(long)-sizeof(int)]; /* Padding to 64 bytes */
};
extern unsigned long g_phy_mem_size;
int SYS_sysinfo(struct sysinfo *info){
    SYSCALL_DEBUG("sysinfo args:%x \n", info);

    info->loads[0] =0;
    info->loads[1] =0;
    info->loads[2] =0;
    info->freeram = (0.8)*g_phy_mem_size;
    info->totalram = g_phy_mem_size;
    info->mem_unit = 1 ;
    info->sharedram = 1024*1024;
    info->totalswap =0;
    info->freeswap = 0;
    return 0;
}

unsigned long SYS_uname(unsigned long *args) {
	SYSCALL_DEBUG("uname args:%x \n", args);
	if (init_uts_done == 0)
		init_utsname();
//	ut_printf(" Inside uname : %s \n",g_utsname.sysname);
	ut_memcpy((uint8_t *) args, (uint8_t *) &g_utsname,
			sizeof(g_utsname));
	return 0;
}

#define ARCH_SET_FS 0x1002
unsigned long SYS_arch_prctl(unsigned long code, unsigned long addr) {
	SYSCALL_DEBUG("sys arc_prctl : code :%x addr:%x \n", code, addr);
	if (code == ARCH_SET_FS)
		ar_archSetUserFS(addr);
	else
		SYSCALL_DEBUG(" ERROR arc_prctl code is invalid \n");
	return 0;
}
unsigned long SYS_getpid() {
	SYSCALL_DEBUG("getpid :\n");
	return g_current_task->process_id;
}
unsigned long SYS_gettid() {
	SYSCALL_DEBUG("gettid :\n");
	return g_current_task->task_id;
}
unsigned long SYS_nanosleep(const struct timespec *req, struct timespec *rem) {
	long ticks;
	SYSCALL_DEBUG("nanosleep sec:%d nsec:%d:\n", req->tv_sec, req->tv_nsec);
	if (req == 0)
		return 0;
	ticks = req->tv_sec * 100; /* so many 10 ms */
	ticks = ticks + (req->tv_nsec / 10000000);
	sc_sleep(ticks); /* units of 10ms */
	return SYSCALL_SUCCESS;
}
unsigned long SYS_getppid() {
	SYSCALL_DEBUG("getppid :\n");
	return g_current_task->parent_process_pid;
}
unsigned long snull(unsigned long *args) {
	unsigned long syscall_no;

	asm volatile("movq %%rax,%0" : "=r" (syscall_no));
	ut_printf("ERROR: SYSCALL null as hit :%d \n", syscall_no);
	SYS_sc_exit(123);
	return -1;
}

long int SYS_time(__time_t *time) {
	//SYSCALL_DEBUG("time :%x \n", time);
	if (time == 0)
		return 0;
	//*time = g_jiffies;
	ut_get_wallclock(time,0);
	//SYSCALL_DEBUG("Return time :%x seconds \n", *time);
	return *time;
}
void fast_clock(unsigned long *sec, unsigned long *usec);
unsigned long SYS_gettimeofday(time_t *tv, struct timezone *unused_arg_tz) {

	if (tv == 0){
		return SYSCALL_FAIL;
	}
#if 0
	if (ar_check_valid_address((addr_t)tv,sizeof(time_t))==JFAIL){
		BUG();
	}
#endif

#if 1
	fast_clock(&(tv->tv_sec),&(tv->tv_usec));
#else
	ut_get_wallclock(&(tv->tv_sec),&(tv->tv_usec));
#endif
	//SYSCALL_DEBUG("gettimeofday sec:%d(%x) usec:%d(%x)\n", tv->tv_sec,tv->tv_sec, tv->tv_usec,tv->tv_usec);
	return SYSCALL_SUCCESS;
}
#define TEMP_UID 26872
#define TEMP_GID 500
/*****************************************
 TODO : Below are hardcoded system calls , need to make generic *
 ******************************************/
unsigned long SYS_getuid_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("getuid(Hardcoded) :\n");
	return TEMP_UID;
}
unsigned long SYS_getgid_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("getgid(Hardcoded) :\n");
	return TEMP_GID;
}
unsigned long SYS_setuid_PART(unsigned long uid) {
	count_sycall_partial();
	SYSCALL_DEBUG("setuid(Hardcoded) :%x(%d)\n", uid, uid);
	return 0;
}
unsigned long SYS_setgid_PART(unsigned long gid) {
	count_sycall_partial();
	SYSCALL_DEBUG("setgid(Hardcoded) :%x(%d)\n", gid, gid);
	return 0;
}
static unsigned long temp_pgid = 0;
unsigned long SYS_setpgid_PART(unsigned long pid, unsigned long gid) {
	count_sycall_partial();
	SYSCALL_DEBUG("setpgid(Hardcoded) :%x(%d)\n", gid, gid);
	temp_pgid = gid;
	return 0;
}
unsigned long SYS_getpgid_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("SYS_getpgid(Hardcoded) :\n");
	return temp_pgid;
}
unsigned long  SYS_setsid_PART(){
	count_sycall_partial();
	SYSCALL_DEBUG("setsid(Hardcoded) :\n");
	return g_current_task->task_id;
}
unsigned long SYS_geteuid_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("geteuid(Hardcoded) :\n");
	return 500;
}
unsigned long SYS_getpgrp_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("getpgrp(Hardcoded) :\n");
	return 0x123;
}
unsigned long SYS_rt_sigaction_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("sigaction(Dummy) \n");
	return SYSCALL_SUCCESS;
}
int Jcmd_maps(char *arg1, char *arg2);
unsigned long SYS_rt_sigprocmask_PART() {
	count_sycall_partial();
	SYSCALL_DEBUG("sigprocmask(Dummy) \n");
	//Jcmd_maps(0,0);
	SYSCALL_DEBUG("sigprocmask(Dummy) END \n");
	return SYSCALL_SUCCESS;
}

#define TIOCSPGRP 0x5410
extern unsigned long fs_shm_fsync_setup(int fd);
unsigned long SYS_ioctl_PART(int fd, int request, unsigned long *addr) {//TODO
	count_sycall_partial();
	SYSCALL_DEBUG("ioctl(Dummy) d:%x request:%x addr:%x\n", fd, request, addr);
	if (request == TIOCSPGRP && addr != 0) {
		*addr = temp_pgid;
		return 0;
	} else if (request == ASYNC_FSYNC_IOCTL && addr != 0){
		*addr = fs_shm_fsync_setup(fd);
		return SYSCALL_SUCCESS;
	}
	if (addr != 0) {
		*addr = 0x123;
	}
	return SYSCALL_SUCCESS;
}

unsigned long SYS_set_tid_address(unsigned long tid_address){
    SYSCALL_DEBUG("settid address: %x \n",tid_address);
	g_current_task->child_tid_address = tid_address;
	return g_current_task->task_id;
}

extern task_queue_t g_task_queue;
void Jcmd_pt(unsigned char *arg1,unsigned char *arg2){
	unsigned long pt=0x101000;
	struct task_struct *task;
	unsigned char *error=0;
	int ret,pid,i,max_len;
	struct list_head *pos;
	int found=0;
	unsigned long flags;

	spin_lock_irqsave(&g_global_lock, flags);
	if (arg1 == 0) {
		found=1;
		task = g_current_task;
	} else {
		pid=ut_atoi(arg1, FORMAT_DECIMAL);
		list_for_each(pos, &g_task_queue.head) {
			task = list_entry(pos, struct task_struct, task_queue);
			if (task->task_id == pid) {
				found =1;
				pt = task->mm->pgd;
				break;
			}
		}
		if (found == 0){
			error = "ERROR pid  Not found\n";
		}
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
	if (error){
		ut_printf("%s\n",error);
		return;
	}
	pagetable_walk(4,pt,1,0);
	return;
}
extern void symbol_clear_stats();
unsigned long SYS_sysctl(struct __sysctl_args *args) {
	SYSCALL_DEBUG("sysctl  args:%x: \n", args);
	unsigned long *name;
	uint8_t *carg[10];
	int i,k;

	if (args == 0)
		return -1; /* -1 is error */

	//ut_printf(" cmd :%s: ")
	name = args->name[0];
//	ut_printf(" sysctl name.. addr:%x: %x: %x: nlen:%d: \n", name[0], name[1], name[2], args->nlen);

	for (i = 0; i < 10; i++) {
		carg[i] = 0;
	}
	//BRK;
	for (i = 0; i < 3; i++) {
		carg[i] = 0;
		if (name[i] != 0) {
			carg[i] = name[i];  // TODO: currently this is a non-standard name[i] should get integer not strings
		}
	}
	if (carg[0] != 0 && ut_strcmp(carg[0], (uint8_t *)"set") == 0) {
		ut_symbol_execute(SYMBOL_CONF, carg[1], carg[2],0);
	} else if (carg[0] != 0 && ut_strcmp(carg[0], (uint8_t *)"stat") == 0){
		ut_symbol_execute(SYMBOL_STAT, carg[1], carg[2],0);
	}else{
		if (carg[0] == 0) {
			ut_printf("Conf variables:\n");
			ut_symbol_show(SYMBOL_CONF);
			ut_printf("Cmd variables:\n");
			ut_symbol_show(SYMBOL_CMD);
		} else {
			int ret = ut_symbol_execute(SYMBOL_CMD, carg[0],carg[1], carg[2]);
			if (ret ==0){
				ut_printf(" Command Failed\n");
			}
		}
	}
	return SYSCALL_SUCCESS; /* success */
}

unsigned long SYS_wait4(int pid, void *status, unsigned long option, void *rusage) {
	SYSCALL_DEBUG("wait4()  pid:%x status:%x option:%x rusage:%x\n",pid,status,option,rusage);
	unsigned long child_id;
	unsigned long flags;
	struct task_struct *child_task = 0;
#define WNOHANG 1
repeat:
	struct list_head *node;
	node = g_current_task->dead_tasks.head.next;
	if (list_empty((g_current_task->dead_tasks.head.next))) {
		if (!(option & WNOHANG)){
			g_current_task->wait_for_child_exit = 1;
			sc_sleep(50);
			g_current_task->wait_for_child_exit = 0;
			goto repeat;
		}
	} else {
		spin_lock_irqsave(&g_global_lock, flags);
		child_task = list_entry(node,struct task_struct, wait_queue);
		if (child_task == 0)
			BUG();
		list_del(&child_task->wait_queue);
		spin_unlock_irqrestore(&g_global_lock, flags);
	}

	if (child_task != 0) {
		child_id = child_task->task_id;
		atomic_dec(&child_task->count);
	//	ut_log("wait child exited :%s: exitcode:%d \n",child_task->name,child_task->exit_code);
		sc_delete_task(child_task);
		SYSCALL_DEBUG(" wait returning the child id :%d(%x) \n", child_id, child_id);
		DEBUG(" WAIT returning the child id :%d(%x) \n", child_id, child_id);
		return child_id;
	} else {
		return 0;
	}
}
unsigned long SYS_alarm(int seconds){
	SYSCALL_DEBUG("alarm seconds :%d \n", seconds);
	g_current_task->sys_alaram = g_jiffies + (seconds*100);
	return SYSCALL_SUCCESS;
}
unsigned long SYS_chroot_PART(uint8_t *filename) {
	count_sycall_partial();
	SYSCALL_DEBUG("chroot(TODO) :%s \n", filename);
	return SYSCALL_SUCCESS;
}
unsigned long SYS_chdir(uint8_t *filename) {
	int ret;
	struct file *fp;
	SYSCALL_DEBUG("chdir  buf:%s \n", filename);
	if (filename == 0)
		return SYSCALL_FAIL;
	fp = (struct file *) fs_open((uint8_t *) filename, 0, 0);
	if (fp == 0) {
		if (ut_strcmp(filename, (uint8_t *)"/") != 0)
			return SYSCALL_FAIL;
	} else {
		fs_close(fp);
	}
	ut_strncpy(g_current_task->fs->cwd, filename, MAX_FILENAME);
	ret = ut_strlen(g_current_task->fs->cwd);

	return SYSCALL_SUCCESS;
}

unsigned long SYS_getcwd(uint8_t *buf, int len) {
	int ret = SYSCALL_FAIL;

	SYSCALL_DEBUG("getcwd  buf:%x len:%d  \n", buf, len);
	if (buf == 0)
		return ret;
	ut_strncpy(buf, g_current_task->fs->cwd, len);
	ret = ut_strlen(buf);

	return ret;
}

/*************************************
 * TODO : partially implemented calls
 * **********************************/
extern int wait_for_sock_data(void *inode, int timeout);
int SYS_select(int nfds, int *readfds, int *writefds, int *exceptfds, struct timeval *timeout){
	int *p;
	int i;
		int duration =0 ;/* interms of 10 ms */
	if (timeout> 0){
		duration = timeout->tv_sec * 100;
		duration = duration + (timeout->tv_usec/10000);
	}
	SYSCALL_DEBUG("select nfds:%x readfds:%x write:%x execpt:%x timeout:%d duration:%d ms\n", nfds, readfds,writefds,exceptfds,timeout,duration);

	//if (readfds != 0 && timeout == 0) {
	if (readfds != 0 ) {
		p = readfds;
		for (i = 0; i < 32; i++) {
			if (test_bit(i,p)) {
				break;
			}
		} /* TODO :need to extend for multiple fds */
		if (i == 32)
			return 0;
		SYSCALL_DEBUG("select checking read fd :%d \n",i);
		while (1) {
			struct file *file = fd_to_file(i);
			if (file && file->type == NETWORK_FILE){

				if (wait_for_sock_data(file->vinode, duration) >0) {
					return 1;
				}else{
					return 0;
				}
			}else {
				int ret = SYS_fs_read(i,0,0);
				if (ret > 0) return 1;
				sc_sleep(100);
			}
		}
	}else {
		if (duration > 0){
			sc_sleep(duration);
		}
	}
	//SYSCALL_DEBUG("select readval :%x \n",*p);

	return 0;
}
unsigned long SYS_poll_PART(struct pollfd *fds, int nfds, struct timeval *timeout) {
	int i,fd;
	struct file *file;
	unsigned long time_duration;

	count_sycall_partial();
	SYSCALL_DEBUG("poll(partial)  fds:%x nfds:%d timeout:%x  \n", fds, nfds, timeout);
	if (nfds > 1){
		SYSCALL_DEBUG("poll(partial)  fds[0]:%x fds[1]:%x  \n",fds[0].fd,fds[1].fd );
	}
	if (timeout == -1) timeout =0;
	if (timeout == 0){
		time_duration = 0xffffff;
	}else{
		time_duration = timeout->tv_sec*1000000 + (timeout->tv_usec/1000) ;
	}
	if (nfds == 0 || fds == 0 ){
		return -1;
	}
	while (time_duration > 0) {
		for (i = 0; i < nfds; i++) {
			fd = fds[i].fd;
			file = fd_to_file(fd);
			if (file == 0)
				continue;
			vinode *vinode=file->vinode;
			if (vinode->ioctl(GENERIC_IOCTL_PEEK_DATA,0) != 0){
				fds[i].revents = POLLIN;
				return 1;
			}
		}/* end of for */
		sc_sleep(10);
		time_duration = time_duration - 100;
	}
	return SYSCALL_SUCCESS;
}
int SYS_socketpair(int domain,int type, int protocol, int *fd_pair){
	struct file *filep1,*filep2;
	SYSCALL_DEBUG("socketpair call \n");
	if (fd_pair==0) return -1;
	filep1 = fs_create_filep(&fd_pair[0], 0);
	filep2 = fs_create_filep(&fd_pair[1], 0);
	filep1->type = SOCKETPAIR_FILE;
	filep2->type = SOCKETPAIR_FILE;

	return 0;/* on success */
}
unsigned long SYS_setsockopt_PART(int sockfd, int level, int optname,
		const void *optval, int optlen) {
	SYSCALL_DEBUG(
			"Setsocklopt (TODO) fd:%d level:%d optname:%d optval:%x optlen:%d\n", sockfd, level, optname, optval, optlen);
	count_sycall_partial();
	if (optname == 8) /* SO_RECVBUF */
	{

	}
	return SYSCALL_SUCCESS;
}
unsigned long SYS_getsockopt_PART(int sockfd, int level, int optname,
		const void *optval, int optlen) {
	SYSCALL_DEBUG(
			"Getsocklopt (TODO) fd:%d level:%d optname:%d optval:%x optlen:%d\n", sockfd, level, optname, optval, optlen);
	count_sycall_partial();
	if (optname == 8) /* SO_RECVBUF */
	{

	}
	return SYSCALL_SUCCESS;
}
unsigned long SYS_exit_group() {
	SYSCALL_DEBUG("exit_group :\n");
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;

	spin_lock_irqsave(&g_global_lock, flags);
	list_for_each(pos, &g_task_queue.head) {
		task = list_entry(pos, struct task_struct, task_queue);
		if ((task->task_id!=g_current_task->task_id) && (task->process_id == g_current_task->process_id)) {
			task->pending_signals = 1;
			task->killed = 1;
		}
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

	SYS_sc_exit(103);
	return SYSCALL_SUCCESS;
}


#define RLIMIT_CPU      0   /* CPU time in sec */
#define RLIMIT_FSIZE        1   /* Maximum filesize */
#define RLIMIT_DATA     2   /* max data size */
#define RLIMIT_STACK        3   /* max stack size */
#define RLIMIT_CORE     4   /* max core file size */
#define RLIMIT_RSS     5   /* max resident set size */
#define RLIMIT_NPROC       6   /* max number of processes */
#define RLIMIT_NOFILE      7   /* max number of open files */
#define RLIMIT_MEMLOCK     8   /* max locked-in-memory address space */
#define RLIMIT_AS      9   /* address space limit */
#define RLIMIT_LOCKS        10  /* maximum file locks held */
#define RLIMIT_SIGPENDING   11  /* max number of pending signals */
#define RLIMIT_MSGQUEUE     12  /* maximum bytes in POSIX mqueues */
#define RLIMIT_NICE     13  /* max nice prio allowed to raise to
                       0-39 for nice level 19 .. -20 */
#define RLIMIT_RTPRIO       14  /* maximum realtime priority */
#define RLIMIT_RTTIME       15  /* timeout for RT tasks in us */
#define RLIM_NLIMITS        16

#define RLIM_INFINITY (~0UL)

struct rlimit {
    unsigned long   rlim_cur;
    unsigned long   rlim_max;
};
#define _STK_LIM    (8*1024*1024)
#define MLOCK_LIMIT ((PAGE_SIZE > 64*1024) ? PAGE_SIZE : 64*1024)
#define INR_OPEN_CUR 1024
#define INR_OPEN_MAX 4096
#define MQ_BYTES_MAX 819200

struct rlimit rlimits[RLIM_NLIMITS] =                            \
{                                   \
    [RLIMIT_CPU]        = {  RLIM_INFINITY,  RLIM_INFINITY },   \
    [RLIMIT_FSIZE]      = {  RLIM_INFINITY,  RLIM_INFINITY },   \
    [RLIMIT_DATA]       = {  RLIM_INFINITY,  RLIM_INFINITY },   \
    [RLIMIT_STACK]      = {       _STK_LIM,   RLIM_INFINITY },   \
    [RLIMIT_CORE]       = {              0,  RLIM_INFINITY },   \
    [RLIMIT_RSS]        = {  RLIM_INFINITY,  RLIM_INFINITY },   \
    [RLIMIT_NPROC]      = {              0,              0 },   \
    [RLIMIT_NOFILE]     = {   INR_OPEN_CUR,   INR_OPEN_MAX },   \
    [RLIMIT_MEMLOCK]    = {    MLOCK_LIMIT,    MLOCK_LIMIT },   \
    [RLIMIT_AS]     = {  RLIM_INFINITY,  RLIM_INFINITY },   \
    [RLIMIT_LOCKS]      = {  RLIM_INFINITY,  RLIM_INFINITY },   \
    [RLIMIT_SIGPENDING] = {         0,         0 }, \
    [RLIMIT_MSGQUEUE]   = {   MQ_BYTES_MAX,   MQ_BYTES_MAX },   \
    [RLIMIT_NICE]       = { 0, 0 },             \
    [RLIMIT_RTPRIO]     = { 0, 0 },             \
    [RLIMIT_RTTIME]     = {  RLIM_INFINITY,  RLIM_INFINITY },   \
};
int SYS_getrlimit(int resource, struct rlimit *rlim){
	if (resource >= RLIM_NLIMITS){
		return EINVAL;
	}
	rlim->rlim_cur = rlimits[resource].rlim_cur;
	rlim->rlim_max = rlimits[resource].rlim_max;
	return 0;
}
int SYS_tgkill_PART(int tid, int sig){
	count_sycall_partial();
	ut_printf(" TODO: tg kill not yet supported \n");
	return -1;
}
/* TODO */
long SYS_get_robust_list_PART(int pid, struct robust_list_head **head_ptr, size_t *st){
	count_sycall_partial();
	SYSCALL_DEBUG(" get_robust_list : not supported\n");
	return 0;
}
/* TODO */
long SYS_setrlimit_PART(unsigned int resource, void *rlim){
	count_sycall_partial();
	SYSCALL_DEBUG(" set_rlimit : not supported\n");
		return 0;
}
/* TODO */
long SYS_clock_gettime_PART(unsigned int arg1, int arg2){
	count_sycall_partial();
	SYSCALL_DEBUG(" clock_gettime : not supported\n");
	return -1;
}
long SYS_getrandom(void *buf, size_t buflen, unsigned int flags){
	SYSCALL_DEBUG(" SYS_getrandom : partiall supported\n");
	unsigned long  *rand = buf;
	buf=g_jiffies;

	return 5;
}
/* TODO */
long SYS_set_robust_list_PART(struct robust_list_head *head, size_t len){
	count_sycall_partial();
	SYSCALL_DEBUG(" set_robust_list : not supported len:%d\n",len);
	return 0;
}
#define MADV_NORMAL     0               /* no further special treatment */
#define MADV_RANDOM     1               /* expect random page references */
#define MADV_SEQUENTIAL 2               /* expect sequential page references */
#define MADV_WILLNEED   3               /* will need these pages */
#define MADV_DONTNEED   4               /* don't need these pages */
 /* common parameters: try to keep these consistent across architectures */
#define MADV_REMOVE     9               /* remove these pages & resources */
#define MADV_DONTFORK   10              /* don't inherit across fork */
#define MADV_DOFORK     11              /* do inherit across fork */
#define MADV_HWPOISON   100             /* poison a page for testing */
#define MADV_SOFT_OFFLINE 101           /* soft offline page for testing */
#define MADV_MERGEABLE   12             /* KSM may merge identical pages */
#define MADV_UNMERGEABLE 13             /* KSM may not merge identical pages */
#define MADV_HUGEPAGE   14              /* Worth backing with hugepages */
#define MADV_NOHUGEPAGE 15              /* Not worth backing with hugepages */
#define MADV_DONTDUMP   16              /* Explicity exclude from the core dump,
                                            overrides the coredump filter bits */
#define MADV_DODUMP     17              /* Clear the MADV_DONTDUMP flag */


/* TODO */
int SYS_madvise_PART(void *addr, size_t length, int advise){
	if ((advise == MADV_DONTNEED) || (advise == MADV_REMOVE)){
		ut_printf("MADVISE partially implemented: addr:%x length:%x advise:%x\n",addr,length,advise);
		ut_memset(addr, 0 , length);
	}else{
		count_sycall_partial();
		SYSCALL_DEBUG("TODO  madvise : addr:%x lenght: %d  advise:%d \n",addr,length,advise);
	}
	return 0;
}
int SYS_sched_getaffinity_PART(){
	SYSCALL_DEBUG("TODO  SYS_sched_getaffinity\n");
	count_sycall_partial();
	return 0;
}
int SYS_sigalt_stack_PART(){
	SYSCALL_DEBUG("TODO  SYS_sigaltstack\n");
	count_sycall_partial();
	return 0;
}
extern int SYS_fs_openat(int unused,char *filename, int mode, int flags);
extern int SYS_epoll_create(int flags);
extern int SYS_epoll_ctl(uint32_t  efd, uint32_t op, uint32_t fd, struct epoll_event *event);
extern int SYS_epoll_wait(uint32_t efd, struct epoll_event *events, uint32_t maxevents, uint32_t timeout);
extern unsigned long SYS_sched_yield();
syscalltable_t syscalltable[] = {
/* 0 */
{ SYS_fs_read },/* 0 */{ SYS_fs_write }, { SYS_fs_open }, { SYS_fs_close }, { SYS_fs_stat }, { SYS_fs_fstat }, /* 5 */
{ SYS_fs_stat }, { SYS_poll_PART }, { SYS_fs_lseek }, { SYS_vm_mmap }, { SYS_vm_mprotect },/* 10 */
{ SYS_vm_munmap }, { SYS_vm_brk }, { SYS_rt_sigaction_PART }, { SYS_rt_sigprocmask_PART }, { snull }, /* 15 */
{ SYS_ioctl_PART }, { snull }, { snull }, { SYS_fs_readv }, { SYS_fs_writev }, /* 20 */
{ SYS_fs_access }, { SYS_pipe }, { SYS_select }, { SYS_sched_yield }, { snull }, /* 25 */
{ snull }, { snull }, { SYS_madvise_PART }, { snull }, { snull }, /* 30 */
{ snull }, { SYS_fs_dup }, { SYS_fs_dup2 }, { snull }, { SYS_nanosleep }, /* 35 = nanosleep */
{ snull }, { SYS_alarm }, { snull }, { SYS_getpid }, { snull }, /* 40 */
{ SYS_socket }, { SYS_connect }, { SYS_accept }, { SYS_sendto }, { SYS_recvfrom }, /* 45 */
{ SYS_sendmsg }, { SYS_recvmsg }, { snull }, { SYS_bind }, { SYS_listen }, /* 50 */
{ SYS_getsockname }, { SYS_getpeername }, { SYS_socketpair }, { SYS_setsockopt_PART }, { SYS_getsockopt_PART }, /* 55 */
{ SYS_sc_clone }, { SYS_sc_fork }, { SYS_sc_vfork }, { SYS_sc_execve }, { SYS_sc_exit }, /* 60 */
{ SYS_wait4 }, { SYS_sc_kill }, { SYS_uname }, { snull }, { snull }, /* 65 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 70 */
{ snull }, { SYS_fs_fcntl }, { snull }, { snull }, { SYS_fs_fdatasync }, /* 75 */
{ snull }, { snull }, { SYS_getdents }, { SYS_getcwd }, { SYS_chdir }, /* 80 */
{ snull }, { snull }, { SYS_fs_mkdir }, { snull }, { snull }, /* 85 */
{ snull }, { SYS_fs_unlink }, { snull }, { SYS_fs_readlink }, { snull }, /* 90 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 95 */
{ SYS_gettimeofday }, { SYS_getrlimit }, { snull }, { SYS_sysinfo }, { snull }, /* 100 */
{ snull }, { SYS_getuid_PART }, { snull }, { SYS_getgid_PART }, { SYS_setuid_PART }, /* 105 */
{ SYS_setgid_PART }, { SYS_geteuid_PART }, { SYS_getpgid_PART }, { SYS_setpgid_PART }, { SYS_getppid }, /* 110 */
{ SYS_getpgrp_PART }, { SYS_setsid_PART }, { snull }, { snull }, { snull }, /* 115 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 120 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 125 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 130 */
{ SYS_sigalt_stack_PART }, { snull }, { snull }, { snull }, { snull }, /* 135 */
{ snull }, { SYS_fs_stat }, { snull }, { snull }, { snull }, /* 140 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 145 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 150 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 155 */
{ SYS_sysctl }, { snull }, { SYS_arch_prctl }, { snull }, { SYS_setrlimit_PART }, /* 160 */
{ SYS_chroot_PART }, { SYS_sync }, { snull }, { snull }, { snull }, /* 165 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 170 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 175 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 180 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 185 */
{ SYS_gettid }, { snull }, { snull }, { snull }, { snull }, /* 190 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 195 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 200 */
{ SYS_time }, { SYS_futex }, { snull }, { SYS_sched_getaffinity_PART }, { snull }, /* 205 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 210 */
{ snull }, { snull }, { SYS_epoll_create }, { snull }, { snull }, /* 215 */
{ snull }, { snull }, { SYS_set_tid_address }, { snull }, { snull }, /* 220 */
{ SYS_fs_fadvise }, { snull }, { snull }, { snull }, { snull }, /* 225 */
{ snull }, { snull }, { SYS_clock_gettime_PART }, { snull }, { snull }, /* 230  */
{ SYS_exit_group }, { SYS_epoll_wait }, { SYS_epoll_ctl }, { SYS_tgkill_PART }, { snull }, /* 235 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 240 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 245 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 250 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 255 */
{ snull }, { SYS_fs_openat }, { snull }, { snull }, { snull }, /* 260 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 265 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 270 */
{ snull }, { snull }, { SYS_set_robust_list_PART }, { SYS_get_robust_list_PART }, { snull }, /* 275 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 280 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 285 */
{ snull }, { snull }, { SYS_accept4 }, { snull }, { snull }, /* 290 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 295 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 300 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 305 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 310 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 315 */
{ snull }, { snull }, { SYS_getrandom }, { snull }, { snull }, /* 320 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 325 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 330 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 335 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 340 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 345 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 350 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 355 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 360 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 365 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 370 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 375 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 380 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 385 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 390 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 395 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 400 */
{ snull }, { snull }, { snull }, { snull },  { snull }};
/****************************************** syscall debug *********************************************/

void print_syscall_stat(struct task_struct *task, int output){
	int i;
	int count=0;
	if (task->stats.syscalls==0) return;
	for (i=0; i<MAX_SYSCALL; i++){
		if ( task->stats.syscalls[i].call_count > 0){
			if (output ==0 ){
				ut_printf("%d: %s: %s -> %d(%d)\n",count,task->name,ut_get_symbol(syscalltable[i].func),task->stats.syscalls[i].call_count,task->stats.syscalls[i].partial_calls);
			}else{
				ut_log("%d: %s: %s -> %d(%d)\n",count,task->name,ut_get_symbol(syscalltable[i].func),task->stats.syscalls[i].call_count,task->stats.syscalls[i].partial_calls);
			}
			count++;
		}
	}
}
static int strace_progress_id=0;
static uint8_t strace_thread_name[100]="";
static unsigned int strace_syscall_id=0;
int strace_wait() {
	int ret = JFAIL;
	if (ut_strcmp(g_current_task->name, strace_thread_name) == 0) {
		int id = strace_progress_id;
		ret =JSUCCESS;
		ut_strncpy(g_current_task->status_info , "waiting @syscall..",MAX_TASK_STATUS_DATA);
		while ((id == strace_progress_id) && (g_current_task->curr_syscall_id == strace_syscall_id)){
			sc_sleep(20);
		}
	}
	g_current_task->status_info[0] = 0;
	return ret;
}
void Jcmd_strace(uint8_t *arg1,uint8_t *arg2){
	int sid;

	if (arg1==NULL ){
		ut_printf(" USAGE : strace <process-name> <syscallid> \n strace n \n");
		return ;
	}

	if (ut_strcmp(arg1,"n")==0){
		strace_progress_id++;
		ut_printf(" strace made progess \n");
		return;
	}else if (arg2==NULL){
		ut_printf("USAGE: strace <process-name> <syscallid> \n strace n \n");
		return;
	}

	sid =  ut_atoi(arg2, FORMAT_DECIMAL);
	ut_printf("Enabled Strace on  syscall id :%d  process name :%s: \n",sid,arg1);
	ut_strcpy(strace_thread_name,arg1);
	strace_syscall_id = sid;
	return;
}
}
