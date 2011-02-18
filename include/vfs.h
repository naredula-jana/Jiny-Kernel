#ifndef __VFS_H__
#define __VFS_H__
#include "common.h"
#include "mm.h"
#define MAX_FILENAME 200
#define HOST_SHM_ADDR 0xd0000000
#define HOST_SHM_CTL_ADDR 0xd1000000
extern unsigned long g_hostShmLen;
extern kmem_cache_t *vfs_cachep;

struct file {	
	char filename[MAX_FILENAME];
        int type;
        addr_t *addr;
};
struct filesystem {
	struct file *(*open)(char *filename);
	int (*read)(struct file *file, unsigned long offset, unsigned long len,unsigned char *buff);
	int (*close)(struct file *file);
};

int fs_registerFileSystem( struct filesystem *fs);

#endif
