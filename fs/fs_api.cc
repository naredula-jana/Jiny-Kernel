/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/fs_api.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
//#define DEBUG_ENABLE 1

#include "file.hh"
extern "C"{
#include "common.h"
#include "mm.h"

#include "interface.h"
extern kmem_cache_t *g_slab_filep;
struct timespec {
    long   tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};
struct timeval {
    long         tv_sec;     /* seconds */
    long    tv_usec;    /* microseconds */
};

/* TODO : locking need to added */
struct file *fs_create_filep(int *fd, struct file *in_fp) {
	int i;
	struct file *fp;

	for (i = 3; i < MAX_FDS; i++) {
		if (g_current_task->mm->fs->filep[i] == 0) {
			if (in_fp !=0 ){
				fp = in_fp;
			}else{
				fp = (struct file *)mm_slab_cache_alloc(g_slab_filep, 0);
			}
			if (fp == 0)
				return 0;
			if (i >= g_current_task->mm->fs->total) {
				g_current_task->mm->fs->total = i + 1;
			}
			g_current_task->mm->fs->filep[i] = fp;
			if (fd != 0)
				*fd = i;
			return fp;
		}
	}

	return 0;
}
int fs_destroy_filep(int fd) {
	struct file *fp;
	if (fd < 0 || fd > MAX_FDS) {
		return -1;
	}
	fp = g_current_task->mm->fs->filep[fd];
	g_current_task->mm->fs->filep[fd] = 0;
	mm_slab_cache_free(g_slab_filep, fp);
	return 0;
}
unsigned long SYS_fs_access(char *filename, int mode) {
	struct file *filep;
	int ret=0;;
	filep  = (struct file *)fs_open((uint8_t *) filename, 0, mode);
	if (filep == 0) {
		ret = -1;
		return ret;
	}else{
		fs_close(filep);
		return 0;
	}
}
unsigned long SYS_fs_open(char *filename, int mode, int flags) {
	struct file *filep;
	int ret= SYSCALL_FAIL;

	SYSCALL_DEBUG("open: filename :%s: mode:%x flags:%x \n", filename, mode, flags);
	if (ut_strcmp((uint8_t *) filename, (uint8_t *) "/dev/tty") == 0) {
		ret = 1;
		goto last;
	}
	if (ut_strcmp((uint8_t *) filename, (uint8_t *) "/dev/null") == 0) {
		filep  = (struct file *)fs_create_filep(&ret, 0);
		if (filep == 0) {
			ret = -1;
			goto fail;
		}
		filep->type = DEV_NULL_FILE;
	}else{
		filep = (struct file *) fs_open((uint8_t *) filename, flags, mode);
		if (filep != 0){
			if (fs_create_filep(&ret, filep)==0) {
				ret = -1;
				goto fail;
			}
		}else{
			ret = -2;
			goto fail;
		}
	}
	goto last;
#if 0
	total = g_current_task->mm->fs.total;
	if (filep != 0 && total < MAX_FDS) {
		for (i = 3; i < MAX_FDS; i++) { /* fds: 0,1,2 are for in/out/error */
			if (g_current_task->mm->fs.filep[i] == 0) {
				break;
			}
		}
		if (i == MAX_FDS){
			goto fail;
		}
		g_current_task->mm->fs.filep[i] = filep;
		if (i >= total)
			g_current_task->mm->fs.total = i + 1;

		ret = i;
		goto last;
	}
#endif
fail:
	if (filep != 0)
		fs_close(filep);

last:
	SYSCALL_DEBUG("open return value: %d \n", ret);
	return ret;
}
int SYS_fs_dup2(int fd_old, int fd_new);
int SYS_fs_dup(int fd_old) {
	int i;
	for (i = 3; i < MAX_FDS; i++) { /* fds: 0,1,2 are for in/out/error */
		if (g_current_task->mm->fs->filep[i] == 0) {
			break;
		}
	}
	if (i==MAX_FDS) return SYSCALL_FAIL;
	return SYS_fs_dup2(fd_old, i);
}
int SYS_fs_dup2(int fd_old, int fd_new) {
	int ret = fd_new; /* on success */
	struct file *fp_new, *fp_old;
	SYSCALL_DEBUG("dup2  fd_new:%x fd_old:%x \n", fd_new, fd_old);
	fp_new = fd_to_file(fd_new);
	fp_old = fd_to_file(fd_old);
	if ( fp_old == 0){
		ret = SYSCALL_FAIL;
		goto last;
	}
	fp_new = fs_dup(fp_old, fp_new);
	if (fp_new ==0){
		ret = SYSCALL_FAIL;
	}
	g_current_task->mm->fs->filep[fd_new] = fp_new;

last:
	SYSCALL_DEBUG("dup2 Return ret:%d new_fd:%d \n",ret,fd_new);
	return ret;
}
int SYS_fs_read(unsigned long fd, uint8_t *buff, unsigned long len) {
	struct file *file;
	int ret;

	SYSCALL_DEBUG("read fd:%d buff:%x len:%x \n",fd,buff,len);

	file = fd_to_file(fd);
	if (file == 0){
		ut_log("ERRO read fd:%d buff:%x len:%x \n", fd, buff, len);
		return 0;
	}

	if (file->type == NETWORK_FILE) {
		//return socket_read(file, buff, len);
	} else if (file->type == DEV_NULL_FILE) {
		//ut_memset(buff,0,len-1);
		return len;
	}

	ret = fs_read(file, buff, len);
	SYSCALL_DEBUG("read ret :%d\n",ret);
	return ret;
}
long SYS_fs_readv(int fd, const struct iovec *iov, int iovcnt) {
	int i;
	long ret, tret;
	struct file *file;

	SYSCALL_DEBUG("readv: fd:%d iovec:%x count:%d\n", fd, iov, iovcnt);
	file = fd_to_file(fd);
	ret = 0;
	for (i = 0; i < iovcnt; i++) {
		tret = fs_read(file,(uint8_t *) iov[i].iov_base, iov[i].iov_len);
		if (tret > 0)
			ret = ret + tret;
		else
			goto last;
	}
	last: return ret;
}
int SYS_fs_write(unsigned long fd, uint8_t *buff, unsigned long len) {
	struct file *file;
	int ret;

	//SYSCALL_DEBUG("write fd:%d buff:%x len:%x data:%s:\n", fd, buff, len,buff);
	file = fd_to_file(fd);

	if (file == 0){
		ut_log("ERROR write fd:%d buff:%x len:%x \n", fd, buff, len);
		return -1; // TODO : temporary
	}
	if (file->type == NETWORK_FILE) {
	//	return socket_write(file, buff, len);
	}
	ret = fs_write(file, buff, len);
	//SYSCALL_DEBUG("write return : fd:%d ret:%d \n",fd,ret);
	return ret;
}
long SYS_fs_writev(int fd, const struct iovec *iov, int iovcnt) {
	int i;
	long ret, tret;
	struct file *file;

	//SYSCALL_DEBUG("writev: fd:%d iovec:%x count:%d\n", fd, iov, iovcnt);

	file = fd_to_file(fd);
	ret = 0;
	for (i = 0; i < iovcnt; i++) {
		if (fd == 1 || fd == 2) {
			ut_printf("write %s\n", iov[i].iov_base);/* TODO need to terminate the buf with \0  */
			ret = ret + iov[i].iov_len;
			continue;
		}
		tret = fs_write(file, (uint8_t *)iov[i].iov_base, iov[i].iov_len);
		if (tret > 0)
			ret = ret + tret;
		else
			goto last;
	}
	last: return ret;
}
unsigned long SYS_fs_fdatasync(unsigned long fd) {
	struct file *file;

	SYSCALL_DEBUG("fdatasync fd:%d \n", fd);
	file = fd_to_file(fd);
	return fs_fdatasync(file);
}
unsigned long SYS_fs_lseek(unsigned long fd, unsigned long offset, int whence) {
	struct file *file;

	SYSCALL_DEBUG("lseek fd:%d offset:%x whence:%x \n", fd, offset, whence);
	file = fd_to_file(fd);
	if ( file == 0)
		return -1;
	if (whence == SEEK_SET) {
		file->offset = offset;
	} else if (whence == SEEK_CUR) {
		file->offset = file->offset + offset;
	} else
		return -1;

	return file->offset;
}
unsigned long SYS_fs_fadvise(unsigned long fd, unsigned long offset,
		unsigned long len, int advise) {
	struct file *file;
	void *inode;

	SYSCALL_DEBUG(
			"fadvise fd:%d offset:%d len:%d advise:%x \n", fd, offset, len, advise);
	file = fd_to_file(fd);
	if (file == 0 || file->vinode == 0)
		return 0;
	inode = file->vinode;
	return fs_fadvise(inode, offset, len, advise);
}
unsigned long SYS_fs_close(unsigned long fd) {
	struct file *file;

	SYSCALL_DEBUG("close fd:%d \n", fd);
	if (fd == 1) {
		SYSCALL_DEBUG("close fd:%d NOT CLOSING Temporary fix \n", fd);
		return 0; //TODO : temporary
	}
	file = fd_to_file(fd);
	if (file == 0)
		return SYSCALL_FAIL;
	g_current_task->mm->fs->filep[fd] = 0;

	return fs_close(file);
}

unsigned long SYS_fs_readlink(uint8_t *path, uint8_t *buf, int bufsiz) {
	struct file *fp;
	int ret = -2; /* no such file exists */

	SYSCALL_DEBUG("readlink (ppath:%x(%s) buf:%x \n", path, path, buf);

	if (path == 0 || buf == 0)
		return ret;

	buf[0] = '\0';
	fp = (struct file *) fs_open((uint8_t *) path, 0, 0);
	if (fp == 0) {
		return ret;
	}
	struct fs_inode *inode=(struct fs_inode *)fp->vinode;
	if (inode!=0 && fs_get_inode_type(inode) == SYM_LINK_FILE) {
		ret = fs_read(fp, buf, (unsigned long)bufsiz);
	}
	fs_close(fp);

	SYSCALL_DEBUG("RET readlink (ppath:%x(%s) buf:%s: \n", path, path, buf);
	return ut_strlen((const uint8_t *)buf);
}
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
#define TEMP_UID 26872
#define TEMP_GID 500
static int copy_stat_touser(struct file *fp,struct stat *buf){
	struct fileStat fstat;
	int ret;

	ret = fs_stat(fp, &fstat);
	ut_memset((uint8_t *) buf, 0, sizeof(struct stat));
	if (fp->type == IN_FILE || fp->type == OUT_FILE) {

	} else {
		buf->st_size = fstat.st_size;
		buf->st_ino = fstat.inode_no;
		buf->st_blksize = 4096; //TODO
		buf->st_blocks = 8; //TODO
		buf->st_nlink = 4; //TODO
	}

	buf->st_mtime.tv_sec = fstat.mtime;
	buf->st_mtime.tv_nsec = 0;
	buf->st_atime.tv_sec = fstat.atime;
	buf->st_atime.tv_nsec = 0;

	buf->st_mode = (fstat.mode | fstat.type) & 0xfffffff;
	buf->st_dev = 2054;
	buf->st_rdev = 0;

	/* TODO : fill the rest of the fields from fstat */
	buf->st_gid = TEMP_GID;
	buf->st_uid = TEMP_UID;

	buf->__unused[0] = buf->__unused[1] = buf->__unused[2] = 0;
	buf->__pad0 = 0;


	if (fstat.type == SYM_LINK_FILE) {
		buf->st_blocks = 0; //TODO
		buf->st_nlink = 1; //TODO
	}
	return ret;
}

unsigned long SYS_fs_stat(const char *path, struct stat *buf) {
	struct file *fp;
	struct fileStat fstat;
	int ret = -2; /* no such file exists */

	SYSCALL_DEBUG("Stat (ppath:%x(%s) buf:%x size:%d\n", path, path, buf, sizeof(struct stat));

	if (path == 0 || buf == 0)
		return ret;

	fp = (struct file *) fs_open((uint8_t *) path, 0, 0);
	if (fp == 0) {
		return ret;
	}

	ret = copy_stat_touser(fp,buf);
	fs_close(fp);
	ret = SYSCALL_SUCCESS;
	/*
	 *stat(".", {st_dev=makedev(8, 6), st_ino=5381699, st_mode=S_IFDIR|0775, st_nlink=4,
	 *          st_uid=500, st_gid=500, st_blksize=4096, st_blocks=8, st_size=4096, st_atime=2012/09/29-23:41:20,
	 *          st_mtime=2012/09/16-11:29:50, st_ctime=2012/09/16-11:29:50}) = 0
	 *
	 */
//	SYSCALL_DEBUG(
//			" stat END : st_size: %d st_ino:%d nlink:%x mode:%x uid:%x gid:%x blksize:%x ret:%x mtime: %x :%x sec\n", buf->st_size, buf->st_ino, buf->st_nlink, buf->st_mode, buf->st_uid, buf->st_gid, buf->st_blksize, ret, buf->st_mtime.tv_sec, fstat.mtime);
	return ret;
}
unsigned long SYS_fs_fstat(int fd, struct stat *buf) {
	struct file *fp;
	int ret;
	SYSCALL_DEBUG("fstat  fd:%x buf:%x \n", fd, buf);

	fp = fd_to_file(fd);
	if (fp <= 0 || buf == 0)
		return -1;

	ret = copy_stat_touser(fp,buf);
	if (buf){
		SYSCALL_DEBUG("fstat ret: file size:%d mode:%x inode:%x  \n",buf->st_size,buf->st_mode,buf->st_ino);
	}
	return ret;
}
/************************* getdents ******************************/
struct linux_dirent {
	unsigned long d_ino; /* Inode number */
	unsigned long d_off; /* Offset to next linux_dirent */
	unsigned short d_reclen; /* Length of this linux_dirent */
	char d_name[]; /* Filename (null-terminated) */
};

unsigned long SYS_getdents(unsigned int fd, uint8_t *user_buf, int size) {
	struct file *fp = 0;
	int ret = -1;

	SYSCALL_DEBUG("getidents fd:%d userbuf:%x size:%d \n", fd, user_buf, size);
	fp = fd_to_file(fd);
	if (fp <= 0 || user_buf == 0) {
		ret = -1;
		return ret;
	}

	ret = fs_readdir(fp, (dirEntry *)user_buf, size,(int *)&fp->offset);
	return ret;
}
/*********************** end of getdents *************************/


unsigned long SYS_fs_fcntl(int fd, int cmd, int args) {
	struct file *fp_old,*fp_new;
	int i;
	int new_fd=args;
	int ret = SYSCALL_FAIL;

	SYSCALL_DEBUG("fcntl(Partial)  fd:%x cmd:%x args:%x\n", fd, cmd, args);
	if (cmd == F_DUPFD){
		fp_old = fd_to_file(fd);
		if (fp_old==0 ) return SYSCALL_FAIL;
		fp_new = (struct file *)mm_slab_cache_alloc(g_slab_filep, 0);
		if (fp_new==0 ) return SYSCALL_FAIL;

		if (new_fd > g_current_task->mm->fs->total && new_fd<MAX_FDS){
			g_current_task->mm->fs->total = new_fd +1;
			g_current_task->mm->fs->filep[new_fd]=fp_new;
			fs_dup(fp_old,fp_new);
			ret = new_fd;
			goto last;
		}
		for (i = new_fd; i < MAX_FDS; i++) {
			if (g_current_task->mm->fs->filep[i] == 0) {
				if (i >= g_current_task->mm->fs->total ){
					g_current_task->mm->fs->total = i+1;
				}
				g_current_task->mm->fs->filep[i]=fp_new;
				fs_dup(fp_old,fp_new);
				ret = i;
				goto last;
			}
		}
		mm_slab_cache_free(g_slab_filep, fp_new);
		ret = SYSCALL_FAIL;
		goto last;
	}else if (cmd == F_SETFD){
		if (args == FD_CLOSEXEC){
			fp_old = fd_to_file(fd);
			if (fp_old==0 ) return SYSCALL_FAIL;
			fp_old->flags = FD_CLOSEXEC;
			ret = SYSCALL_SUCCESS;
		}
	}else if (cmd == F_GETFD){
		fp_old = fd_to_file(fd);
		if (fp_old==0 ) return SYSCALL_FAIL;
		ret = fp_old->flags;
	}
last:
	SYSCALL_DEBUG("fcntlret  fd:%x ret:%x(%d)\n",fd,ret,ret);
	return ret;
}

extern int fs_sync();
int SYS_sync(){
	return fs_sync();
}

extern struct list_head fs_inode_list;
int Jcmd_ls(uint8_t *arg1, uint8_t *arg2) {
	struct fs_inode *tmp_inode;
	struct list_head *p;
	int i,len,max_len;
	uint8_t *buf;
	int total_pages = 0;
	const uint8_t *type;
	int active=1;

	ut_printf("usagecount nrpages length  inode_no type/type  name (in/out/err)\n");
	len = PAGE_SIZE*100;
	max_len=len;
	buf = (uint8_t *) vmalloc(len,0);
	if (buf == 0) {
		ut_printf(" Unable to get vmalloc memory\n");
		return 0;
	}
	if (arg1!=0 && ut_strcmp(arg1,(uint8_t *)"all")==0){
		active=0;
	}
	mutexLock(g_inode_lock);
	list_for_each(p, &fs_inode_list) {
		tmp_inode = list_entry(p, struct fs_inode, inode_link);
		if (active==1 && tmp_inode->count.counter==1) continue;
		if (tmp_inode->flags & INODE_EXECUTING) type=(uint8_t *)"*";
		else type=(uint8_t *)" ";
		if (tmp_inode->file_type == NETWORK_FILE) {
			len = len
					- ut_snprintf(buf + max_len - len, len,
							"%d: %4d %4d(%2d) %8d %8d %2x/%x %s", i,
							tmp_inode->count, tmp_inode->nrpages,
							tmp_inode->stat_locked_pages.counter, 0,
							tmp_inode->fileStat.inode_no, tmp_inode->file_type,
							tmp_inode->flags, type);
		} else {
			len = len
					- ut_snprintf(buf + max_len - len, len,
							"%d: %4d %4d(%2d) %8d %8d %2x/%x %s", i,
							tmp_inode->count, tmp_inode->nrpages,
							tmp_inode->stat_locked_pages.counter,
							tmp_inode->fileStat.st_size,
							tmp_inode->fileStat.inode_no, tmp_inode->file_type,
							tmp_inode->flags, type );
		}
		len = len - ut_snprintf(buf + max_len - len, len,"(%d/%d/%d-%s\n",tmp_inode->stat_in,tmp_inode->stat_out,tmp_inode->stat_err,tmp_inode->filename);
		total_pages = total_pages + tmp_inode->nrpages;
		i++;
		if (len < 500) break;
	}
	mutexUnLock(g_inode_lock);
	socket::print_stats();
	len = len - ut_snprintf(buf+max_len-len, len,"Total pages :%d\n", total_pages);

	len = ut_strlen(buf);
	SYS_fs_write(1,buf,len);

	vfree((unsigned long)buf);
	return 1;
}

int Jcmd_clear_pagecache() {
	struct fs_inode *tmp_inode;
	struct list_head *p;

	mutexLock(g_inode_lock);  // Need to implement Recursive lock
	list_for_each(p, &fs_inode_list) {
		tmp_inode = list_entry(p, struct fs_inode, inode_link);
		ut_printf("%s %d %d %d \n", tmp_inode->filename, tmp_inode->count,
				tmp_inode->nrpages, tmp_inode->fileStat.st_size);
		fs_fadvise(tmp_inode, 0, 0, POSIX_FADV_DONTNEED);
	}
	mutexUnLock(g_inode_lock);

	return 1;
}
extern int fs_sync();
int Jcmd_sync() {
	return fs_sync();
}

#if 0
int get_output_device(){

	struct vinode *v=(struct vinode *)g_current_task->mm->fs.filep[1]->vinode;
	return v->ioctl(0,0);
}
#endif
int fs_get_type(struct file *fp){
	struct fs_inode *inode = (struct fs_inode *)fp->vinode;
	return inode->file_type;
}
int fs_set_flags(struct file *fp, int flags){
	struct fs_inode *inode = (struct fs_inode *)fp->vinode;
	inode->flags= inode->flags | flags;
	return JSUCCESS;
}
void* fs_get_stat_locked_pages(struct fs_inode *inodep){
	return &inodep->stat_locked_pages;
}
void fs_set_inode_used(void *inode_arg){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	atomic_inc(&inodep->count);
}
int fs_get_inode_flags(void *inode_arg){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	return inodep->flags;
}
void *fs_get_inode_vma_list(void *inode_arg){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	return &(inodep->vma_list);
}
void *fs_get_stat(void *inode_arg, int type){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	inodep->file_type = type;
	inodep->fileStat_insync = 1;
	return &(inodep->fileStat);
}
int fs_get_inode_type(void *inode_arg){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	return inodep->file_type;
}
unsigned char *fs_get_filename(void *inode){
	struct fs_inode *inodep = (struct fs_inode *)inode;
	return &(inodep->filename[0]);
}
void fs_set_private(void *inode_arg, int fid){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	inodep->fs_private = fid;
}
int fs_get_private(void *inode_arg){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	return inodep->fs_private;
}
uint64_t fs_get_size(void *inode_arg){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	return inodep->fileStat.st_size ;
}
int fs_set_size(void *inode_arg, unsigned long size){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	inodep->fileStat.st_size = size;
	return JSUCCESS;
}
int fs_set_offset(void *inode_arg, unsigned long offset){
	struct fs_inode *inodep = (struct fs_inode *)inode_arg;
	inodep->stat_last_offset = offset;
	return JSUCCESS;
}

}
