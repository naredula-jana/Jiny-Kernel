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
#include "interface.h"
#include "isr.h"

unsigned long SYS_printf(unsigned long *args);

unsigned long SYS_fork();
int g_conf_syscall_debug=1;
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
int SYS_fs_stat(const char *path, struct stat *buf);
unsigned long SYS_fs_fstat(int fd, void *buf);
unsigned long SYS_fs_dup2(int fd1, int fd2);
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
unsigned long SYS_wait4(void *arg1, void *arg2, void *arg3, void *arg4);
struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};
unsigned long SYS_poll(struct pollfd *fds, int nfds, int timeout);

unsigned long SYS_nanosleep(const struct timespec *req, struct timespec *rem);
unsigned long SYS_getcwd(unsigned char *buf, int len);
unsigned long SYS_fs_fcntl(int fd, int cmd, void *args);

typedef struct {
	void *func;
} syscalltable_t;

syscalltable_t syscalltable[] = {
/* 0 */
{ SYS_fs_read },/* 0 */{ SYS_fs_write }, { SYS_fs_open }, { SYS_fs_close }, { SYS_fs_stat }, { SYS_fs_fstat }, /* 5 */
{ snull }, { SYS_poll }, { snull }, { SYS_vm_mmap }, { SYS_vm_mprotect },/* 10 */
{ SYS_vm_munmap }, { SYS_vm_brk }, { SYS_rt_sigaction }, { snull }, { snull }, /* 15 */
{ SYS_ioctl }, { snull }, { snull }, { SYS_fs_readv }, { SYS_fs_writev }, /* 20 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 25 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 30 */
{ snull }, { snull }, { SYS_fs_dup2 }, { snull }, { SYS_nanosleep }, /* 35 = nanosleep */
{ snull }, { snull }, { snull }, { SYS_getpid }, { snull }, /* 40 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 45 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 50 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 55 */
{ SYS_sc_clone }, { SYS_sc_fork }, { snull }, { SYS_sc_execve }, { SYS_sc_exit }, /* 60 */
{ SYS_wait4 }, { SYS_sc_kill }, { SYS_uname }, { snull }, { snull }, /* 65 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 70 */
{ snull }, { SYS_fs_fcntl }, { snull }, { snull }, { SYS_fs_fdatasync }, /* 75 */
{ snull }, { snull }, { snull }, { SYS_getcwd }, { snull }, /* 80 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 85 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 90 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 95 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 100 */
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
{ snull }, { snull }, { SYS_arch_prctl }, { snull }, { snull }, /* 160 */
{ snull }, { snull }, { snull }, { snull }, { snull }, /* 165 */
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
static int init_utsname()
{
	ut_strcpy(g_utsname.sysname,(unsigned char *)"Linux");
	ut_strcpy(g_utsname.nodename,(unsigned char *)"njana-desk");
	ut_strcpy(g_utsname.release,(unsigned char *)"2.6.35-22-generic");
	ut_strcpy(g_utsname.version,(unsigned char *)"#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010");
	ut_strcpy(g_utsname.machine,(unsigned char *)"x86_64");
	return 1;
}
static int init_uts_done=0;

unsigned long SYS_uname(unsigned long *args)
{
	SYSCALL_DEBUG("uname args:%x \n",args);
	if (init_uts_done==0) init_utsname();
	ut_printf(" Inside uname : %s \n",g_utsname.sysname);
	ut_memcpy((unsigned char *)args,(unsigned char *)&g_utsname,sizeof(g_utsname));
	return 0;
}

#define ARCH_SET_FS 0x1002
unsigned long SYS_arch_prctl(unsigned long code,unsigned long addr)
{
	SYSCALL_DEBUG("sys arc_prctl : code :%x addr:%x \n",code,addr);
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
	SYSCALL_DEBUG("nanosleep sec:%d nsec:%d:\n",req->tv_sec,req->tv_nsec);
	if (req == 0)
		return 0;
	ticks = req->tv_sec * 100;
	ticks = ticks + (req->tv_nsec / 100000);
	sc_sleep(ticks); /* units of 10ms */
	return 0;
}
unsigned long SYS_getppid() {
	SYSCALL_DEBUG("getppid :\n");
	return g_current_task->ppid;
}
unsigned long snull(unsigned long *args)
{
	unsigned long syscall_no;

	asm volatile("movq %%rax,%0" : "=r" (syscall_no));
	ut_printf("ERROR: SYSCALL null as hit :%d \n",syscall_no);	
	SYS_sc_exit(123);
	return 1;
}

long int SYS_time(__time_t *time){
	SYSCALL_DEBUG("time :%x \n",time);
	if (time==0) return 0;
	*time = g_jiffies;
	return *time;
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
	SYSCALL_DEBUG("setuid(Hardcoded) :%x(%d)\n",uid,uid);
	return 0;
}
unsigned long SYS_setgid(unsigned long gid) {
	SYSCALL_DEBUG("setgid(Hardcoded) :%x(%d)\n",gid,gid);
	return 0;
}
static unsigned long temp_pgid=0;
unsigned long SYS_setpgid(unsigned long pid, unsigned long gid) {
	SYSCALL_DEBUG("setpgid(Hardcoded) :%x(%d)\n",gid,gid);
	temp_pgid=gid;
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
unsigned long SYS_rt_sigaction(){
	SYSCALL_DEBUG("sigaction(Dummy) \n");
	return 0;
}
#define TIOCSPGRP 0x5410
unsigned long SYS_ioctl(int d, int request, unsigned long *addr){
	SYSCALL_DEBUG("ioctl(Dummy) d:%x request:%x addr:%x\n",d,request,addr);
	if (request == TIOCSPGRP && addr != 0) {
      *addr=temp_pgid;
      return 0;
	}
	if (addr != 0) {
	   *addr=0x123;
	}
	return 0;
}

int SYS_fs_stat(const char *path, struct stat *buf)
{
	struct file *fp;
	struct fileStat fstat;
	int ret;
	SYSCALL_DEBUG("stat( ppath:%x(%s) buf:%x size:%d\n",path,path,buf,sizeof(struct stat));
//return -1;
	if (path==0 || buf==0) return -1;

	fp=(struct file *)fs_open((unsigned char *)path,0,0);
	if ( fp==0 ){

		return -1;
	}
	ret = fs_stat(fp, &fstat);
	ut_memset((unsigned char *)buf,0,sizeof(struct stat));
	buf->st_size = fstat.st_size ;
	buf->st_ino = fstat.inode_no ;
    buf->st_blksize = 4096;
    buf->st_blocks = 8;
    buf->st_nlink = 49;
    buf->st_mtime.tv_sec =  fstat.mtime/1000000;
    buf->st_mtime.tv_nsec = fstat.mtime;
    buf->st_atime.tv_sec = fstat.atime/1000000;
    buf->st_atime.tv_nsec = fstat.atime;
    buf->st_mode = fstat.mode;

/* TODO : fill the rest of the fields from fstat */
   buf->st_gid = TEMP_UID;
   buf->st_uid = TEMP_UID;

    fs_close(fp);
    /*
     *stat(".", {st_dev=makedev(8, 6), st_ino=5381699, st_mode=S_IFDIR|0775, st_nlink=4,
     *          st_uid=500, st_gid=500, st_blksize=4096, st_blocks=8, st_size=4096, st_atime=2012/09/29-23:41:20,
     *          st_mtime=2012/09/16-11:29:50, st_ctime=2012/09/16-11:29:50}) = 0
     *
     */
    SYSCALL_DEBUG(" stat END : st_size: %d st_ino:%d mode:%x uid:%x gid:%x blksize:%x\n",
    		buf->st_size,buf->st_ino, buf->st_mode, buf->st_uid, buf->st_gid, buf->st_blksize );
	return ret;
}

unsigned long SYS_fs_dup2(int fd1, int fd2)
{
	SYSCALL_DEBUG("dup2(hardcoded)  fd1:%x fd2:%x \n",fd1,fd2);
	return fd2;
}
unsigned long SYS_fs_fstat(int fd, void *buf)
{
	struct file *fp;
	SYSCALL_DEBUG("fstat  fd:%x buf:%x \n",fd,buf);
//return 0;
	fp=fd_to_file(fd);
	if (fp <= 0 || buf==0) return -1;
	return fs_stat(fp, buf);
}
unsigned long SYS_futex(unsigned long *a){
	SYSCALL_DEBUG("futex  addr:%x \n",a);
	*a=0;
	return 1;
}
unsigned long SYS_fs_fcntl(int fd, int cmd, void *args) {
	SYSCALL_DEBUG("fcntl(Dummy)  fd:%x cmd:%x args:%x\n",fd,cmd,args);
	return 0;
}
unsigned long SYS_wait4(void *arg1, void *arg2, void *arg3, void *arg4) {
	SYSCALL_DEBUG("wait4(Dummy)  arg1:%x arg2:%x arg3:%x\n",arg1,arg2,arg3);
	return 0;
}
/*************************************
 * TODO : partially implemented calls
 * **********************************/

unsigned long SYS_poll(struct pollfd *fds, int nfds, int timeout) {
	SYSCALL_DEBUG("poll(partial)  fds:%x nfds:%d timeout:%d  \n",fds,nfds,timeout);
	if (nfds==0 || fds==0 || timeout==0) return 0;
    return 0;
}
unsigned long SYS_getcwd(unsigned char *buf, int len) {
	SYSCALL_DEBUG("getcwd(partial)  buf:%x len:%d  \n",buf,len);
	if (buf == 0) return 0;
	ut_strcpy(buf,(unsigned char *)"/root");
     return (unsigned long)buf;
}
unsigned long SYS_exit_group(){
	SYSCALL_DEBUG("exit_group :\n");
	SYS_sc_exit(103);
	return 0;
}

