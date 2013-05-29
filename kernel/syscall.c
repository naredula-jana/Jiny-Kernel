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
#include "common.h"
#include "isr.h"

struct __sysctl_args {
	int *name; /* integer vector describing variable */
	int nlen; /* length of this vector */
	void *oldval; /* 0 or address where to store old value */
	size_t *oldlenp; /* available room for old value,
	 overwritten by actual size of old value */
	void *newval; /* 0 or address of new value */
	size_t newlen; /* size of new value */
};

unsigned long SYS_sysctl(struct __sysctl_args *args );
unsigned long SYS_getdents(unsigned int fd, unsigned char *user_buf,int size);
unsigned long SYS_fork();
int g_conf_syscall_debug=0;
long SYS_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags,unsigned long fd, unsigned long off);
unsigned long snull(unsigned long *args);
unsigned long SYS_uname(unsigned long *args);

unsigned long SYS_futex(unsigned long *a);
unsigned long SYS_arch_prctl(unsigned long code,unsigned long addr);
struct timespec {
    long   tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};


struct stat {
	unsigned long int st_dev; /* ID of device containing file */
	unsigned long int st_ino; /* inode number */
	unsigned long int st_nlink; /* number of hard links */
	unsigned int st_mode; /* protection */
	unsigned int st_uid; /* user ID of owner */
	unsigned int st_gid; /* group ID of owner */
    int __pad0;
	unsigned long int st_rdev; /* device ID (if special file) */
	long int st_size; /* total size, in bytes */
	long int st_blksize; /* blocksize for file system I/O */
	long int st_blocks; /* number of 512B blocks allocated */
	struct timespec st_atime; /* time of last access */
	struct timespec st_mtime; /* time of last modification */
	struct timespec st_ctime; /* time of last status change */
	long int __unused[3];
};
typedef long int __time_t;
long int SYS_time(__time_t *time);

unsigned long SYS_fs_fstat(int fd, void *buf);
unsigned long SYS_fs_stat(const char *path, struct stat *buf);
unsigned long SYS_fs_fstat(int fd, void *buf);
unsigned long SYS_fs_dup2(int fd1, int fd2);
unsigned long SYS_fs_readlink(unsigned char *path, char *buf, int bufsiz);

unsigned long SYS_rt_sigaction();
unsigned long SYS_getuid();
unsigned long SYS_getgid();
unsigned long SYS_setuid(unsigned long uid) ;
unsigned long SYS_setgid(unsigned long gid) ;
unsigned long SYS_setpgid(unsigned long pid, unsigned long gid);
unsigned long SYS_geteuid() ;
unsigned long SYS_sigaction();
unsigned long SYS_ioctl();
unsigned long SYS_getpid();
unsigned long SYS_getppid();
unsigned long SYS_getpgrp();
unsigned long SYS_exit_group();
unsigned long SYS_wait4(int pid, void *status,  unsigned long  option, void *rusage);

struct pollfd {
	int fd; /* file descriptor */
	short events; /* requested events */
	short revents; /* returned events */
};
struct timezone {
	int tz_minuteswest; /* minutes west of Greenwich */
	int tz_dsttime; /* type of DST correction */
};

unsigned long SYS_poll(struct pollfd *fds, int nfds, int timeout);

unsigned long SYS_nanosleep(const struct timespec *req, struct timespec *rem);
unsigned long SYS_getcwd(unsigned char *buf, int len);
unsigned long SYS_chdir(unsigned char *filename);
unsigned long SYS_fs_fcntl(int fd, int cmd, int args);
unsigned long SYS_setsockopt(int sockfd, int level, int optname,
		const void *optval, int optlen);
unsigned long SYS_gettimeofday(time_t *tv, struct timezone *tz);
unsigned long SYS_pipe(unsigned int *fds);
int SYS_sync();

typedef struct {
	void *func;
} syscalltable_t;

