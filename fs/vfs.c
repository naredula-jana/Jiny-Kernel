/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/vfs.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
//#define DEBUG_ENABLE 1
#include "common.h"
#include "mm.h"
#include "vfs.h"
#include "interface.h"


static struct filesystem *vfs_fs = 0;
static kmem_cache_t *slab_inodep;
static LIST_HEAD(inode_list);
spinlock_t g_inode_lock = SPIN_LOCK_UNLOCKED; /* protects inode_list */
#define OFFSET_ALIGN(x) ((x/PC_PAGESIZE)*PC_PAGESIZE) /* the following 2 need to be removed */

kmem_cache_t *g_slab_filep;

static int inode_init(struct inode *inode, char *filename, struct filesystem *vfs) {
	unsigned long flags;

	if (inode == NULL)
		return 0;
	inode->count.counter = 0;
	inode->nrpages = 0;
	if (filename && filename[0] == 't') /* TODO : temporary solution need to replace with fadvise call */
	{
		inode->type = TYPE_SHORTLIVED;
	} else {
		inode->type = TYPE_LONGLIVED;
	}
	inode->file_size = -1;
	inode->fs_private = 0;
	inode->vfs = vfs;
	ut_strcpy(inode->filename, filename);
	INIT_LIST_HEAD(&(inode->page_list));
	INIT_LIST_HEAD(&(inode->inode_link));
	DEBUG(" inode init filename:%s: :%x  :%x \n",filename,&inode->page_list,&(inode->page_list));

	spin_lock_irqsave(&g_inode_lock, flags);
	list_add(&inode->inode_link, &inode_list);
	spin_unlock_irqrestore(&g_inode_lock, flags);

	return 1;
}

/*************************** API functions ************************/

int Jcmd_ls(char *arg1, char *arg2) {
	struct inode *tmp_inode;
	struct list_head *p;

	ut_printf(" Name usagecount nrpages length \n");
	list_for_each(p, &inode_list) {
		tmp_inode = list_entry(p, struct inode, inode_link);
		ut_printf("%s %d %d %d \n", tmp_inode->filename, tmp_inode->count,
				tmp_inode->nrpages, tmp_inode->file_size);
	}
	return 1;
}
struct inode *fs_getInode(char *filename) {
	struct inode *ret_inode;
	struct list_head *p;
	unsigned long flags;

	ret_inode = 0;

	spin_lock_irqsave(&g_inode_lock, flags);
	list_for_each(p, &inode_list) {
		ret_inode = list_entry(p, struct inode, inode_link);
		if (ut_strcmp(filename, ret_inode->filename) == 0) {
			atomic_inc(&ret_inode->count);
			spin_unlock_irqrestore(&g_inode_lock, flags);
			goto last;
		}
	}
	spin_unlock_irqrestore(&g_inode_lock, flags);

	ret_inode = kmem_cache_alloc(slab_inodep, 0);
	inode_init(ret_inode, filename, vfs_fs);

	last: return ret_inode;
}

unsigned long fs_putInode(struct inode *inode) {
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&g_inode_lock, flags);
	if (inode != 0 && inode->nrpages == 0 && inode->count.counter == 0) {
		list_del(&inode->inode_link);
		ret = 1;
	}
	spin_unlock_irqrestore(&g_inode_lock, flags);

	if (ret == 1)
		kmem_cache_free(slab_inodep, inode);

	return 0;
}

static struct file *vfsOpen(unsigned char *filename, int flags, int mode) {
	struct file *filep;
	struct inode *inodep;
	int ret;

	filep = kmem_cache_alloc(g_slab_filep, 0);
	if (filep == 0)
		goto error;
	if (filename != 0) {
		ut_strcpy(filep->filename, filename);
	} else {
		goto error;
	}

	inodep = fs_getInode(filep->filename);
	if (inodep == 0)
		goto error;
	if (inodep->fs_private == 0) /* need to get info from host  irrespective the file present, REQUEST_OPEN checks the file modification and invalidated the pages*/
	{
		ret = vfs_fs->open(inodep, flags, mode);
		if (ret < 0)
			goto error;
		inodep->file_size = ret;
	}
	filep->inode = inodep;
	filep->offset = 0;
	return filep;

error:
    if (filep != NULL)
		kmem_cache_free(g_slab_filep, filep);
	if (inodep != NULL) {
		fs_putInode(inodep);
	}
	return 0;
}
unsigned long fs_open(char *filename, int flags, int mode) {
	if (vfs_fs == 0 || filename==0)
		return 0;
	return vfsOpen(filename, flags, mode);
}

