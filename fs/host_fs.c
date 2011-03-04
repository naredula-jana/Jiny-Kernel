#include "../util/host_fs/filecache_schema.h"
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
struct filesystem host_fs;
static fileCache_t *shm_headerp=0;
struct wait_struct g_hfs_waitqueue;

static inline unsigned char *to_ptr(struct page *p)  
{
   unsigned char *addr;
   unsigned long pn;
   pn=p-pagecache_map; 
   addr=pc_startaddr+pn*PC_PAGESIZE; 
   return addr;
 }
static struct file *hfOpen(unsigned char *filename)
{
	struct file *filep;
	struct inode *inodep;

	filep = kmem_cache_alloc(g_slab_filep, 0);
	if (filep == 0) goto error;
	if (filename != 0) 
	{	
		ut_strcpy(filep->filename,filename);
	}else
	{
		goto error;
	}	
	inodep = fs_getInode(filep->filename);
	if (inodep == 0) goto error;
	filep->inode=inodep;
	filep->offset=0;
	inodep->count++;
	return filep;
error:
	if (filep != NULL)  kmem_cache_free(g_slab_filep, filep);	
	if (inodep != NULL) 
	{
	}
	return 0;
}
int request_hostserver(unsigned char type,struct file *filep, struct page *page,int data_len)
{
	int i,j,ret,tlen;
	unsigned long offset;

	j=-1;
	for (i=0; i<shm_headerp->request_highindex; i++)
	{
		if (shm_headerp->requests[i].state==STATE_INVALID)
		{
			j=i;
			break;
		}
	}
	ut_printf(" request high index %i \n",shm_headerp->request_highindex);
	if (j==-1)
	{
		j=shm_headerp->request_highindex;
		ut_printf(" client high index %i \n",shm_headerp->request_highindex);
		shm_headerp->request_highindex++;
		if (j >=MAX_REQUESTS)
		{
			ret=-1;
			goto error;
		}
		if (shm_headerp->requests[j].state!=STATE_INVALID)
		{
			ut_printf(" error in state  %x \n",shm_headerp->requests[j].state);
			ret=-2;
			goto error;
		}
	}

	ut_strcpy(shm_headerp->requests[j].filename,filep->filename);
	shm_headerp->requests[j].type=type;
	shm_headerp->requests[j].file_offset=filep->offset;
	shm_headerp->requests[j].request_len=data_len;

	shm_headerp->requests[j].shm_offset=to_ptr(page)-HOST_SHM_ADDR;
	shm_headerp->requests[j].response=RESPONSE_NONE;
	shm_headerp->requests[j].state=STATE_VALID;
	shm_headerp->generate_interrupt=1;

	while(shm_headerp->requests[j].response==RESPONSE_NONE)
	{
		sc_wait(&g_hfs_waitqueue,1000);
		ut_printf(" After Wait : %d :\n",g_jiffies);
	}

	if (shm_headerp->requests[j].response == RESPONSE_FAILED)
	{
		ut_printf(" error in response    \n");
		shm_headerp->requests[j].state=STATE_INVALID;
		tlen=-9;
		goto error;
	}

	tlen=shm_headerp->requests[j].response_len;
	ut_printf(" Sucess in reading the file :%i %x \n",tlen,page);
	shm_headerp->requests[j].state=STATE_INVALID;
error:
	return tlen;
}

static int hfLseek(struct file *filep, unsigned long offset,int whence)
{ /* TODO */

}

static int hfFdatasync(struct file *filep)
{ /* TODO */

}
static int hfWrite(struct file *filep,unsigned char *buff, unsigned long len)
{ 
        int ret;
	int tmp_len,size;
        struct page *page;

	ret=0;
	if (filep ==0) return 0;
	ut_printf("Write  filename from hs  :%s: offset:%d \n",filep->filename,filep->offset);

	tmp_len=0;

	while(tmp_len < len)
	{
		page=pc_getInodePage(filep->inode,filep->offset);
		if (page == NULL)
		{
			page=pc_getFreePage();
			if (page == NULL)
			{
				ret=-3;
				goto error;
			}
			page->offset=filep->offset;
			pc_insertInodePage(filep->inode,page);
		}
		size=PC_PAGESIZE;
		if (size > (len-tmp_len)) size=len-tmp_len;
		ut_memcpy(to_ptr(page),buff+tmp_len,size);
		tmp_len=tmp_len+size;
		ut_printf("write memcpy :%x %x  %d \n",buff,to_ptr(page),size);
	}

	return tmp_len;
}

static int hfRead(struct file *filep,unsigned char *buff, unsigned long len)
{
	int ret;
	struct page *page;

	ret=0;
	if (filep ==0) return 0;
	ut_printf("Read filename from hs  :%s: offset:%d \n",filep->filename,filep->offset);
	page=pc_getInodePage(filep->inode,filep->offset);
	if (page == NULL)
	{
		page=pc_getFreePage();	
		if (page == NULL)
		{
			ret=-3;
			goto error;
		}
		page->offset=filep->offset;
		ut_printf("New  Page address : %x \n",page);

		ret=request_hostserver(REQUEST_READ,filep,page,PC_PAGESIZE);
		if (ret > 0)
		{
			filep->offset=filep->offset+ret;
			pc_insertInodePage(filep->inode,page);
		}else
		{
			pc_putFreePage(page);
			ret=-4;
			goto error;
		}
	}else
	{
		ret=PC_PAGESIZE;
		filep->offset=filep->offset+ret;
	}
	if (page > 0  && ret > 0)
	{
		ut_memcpy(buff,to_ptr(page),ret);
		ut_printf(" memcpy :%x %x  %d \n",buff,to_ptr(page),ret);
	}

	return ret;

error:
	ut_printf(" Error in reading the file :%i \n",-ret);
	return ret;
}

static int hfClose(struct file *filep)
{
	struct inode *tmp_inode;

	tmp_inode=filep->inode;
	if (tmp_inode != 0) tmp_inode->count--;
	filep->inode=0;
	kmem_cache_free(g_slab_filep, filep);	
	return 1;	
}

int init_hostFs()
{
	g_hfs_waitqueue.queue=NULL;
	g_hfs_waitqueue.lock=SPIN_LOCK_UNLOCKED;

	if (g_hostShmLen ==0)
	{
		ut_printf(" ERROR : host_shm not Initialized \n");
		return 0;
	}

	shm_headerp=HOST_SHM_ADDR;
	if (shm_headerp->magic_number!=FS_MAGIC)	
	{
		ut_printf("ERROR : host_fs magic number does not match \n");
		return 0;	
	}
	if (shm_headerp->version!=FS_VERSION)	
	{
		ut_printf("ERROR : host_fs version number does not  match :%x \n",shm_headerp->version);
		return 0;	
	}
	if (shm_headerp->state !=STATE_VALID)	
	{
		ut_printf("ERROR : host_fs  cache not valid state \n");
		return 0;	
	}
	host_fs.open=hfOpen;
	host_fs.read=hfRead;
	host_fs.close=hfClose;
	host_fs.write=hfWrite;
	host_fs.fdatasync=hfFdatasync;
	host_fs.lseek=hfLseek;
	fs_registerFileSystem(&host_fs);
	return 1;	
}
