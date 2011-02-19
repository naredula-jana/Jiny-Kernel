#include "common.h"
#include "mm.h"
#include "vfs.h"
struct filesystem *vfs_fs=0;
kmem_cache_t *vfs_cachep;
int kernel_read(struct file *file, unsigned long offset,
        char * addr, unsigned long count)
{
	return 1;
}

struct file *fs_open(unsigned char *filename)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->open(filename);
}
int fs_read(struct file *file ,sunsigned char *buff ,unsigned long len)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->read(file,buff,len);
}
int fs_close(struct file *file)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->close(file);
}
int fs_registerFileSystem( struct filesystem *fs)
{
	vfs_fs=fs;
}
void init_vfs()
{
	vfs_cachep=kmem_cache_create("file_struct",sizeof(struct file), 0,0, NULL, NULL);
	init_hostFs();
}
