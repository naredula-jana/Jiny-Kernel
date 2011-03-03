#ifndef __VFS_H__
#define __VFS_H__
#include "common.h"
#include "mm.h"
#define MAX_FILENAME 200
#define HOST_SHM_ADDR 0xd0000000
#define HOST_SHM_CTL_ADDR 0xd1000000
extern unsigned long g_hostShmLen;
extern kmem_cache_t *g_slab_inodep;
extern kmem_cache_t *g_slab_filep;

struct file {	
	char filename[MAX_FILENAME];
        int type;
	unsigned long offset;
        addr_t *addr;
	struct inode *inode;
};

struct inode {
	int count; /* usage count */
	int nrpages;	
	char filename[MAX_FILENAME];
	struct list_head page_list;	
	struct list_head inode_next;	
};

struct filesystem {
	struct file *(*open)(char *filename);
	int (*read)(struct file *file,  unsigned char *buff,unsigned long len);
	int (*close)(struct file *file);
};

int fs_registerFileSystem( struct filesystem *fs);

#endif