unsigned long SYS_fs_open(char *filename, int mode, int flags) {
	struct file *filep;
	int total;

	SYSCALL_DEBUG("open : filename :%s: mode:%x flags:%x \n",filename,mode,flags);
	if (ut_strcmp(filename,"/dev/tty") == 0){
		return 1;
	}
	filep = fs_open(filename, mode, flags);
	total = g_current_task->mm->fs.total;
	if (filep != 0 && total < MAX_FDS) {
		g_current_task->mm->fs.filep[total] = filep;
		g_current_task->mm->fs.total++;
		SYSCALL_DEBUG(" return value: %d \n",total);
		return total;
	} else {
		if (filep != 0)
			fs_close(filep);
	}
	return -1;
}

unsigned long SYS_fs_fdatasync(unsigned long fd) {
	struct file *file;

	SYSCALL_DEBUG("fdatasync fd:%d \n",fd);
	file = fd_to_file(fd);
	return fs_fdatasync(file);
}
static int vfsDataSync(struct file *filep)
{
	struct list_head *p;
	struct page *page;
	struct inode *inode;
	int ret;

	inode=filep->inode;
	list_for_each(p, &(inode->page_list)) {
		page=list_entry(p, struct page, list);
		if (PageDirty(page))
		{
			int len=inode->file_size;
			if (len < (page->offset+PC_PAGESIZE))
			{
				len=len-page->offset;
			}else
			{
				len=PC_PAGESIZE;
			}
			if (len > 0)
			{
				ret=vfs_fs->write(inode, page->offset, pcPageToPtr(page),len);
				if (ret == len)
				{
					pc_pagecleaned(page);
				}
			}
		}else
		{
			DEBUG(" Page cleaned :%x \n",page);
		}
	}
	return 0;
}
unsigned long fs_fdatasync(struct file *file) {
	if (vfs_fs == 0 || file == 0)
		return 0;
	return vfsDataSync(file);
}
unsigned long fs_lseek(struct file *file, unsigned long offset, int whence) {
	if (vfs_fs == 0)
		return 0;
	if (file == 0)
		return 0;
	return vfs_fs->lseek(file, offset, whence);
}

unsigned long SYS_fs_lseek(unsigned long fd, unsigned long offset, int whence) {
	struct file *file;

	SYSCALL_DEBUG("lseek fd:%d offset:%x whence:%x \n",fd,offset,whence);
	file = fd_to_file(fd);
	if (vfs_fs == 0)
		return 0;
	if (file == 0)
		return 0;
	return vfs_fs->lseek(file, offset, whence);
}

ssize_t SYS_fs_writev(int fd, const struct iovec *iov, int iovcnt) {
	int i;
	ssize_t ret, tret;
	struct file *file;

	SYSCALL_DEBUG("writev: fd:%d iovec:%x count:%d\n",fd,iov,iovcnt);

	file = fd_to_file(fd);
	ret = 0;
	for (i = 0; i < iovcnt; i++) {
		if (fd == 1 || fd ==2) { /* TODO: remove the fd==2 later , this is only for testing */
			ut_printf("write %s\n", iov[i].iov_base);/* TODO need to terminate the buf with \0  */
			ret= ret + iov[i].iov_len;
			continue;
		}
		tret = fs_write(file, iov[i].iov_base, iov[i].iov_len);
		if (tret > 0)
			ret = ret + tret;
		else
			goto last;
	}
	last: return ret;
}

