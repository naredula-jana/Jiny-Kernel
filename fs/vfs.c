#include "common.h"
#include "mm.h"
#include "vfs.h"
static struct filesystem *vfs_fs=0;

kmem_cache_t *g_slab_filep;
kmem_cache_t *g_slab_inodep;
LIST_HEAD(inode_list);

static int inode_init(struct inode *inode,unsigned char *filename)
{
	if (inode == NULL) return 0;
	inode->count=0;
	inode->nrpages=0;
	inode->length=0;
	ut_strcpy(inode->filename,filename);
	INIT_LIST_HEAD(&(inode->page_list));
	INIT_LIST_HEAD(&(inode->inode_next));
	ut_printf(" inode init :%x  :%x \n",&inode->page_list,&(inode->page_list));
        list_add(&inode->inode_next,&inode_list);	
	return 1;
}

/*************************** API functions ************************/

int fs_printInodes()
{
        struct inode *tmp_inode;
        struct list_head *p;

        list_for_each(p, &inode_list) {
                tmp_inode=list_entry(p, struct inode, inode_next);
		ut_printf(" name: %s count:%d nrpages:%d length:%d \n",tmp_inode->filename,tmp_inode->count,tmp_inode->nrpages,tmp_inode->length);
        }
}
struct inode *fs_getInode(unsigned char *filename)
{
	struct inode *tmp_inode;
	struct list_head *p;

	list_for_each(p, &inode_list) {
		tmp_inode=list_entry(p, struct inode, inode_next);
		if (ut_strcmp(filename,tmp_inode->filename) == 0)
		{
			return tmp_inode;
		}
	}

	tmp_inode=kmem_cache_alloc(g_slab_inodep, 0);	
	inode_init(tmp_inode,filename);

	return tmp_inode;	
}

int kernel_read(struct file *file, unsigned long offset,
        char *addr, unsigned long count)/* TODO : need to rework the function  */
{
	return 1;
}

struct file *fs_open(unsigned char *filename,int mode)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->open(filename,mode);
}
struct file *fs_fdatasync(unsigned char *filename)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->fdatasync(filename);
}
int fs_lseek(struct file *file ,unsigned long offset, int whence)
{
        if (vfs_fs == 0) return 0;
        return vfs_fs->lseek(file,offset,whence);
}
int fs_write(struct file *file ,unsigned char *buff ,unsigned long len)
{
	if (vfs_fs == 0) return 0;
	return vfs_fs->write(file,buff,len);
}
int fs_read(struct file *file ,unsigned char *buff ,unsigned long len)
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
	g_slab_filep=kmem_cache_create("file_struct",sizeof(struct file), 0,0, NULL, NULL);
	g_slab_inodep=kmem_cache_create("inode_struct",sizeof(struct inode), 0,0, NULL, NULL);
	init_hostFs();
}
