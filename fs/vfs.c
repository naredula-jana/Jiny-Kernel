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
#include "common.h"
#include "mm.h"
#include "vfs.h"
#include "interface.h"

static struct filesystem *vfs_fs=0;
static kmem_cache_t *slab_inodep;
static LIST_HEAD(inode_list);
static spinlock_t inode_lock  = SPIN_LOCK_UNLOCKED; /* protects inode_list */

kmem_cache_t *g_slab_filep;

static int inode_init(struct inode *inode,char *filename)
{
	unsigned long  flags;

	if (inode == NULL) return 0;
	inode->count.counter=0;
	inode->nrpages=0;
	if (filename && filename[0]=='t') /* TODO : temporary solution need to replace with fadvise call */
	{
		inode->type=TYPE_SHORTLIVED;
	}else
	{
		inode->type=TYPE_LONGLIVED;
	}	
	inode->length=-1;
	ut_strcpy(inode->filename,filename);
	INIT_LIST_HEAD(&(inode->page_list));
	INIT_LIST_HEAD(&(inode->inode_link));
	DEBUG(" inode init filename:%s: :%x  :%x \n",filename,&inode->page_list,&(inode->page_list));

	spin_lock_irqsave(&inode_lock, flags);
        list_add(&inode->inode_link,&inode_list);	
	spin_unlock_irqrestore(&inode_lock, flags);

	return 1;
}

/*************************** API functions ************************/

int fs_printInodes(char *arg1,char *arg2)
{
        struct inode *tmp_inode;
        struct list_head *p;
	
	ut_printf(" Name usagecount nrpages length \n");
        list_for_each(p, &inode_list) {
                tmp_inode=list_entry(p, struct inode, inode_link);
		ut_printf("%s %d %d %d \n",tmp_inode->filename,tmp_inode->count,tmp_inode->nrpages,tmp_inode->length);
        }
	return 1;
}
struct inode *fs_getInode(char *filename)
{
	struct inode *ret_inode;
	struct list_head *p;
	unsigned long  flags;

	ret_inode=0;

	spin_lock_irqsave(&inode_lock, flags);
	list_for_each(p, &inode_list) {
		ret_inode=list_entry(p, struct inode, inode_link);
		if (ut_strcmp(filename,ret_inode->filename) == 0)
		{
			atomic_inc(&ret_inode->count);	
			spin_unlock_irqrestore(&inode_lock, flags);
			goto last;
		}
	}
	spin_unlock_irqrestore(&inode_lock, flags);

	ret_inode=kmem_cache_alloc(slab_inodep, 0);	
	inode_init(ret_inode,filename);

last:
	return ret_inode;	
}

int fs_putInode(struct inode *inode)
{
	unsigned long  flags;
	int ret=0;

	spin_lock_irqsave(&inode_lock, flags);
	if (inode!= 0 && inode->nrpages==0 && inode->count.counter==0)
	{
		list_del(&inode->inode_link);
		ret=1;
	}
	spin_unlock_irqrestore(&inode_lock, flags);

	if ( ret==1 )	kmem_cache_free(slab_inodep,inode);	

	return 0;
}
struct file *fs_open(char *filename,int mode)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->open(filename,mode);
}
int fs_fdatasync(struct file *file )
{
	if (vfs_fs == 0) return 0;
	if (file == 0) return 0;
	return vfs_fs->fdatasync(file);
}
int fs_lseek(struct file *file ,unsigned long offset, int whence)
{
        if (vfs_fs == 0) return 0;
	if (file == 0) return 0;
        return vfs_fs->lseek(file,offset,whence);
}
int fs_write(struct file *file ,unsigned char *buff ,unsigned long len)
{
	if (vfs_fs == 0) return 0;
	if (file == 0) return 0;
	return vfs_fs->write(file,buff,len);
}
int fs_read(struct file *file ,unsigned char *buff ,unsigned long len)
{
	if (vfs_fs == 0) return 0;
	if (file == 0) return 0;
	return vfs_fs->read(file,buff,len);
}
int fs_close(struct file *file)
{
	if (vfs_fs == 0) return 0;
	if (file == 0) return 0;
	return vfs_fs->close(file);
}
int fs_advise(struct file *file,unsigned long offset, unsigned long len,int advise)
{
	struct page *page;
	struct inode *inode;
        struct list_head *p;

	if (file == 0 || file->inode ==0) return 0;
	inode=file->inode;
	if (advise == POSIX_FADV_DONTNEED && len==0)
	{
		while(1) /* delete all the pages in the inode */
		{
			p=inode->page_list.next;
			if (p==&inode->page_list) return 1;
			page=list_entry(p, struct page, list);
			/* TODO:  check the things like lock for page to delete */
			pc_removePage(page);
		}

	}
}
int fs_registerFileSystem( struct filesystem *fs)
{
	vfs_fs=fs;
	return 1;
}
void init_vfs()
{
	g_slab_filep=kmem_cache_create("file_struct",sizeof(struct file), 0,0, NULL, NULL);
	slab_inodep=kmem_cache_create("inode_struct",sizeof(struct inode), 0,0, NULL, NULL);
	init_hostFs();
}