ssize_t SYS_fs_readv(int fd, const struct iovec *iov, int iovcnt) {
	int i;
	ssize_t ret, tret;
	struct file *file;

	SYSCALL_DEBUG("readv: fd:%d iovec:%x count:%d\n",fd,iov,iovcnt);
	file = fd_to_file(fd);
	ret = 0;
	for (i = 0; i < iovcnt; i++) {
		tret = fs_read(file, iov[i].iov_base, iov[i].iov_len);
		if (tret > 0)
			ret = ret + tret;
		else
			goto last;
	}
	last: return ret;
}
static ssize_t vfswrite(struct file *filep, unsigned char *buff, unsigned long len) {
	int ret;
	int tmp_len, size, page_offset;
	struct page *page;

	ret = 0;
	if (filep == 0)
		return 0;
	DEBUG("Write  filename from hs  :%s: offset:%d inode:%x \n",filep->filename,filep->offset,filep->inode);
	tmp_len = 0;

	while (tmp_len < len) {
		page = pc_getInodePage(filep->inode, filep->offset);
		if (page == NULL) {
			page = pc_getFreePage();
			if (page == NULL) {
				ret = -3;
				goto error;
			}
			page->offset = OFFSET_ALIGN(filep->offset);
			if (pc_insertPage(filep->inode, page) == 0) {
				BUG();
			}
		}
		size = PC_PAGESIZE;
		if (size > (len - tmp_len))
			size = len - tmp_len;
		page_offset=filep->offset-page->offset;
		ut_memcpy(pcPageToPtr(page)+page_offset, buff + tmp_len, size);
		pc_pageDirted(page);
		filep->offset=filep->offset+size;
		struct inode *inode = filep->inode;
		if (inode->file_size < filep->offset)
			inode->file_size = filep->offset;
		tmp_len = tmp_len + size;
		DEBUG("write memcpy :%x %x  %d \n",buff,pcPageToPtr(page),size);
	}
	error:
	return tmp_len;
}
ssize_t SYS_fs_write(unsigned long fd, unsigned char *buff, unsigned long len) {
	struct file *file;
	int i;

	SYSCALL_DEBUG("write fd:%d buff:%x len:%x \n",fd,buff,len);
	if (fd == 1 || fd ==2) { /* TODO: remove the fd==2 later , this is only for testing */
		for (i=0; i<len; i++){
			ut_putchar(buff[i]);
		}
		//ut_printf("%s", buff);/* TODO need to terminate the buf with \0  */
		return len;
	}

	file = fd_to_file(fd);
	return fs_write(file, buff, len);
}
ssize_t fs_write(struct file *file, unsigned char *buff, unsigned long len) {
	if (vfs_fs == 0 || (vfs_fs->write==0))
		return 0;
	if (file == 0)
		return 0;
	return vfswrite(file, buff, len);
}

struct page *fs_genericRead(struct inode *inode, unsigned long offset) {
	struct page *page;
	int tret;
	int err = 0;

	page = pc_getInodePage(inode, offset);
	if (page == NULL) {
		page = pc_getFreePage();
		if (page == NULL) {
			err = -3;
			goto error;
		}
		page->offset = OFFSET_ALIGN(offset);

		tret = inode->vfs->read(inode, page->offset, pcPageToPtr(page), PC_PAGESIZE);

		if (tret > 0) {
			if (pc_insertPage(inode, page) == 0) {
				pc_putFreePage(page);
				err = -5;
				goto error;
			}
			if ((tret+offset) > inode->file_size) inode->file_size = offset+tret;
		} else {
			pc_putFreePage(page);
			err = -4;
			goto error;
		}
	}

	error: if (err < 0) {
		DEBUG(" Error in reading the file :%i \n",-err);
		page = 0;
	}
	return page;
}
static ssize_t vfsread(struct file *filep, unsigned char *buff, unsigned long len)
{
	int ret;
	struct page *page;
	struct inode *inode;

	ret = 0;
	if (filep == 0)
		return 0;
	DEBUG("Read filename from hs  :%s: offset:%d inode:%x buff:%x len:%x \n",filep->filename,filep->offset,filep->inode,buff,len);
	inode = filep->inode;
	//TODO 	if (inode->length <= filep->offset) return 0;

	page = fs_genericRead(filep->inode, filep->offset);
	if (page == 0)
		return 0;

	ret = PC_PAGESIZE;
	ret = ret - (filep->offset - OFFSET_ALIGN(filep->offset));
	if (ret > len)
		ret = len;
	if ((filep->offset + ret) > inode->file_size) {
		int r;
		r = inode->file_size - filep->offset;
		if (r < ret)
			ret = r;
	}
	if (page > 0 && ret > 0) {
		ut_memcpy(buff, pcPageToPtr(page) + (filep->offset
				-OFFSET_ALIGN(filep->offset)), ret);
		filep->offset = filep->offset + ret;
		DEBUG(" memcpy :%x %x  %d \n", buff, pcPageToPtr(page), ret);
	}
	return ret;
}
ssize_t fs_read(struct file *file, unsigned char *buff, unsigned long len) {
	if (vfs_fs == 0)
		return 0;
	if (file == 0)
		return 0;
	return vfsread(file, buff, len);
}

