#ifndef __VFS_H__
#define __VFS_H__
#include "common.h"
#include "mm.h"
#define MAX_FILENAME 200
#define HOST_SHM_ADDR 0xd0000000
#define HOST_SHM_CTL_ADDR 0xd1000000

enum {
 O_CREATE=1
};

enum {
POSIX_FADV_DONTNEED=1
};

enum {
TYPE_SHORTLIVED=1,
TYPE_LONGLIVED=2
};
extern unsigned long g_hostShmLen;
extern kmem_cache_t *g_slab_inodep;
extern kmem_cache_t *g_slab_filep;

struct filesystem {
	struct file *(*open)(char *filename,int mode);
	int (*lseek)(struct file *file,  unsigned long offset,int whence);
	ssize_t (*write)(struct inode *inode, uint64_t offset, unsigned char *buff,unsigned long len);
	ssize_t (*read)(struct inode *inode, uint64_t offset,  unsigned char *buff,unsigned long len);
	int (*close)(struct file *file);
	int (*fdatasync)(struct file *file);
};

struct file {	
	char filename[MAX_FILENAME];
        int type;
	unsigned long offset;
        addr_t *addr;
	struct inode *inode;
};

struct inode {
	atomic_t count; /* usage count */
	int nrpages;	
	int type; /* short leaved (MRU) or long leaved (LRU) */
	time_t mtime; /* last modified time */
	unsigned long fs_private;
	struct filesystem *vfs;
	unsigned long file_size; /* file length */
	char filename[MAX_FILENAME];
	struct list_head page_list;	
	struct list_head vma_list;	
	struct list_head inode_link;	
};



#endif