syscalltable_t syscalltable[] = {
/* 0 */
{ SYS_fs_read },/* 0 */{ SYS_fs_write }, { SYS_fs_open }, { SYS_fs_close }, { SYS_fs_stat }, { SYS_fs_fstat }, /* 5 */
{ SYS_fs_stat }, { SYS_poll }, { SYS_fs_lseek }, { SYS_vm_mmap }, { SYS_vm_mprotect },/* 10 */
{ SYS_vm_munmap }, { SYS_vm_brk }, { SYS_rt_sigaction }, { snull }, { snull }, /* 15 */
{ SYS_ioctl }, { snull }, { snull }, { SYS_fs_readv }, { SYS_fs_writev }, /* 20 */
{ snull }, { SYS_pipe }, { snull }, { snull }, { snull }, /* 25 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 30 */
{ snull }, { snull }, { SYS_fs_dup2 }, { snull }, { SYS_nanosleep }, /* 35 = nanosleep */
{ snull }, { snull }, { snull }, { SYS_getpid }, { snull }, /* 40 */
{ SYS_socket }, { SYS_connect }, { SYS_accept }, { SYS_sendto }, { SYS_recvfrom }, /* 45 */
{ snull }, { snull }, { snull }, { SYS_bind }, { SYS_listen }, /* 50 */
{ snull }, { snull }, { snull }, { SYS_setsockopt }, { snull }, /* 55 */
{ SYS_sc_clone }, { SYS_sc_fork }, { snull }, { SYS_sc_execve }, { SYS_sc_exit }, /* 60 */
{ SYS_wait4 }, { SYS_sc_kill }, { SYS_uname }, { snull }, { snull }, /* 65 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 70 */
{ snull }, { SYS_fs_fcntl }, { snull }, { snull }, { SYS_fs_fdatasync }, /* 75 */
{ snull }, { snull }, { SYS_getdents }, { SYS_getcwd }, { SYS_chdir }, /* 80 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 85 */
{ snull }, { snull }, { snull }, { SYS_fs_readlink }, { snull }, /* 90 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 95 */
{ SYS_gettimeofday }, { snull }, { snull }, { snull }, { snull }, /* 100 */
{ snull }, { SYS_getuid }, { snull }, { SYS_getgid }, { SYS_setuid }, /* 105 */
{ SYS_setgid }, { SYS_geteuid }, { snull }, { SYS_setpgid }, { SYS_getppid }, /* 110 */
{ SYS_getpgrp }, { snull }, { snull }, { snull }, { snull }, /* 115 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 120 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 125 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 130 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 135 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 140 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 145 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 150 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 155 */
{ SYS_sysctl }, { snull }, { SYS_arch_prctl }, { snull }, { snull }, /* 160 */
{ snull }, { SYS_sync }, { snull }, { snull }, { snull }, /* 165 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 170 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 175 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 180 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 185 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 190 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 195 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 200 */
{ SYS_time }, { SYS_futex }, { snull }, { snull }, { snull }, /* 205 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 210 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 215 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 220 */
{ SYS_fs_fadvise }, { snull }, { snull }, { snull }, { snull }, /* 225 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 230  */
{ SYS_exit_group }, { snull }, { snull }, { snull }, { snull }, /* 235 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 240 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 245 */
{ snull }, { snull }, { snull }, { snull }, };


#define UTSNAME_LENGTH	65 
/* Structure describing the system and machine.  */
struct utsname
  {
    /* Name of the implementation of the operating system.  */
    unsigned char sysname[UTSNAME_LENGTH];
    /* Name of this node on the network.  */
    unsigned char nodename[UTSNAME_LENGTH];
    /* Current release level of this implementation.  */
    unsigned char release[UTSNAME_LENGTH];
    /* Current version level of this release.  */
    unsigned char version[UTSNAME_LENGTH];
    /* Name of the hardware type the system is running on.  */
    unsigned char machine[UTSNAME_LENGTH];
  };

struct utsname g_utsname;
//uname({sysname="Linux", nodename="njana-desk", release="2.6.35-22-generic", version="#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010", machine="x86_64"}) = 0
static int init_utsname() {
	ut_strcpy(g_utsname.sysname, (unsigned char *) "Linux");
	ut_strcpy(g_utsname.nodename, (unsigned char *) "njana-desk");
	ut_strcpy(g_utsname.release, (unsigned char *) "2.6.35-22-generic");
	ut_strcpy(g_utsname.version,
			(unsigned char *) "#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010");
	ut_strcpy(g_utsname.machine, (unsigned char *) "x86_64");
	return 1;
}
static int init_uts_done = 0;