ssize_t SYS_fs_read(unsigned long fd, unsigned char *buff, unsigned long len) {
	struct file *file;

	SYSCALL_DEBUG("read fd:%d buff:%x len:%x \n",fd,buff,len);
	file = fd_to_file(fd);
	if ((file==0) || (vfs_fs == 0) || (vfs_fs->read==0))
		return 0;
	if (file == 0)
		return 0;
	return vfsread(file, buff, len);
}

static int vfsClose(struct file *filep)
{
    int ret;

	if (filep->inode != 0) {
		ret = vfs_fs->close(filep->inode);
		fs_putInode(filep->inode);
	}
	filep->inode=0;
	kmem_cache_free(g_slab_filep, filep);
	return 1;
}

int fs_close(struct file *file) {
	if (vfs_fs == 0)
		return 0;
	return vfsClose(file);
}

unsigned long SYS_fs_close(unsigned long fd) {
	struct file *file;

	SYSCALL_DEBUG("close fd:%d \n",fd);
	file = fd_to_file(fd);
	if (file == 0)
		return 0;
	return fs_close(file);
}
static int vfsRemove(struct file *filep)
{
    int ret;

	ret = vfs_fs->remove(filep->inode);
	if (ret == 1) {
		if (filep->inode != 0) fs_putInode(filep->inode);
		filep->inode=0;
		kmem_cache_free(g_slab_filep, filep);
	}
	return ret;
}

int fs_remove(struct file *file) {
	if (vfs_fs == 0)
		return 0;
	return vfsRemove(file);
}
static int vfsStat(struct file *filep, struct fileStat *stat)
{
    int ret;

	ret = vfs_fs->stat(filep->inode,stat);
	if (ret == 1) {
//TODO
	}
	return ret;
}

int fs_stat(struct file *file, struct fileStat *stat) {
	if (vfs_fs == 0)
		return 0;
	return vfsStat(file, stat);
}
unsigned long fs_fadvise(struct inode *inode, unsigned long offset,
		unsigned long len, int advise) {
	struct page *page;
	struct list_head *p;
	//TODO : implementing other advise types
	if (advise == POSIX_FADV_DONTNEED && len == 0) {
		while (1) /* delete all the pages in the inode */
		{
			p = inode->page_list.next;
			if (p == &inode->page_list)
				return 1;
			page = list_entry(p, struct page, list);
			/* TODO:  check the things like lock for page to delete */
			pc_removePage(page);
		}
	}
	return 0;
}

unsigned long SYS_fs_fadvise(unsigned long fd, unsigned long offset,
		unsigned long len, int advise) {
	struct file *file;
	struct inode *inode;

	SYSCALL_DEBUG("fadvise fd:%d offset:%d len:%d advise:%x \n",fd,offset,len,advise);
	file = fd_to_file(fd);
	if (file == 0 || file->inode == 0)
		return 0;
	inode = file->inode;
	return fs_fadvise(inode, offset, len, advise);
}

unsigned long fs_registerFileSystem(struct filesystem *fs) {
	vfs_fs = fs; // TODO : currently only one lowelevel filsystem is hardwired to vfs.
	return 1;
}

void init_vfs() {
	g_slab_filep = kmem_cache_create("file_struct", sizeof(struct file), 0, 0,
			NULL, NULL);
	slab_inodep = kmem_cache_create("inode_struct", sizeof(struct inode), 0, 0,
			NULL, NULL);
	//init_hostFs();
	p9_initFs();
	return;
}