unsigned long SYS_uname(unsigned long *args) {
	SYSCALL_DEBUG("uname args:%x \n", args);
	if (init_uts_done == 0)
		init_utsname();
//	ut_printf(" Inside uname : %s \n",g_utsname.sysname);
	ut_memcpy((unsigned char *) args, (unsigned char *) &g_utsname,
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
	return g_current_task->pid;
}

unsigned long SYS_nanosleep(const struct timespec *req, struct timespec *rem) {
	long ticks;
	SYSCALL_DEBUG("nanosleep sec:%d nsec:%d:\n", req->tv_sec, req->tv_nsec);
	if (req == 0)
		return 0;
	ticks = req->tv_sec * 100;
	ticks = ticks + (req->tv_nsec / 100000);
	sc_sleep(ticks); /* units of 10ms */
	return SYSCALL_SUCCESS;
}
unsigned long SYS_getppid() {
	SYSCALL_DEBUG("getppid :\n");
	return g_current_task->ppid;
}
unsigned long snull(unsigned long *args) {
	unsigned long syscall_no;

	asm volatile("movq %%rax,%0" : "=r" (syscall_no));
	ut_printf("ERROR: SYSCALL null as hit :%d \n", syscall_no);
	SYS_sc_exit(123);
	return -1;
}

long int SYS_time(__time_t *time) {
	SYSCALL_DEBUG("time :%x \n", time);
	if (time == 0)
		return 0;
	//*time = g_jiffies;
	get_wallclock(time);
	SYSCALL_DEBUG("Return time :%x seconds \n", *time);
	return *time;
}
unsigned long SYS_gettimeofday(time_t *tv, struct timezone *tz) {
	SYSCALL_DEBUG("gettimeofday tv:%x tz:%x\n", tv, tz);
	if (tv == 0)
		return SYSCALL_FAIL;
	get_wallclock(&(tv->tv_nsec));
	return SYSCALL_SUCCESS;
}
#define TEMP_UID 500
/*****************************************
 TODO : Below are hardcoded system calls , need to make generic *
 ******************************************/
unsigned long SYS_getuid() {
	SYSCALL_DEBUG("getuid(Hardcoded) :\n");
	return TEMP_UID;
}
unsigned long SYS_getgid() {
	SYSCALL_DEBUG("getgid(Hardcoded) :\n");
	return TEMP_UID;
}
unsigned long SYS_setuid(unsigned long uid) {
	SYSCALL_DEBUG("setuid(Hardcoded) :%x(%d)\n", uid, uid);
	return 0;
}
unsigned long SYS_setgid(unsigned long gid) {
	SYSCALL_DEBUG("setgid(Hardcoded) :%x(%d)\n", gid, gid);
	return 0;
}
static unsigned long temp_pgid = 0;
unsigned long SYS_setpgid(unsigned long pid, unsigned long gid) {
	SYSCALL_DEBUG("setpgid(Hardcoded) :%x(%d)\n", gid, gid);
	temp_pgid = gid;
	return 0;
}
unsigned long SYS_geteuid() {
	SYSCALL_DEBUG("geteuid(Hardcoded) :\n");
	return 500;
}
unsigned long SYS_getpgrp() {
	SYSCALL_DEBUG("getpgrp(Hardcoded) :\n");
	return 0x123;
}
unsigned long SYS_rt_sigaction() {
	SYSCALL_DEBUG("sigaction(Dummy) \n");
	return SYSCALL_SUCCESS;
}
#define TIOCSPGRP 0x5410
unsigned long SYS_ioctl(int d, int request, unsigned long *addr) {//TODO
	SYSCALL_DEBUG("ioctl(Dummy) d:%x request:%x addr:%x\n", d, request, addr);
	if (request == TIOCSPGRP && addr != 0) {
		*addr = temp_pgid;
		return 0;
	}
	if (addr != 0) {
		*addr = 0x123;
	}
	return SYSCALL_SUCCESS;
}
unsigned long SYS_fs_readlink(unsigned char *path, char *buf, int bufsiz) {
	struct file *fp;
	int ret = -2; /* no such file exists */

	SYSCALL_DEBUG("readlink (ppath:%x(%s) buf:%x \n", path, path, buf);

	if (path == 0 || buf == 0)
		return ret;

	buf[0] = '\0';
	fp = (struct file *) fs_open((unsigned char *) path, 0, 0);
	if (fp == 0) {
		return ret;
	}

	if (fp->inode->file_type == SYM_LINK_FILE) {
		ret = fs_read(fp, buf, bufsiz);
	}
	fs_close(fp);

	SYSCALL_DEBUG("RET readlink (ppath:%x(%s) buf:%s: \n", path, path, buf);
	return ut_strlen(buf);
}

unsigned long SYS_fs_stat(const char *path, struct stat *buf) {
	struct file *fp;
	struct fileStat fstat;
	int ret = -2; /* no such file exists */

	SYSCALL_DEBUG(
			"Stat (ppath:%x(%s) buf:%x size:%d\n", path, path, buf, sizeof(struct stat));

	if (path == 0 || buf == 0)
		return ret;

	fp = (struct file *) fs_open((unsigned char *) path, 0, 0);
	if (fp == 0) {
		return ret;
	}
	ret = fs_stat(fp, &fstat);
	ut_memset((unsigned char *) buf, 0, sizeof(struct stat));
	buf->st_size = fstat.st_size;
	buf->st_ino = fstat.inode_no;

	buf->st_mtime.tv_sec = fstat.mtime;
	buf->st_mtime.tv_nsec = 0;
	buf->st_atime.tv_sec = fstat.atime;
	buf->st_atime.tv_nsec = 0;

	buf->st_mode = (fstat.mode | fstat.type) & 0xfffffff;
	buf->st_dev = 2054;
	buf->st_rdev = 0;

	/* TODO : fill the rest of the fields from fstat */
	buf->st_gid = TEMP_UID;
	buf->st_uid = TEMP_UID;

	buf->__unused[0] = buf->__unused[1] = buf->__unused[2] = 0;
	buf->__pad0 = 0;

	buf->st_blksize = 4096; //TODO
	buf->st_blocks = 8; //TODO
	buf->st_nlink = 4; //TODO
	if (fstat.type == SYM_LINK_FILE) {
		buf->st_blocks = 0; //TODO
		buf->st_nlink = 1; //TODO
	}
	fs_close(fp);
	ret = SYSCALL_SUCCESS;
	/*
	 *stat(".", {st_dev=makedev(8, 6), st_ino=5381699, st_mode=S_IFDIR|0775, st_nlink=4,
	 *          st_uid=500, st_gid=500, st_blksize=4096, st_blocks=8, st_size=4096, st_atime=2012/09/29-23:41:20,
	 *          st_mtime=2012/09/16-11:29:50, st_ctime=2012/09/16-11:29:50}) = 0
	 *
	 */
	SYSCALL_DEBUG(
			" stat END : st_size: %d st_ino:%d nlink:%x mode:%x uid:%x gid:%x blksize:%x ret:%x mtime: %x :%x sec\n", buf->st_size, buf->st_ino, buf->st_nlink, buf->st_mode, buf->st_uid, buf->st_gid, buf->st_blksize, ret, buf->st_mtime.tv_sec, fstat.mtime);
	return ret;
}

/************************* getdents ******************************/
struct linux_dirent {
	unsigned long d_ino; /* Inode number */
	unsigned long d_off; /* Offset to next linux_dirent */
	unsigned short d_reclen; /* Length of this linux_dirent */
	char d_name[]; /* Filename (null-terminated) */
};

unsigned long SYS_getdents(unsigned int fd, unsigned char *user_buf, int size) {
	struct file *fp = 0;
	unsigned char *temp_buf = 0;
	struct linux_dirent *dirp;
	int i, tret, len;
	int ret = -1;
	struct dirEntry *dir_ent;

	SYSCALL_DEBUG("getidents fd:%d userbuf:%x size:%d \n", fd, user_buf, size);
	fp = fd_to_file(fd);
	if (fp <= 0 || user_buf == 0) {
		ret = -1;
		return ret;
	}

	ret = fs_readdir(fp, user_buf, size,&fp->offset);
last:
	return ret;
}
/*********************** end of getdents *************************/

kmem_cache_t *g_slab_filep;
unsigned long SYS_fs_fcntl(int fd, int cmd, int args) {
	struct file *fp_old,*fp_new;
	int i;
	int new_fd=args;

	SYSCALL_DEBUG("fcntl(Partial)  fd:%x cmd:%x args:%x\n", fd, cmd, args);
	if (cmd == F_DUPFD){
		fp_old = fd_to_file(fd);
		if (fp_old==0 ) return -1;
		fp_new = mm_slab_cache_alloc(g_slab_filep, 0);
		if (fp_new==0 ) return -1;

		if (new_fd > g_current_task->mm->fs.total && new_fd<MAX_FDS){
			g_current_task->mm->fs.total = new_fd +1;
			g_current_task->mm->fs.filep[new_fd]=fp_new;
			fs_dup(fp_old,fp_new);
			return new_fd;
		}
		for (i = new_fd; i < MAX_FDS; i++) {
			if (g_current_task->mm->fs.filep[i] == 0) {
				if (i >= g_current_task->mm->fs.total ){
					g_current_task->mm->fs.total = i+1;
				}
				g_current_task->mm->fs.filep[i]=fp_new;
				fs_dup(fp_old,fp_new);
				return i;
			}
		}
		mm_slab_cache_free(g_slab_filep, fp_new);
		return -1;
	}
	return -1;
}

unsigned long SYS_futex(unsigned long *a) {//TODO
	SYSCALL_DEBUG("futex  addr:%x \n", a);
	*a = 0;
	return -1;
}

unsigned long SYS_sysctl(struct __sysctl_args *args) {
	SYSCALL_DEBUG("sysctl  args:%x \n", args);
	int *name;
	unsigned char *carg[10];
	int i;

	if (args == 0)
		return -1; /* -1 is error */

	name = args->name[0];
//	ut_printf(" sysctl name.. addr:%x: %x: %x: nlen:%d: \n", name[0], name[1], name[2], args->nlen);

	for (i = 0; i < 10; i++) {
		carg[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		carg[i] = 0;
		if (name[i] != 0) {
			carg[i] = name[i];
		}
	}
	if (carg[0] != 0 && ut_strcmp(carg[0], "set") == 0) {
		ut_symbol_execute(SYMBOL_CONF, carg[1], carg[2]);

	} else {
		if (carg[0] == 0) {
			ut_printf("Conf variables:\n");
			ut_symbol_show(SYMBOL_CONF);
			ut_printf("Cmd variables:\n");
			ut_symbol_show(SYMBOL_CMD);
		} else {
			ut_symbol_execute(SYMBOL_CMD, carg[0], carg[1], carg[2]);
		}
	}

	return SYSCALL_SUCCESS; /* success */
}

unsigned long SYS_wait4(int pid, void *status, unsigned long option,
		void *rusage) {
	//SYSCALL_DEBUG("wait4(Dummy)  pid:%x status:%x option:%x rusage:%x\n",pid,status,option,rusage);
	unsigned long child_id;
	unsigned long flags;
	struct task_struct *child_task = 0;
#define WNOHANG 1

	struct list_head *node;
	node = g_current_task->dead_tasks.head.next;
	if (list_empty(&(g_current_task->dead_tasks.head.next))) {
		if (!(option & WNOHANG)){
			sc_sleep(50);
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
		child_id = child_task->pid;
		ut_log("wait child exited :%s: exitcode:%d \n",child_task->name,child_task->exit_code);
		sc_delete_task(child_task);
		SYSCALL_DEBUG(" wait returning the child id :%d(%x) \n", child_id, child_id);
		return child_id;
	} else {
		return 0;
	}
}
unsigned long SYS_chdir(unsigned char *filename) {
	int ret;
	struct file *fp;
	SYSCALL_DEBUG("chdir  buf:%s \n", filename);
	if (filename == 0)
		return SYSCALL_FAIL;
	fp = (struct file *) fs_open((unsigned char *) filename, 0, 0);
	if (fp == 0) {
		if (ut_strcmp(filename, "/") != 0)
			return SYSCALL_FAIL;
	} else {
		fs_close(fp);
	}
	ut_strncpy(g_current_task->mm->fs.cwd, filename, MAX_FILENAME);
	ret = ut_strlen(g_current_task->mm->fs.cwd);

	return SYSCALL_SUCCESS;
}

unsigned long SYS_getcwd(unsigned char *buf, int len) {
	int ret = SYSCALL_FAIL;

	SYSCALL_DEBUG("getcwd  buf:%x len:%d  \n", buf, len);
	if (buf == 0)
		return ret;
	ut_strncpy(buf, g_current_task->mm->fs.cwd, len);
	ret = ut_strlen(buf);

	return ret;
}

/*************************************
 * TODO : partially implemented calls
 * **********************************/

unsigned long SYS_poll(struct pollfd *fds, int nfds, int timeout) {
	SYSCALL_DEBUG(
			"poll(partial)  fds:%x nfds:%d timeout:%d  \n", fds, nfds, timeout);
	if (nfds == 0 || fds == 0 || timeout == 0)
		return 0;
	return 0;
}
unsigned long SYS_setsockopt(int sockfd, int level, int optname,
		const void *optval, int optlen) {
	SYSCALL_DEBUG(
			"Setsocklopt (TODO) fd:%d level:%d optname:%d optval:%x optlen:%d\n", sockfd, level, optname, optval, optlen);
	if (optname == 8) /* SO_RECVBUF */
	{

	}
	return SYSCALL_FAIL;
}

unsigned long SYS_exit_group() {
	SYSCALL_DEBUG("exit_group :\n");
	SYS_sc_exit(103);
	return SYSCALL_SUCCESS;
}

