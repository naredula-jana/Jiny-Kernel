#include "../util/host_fs/filecache_schema.h"
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
#define OFFSET_ALIGN(x) ((x/PC_PAGESIZE)*PC_PAGESIZE)
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

int request_hostserver(unsigned char type,struct inode *inode, struct page *page,int data_len,int mode)
{
	int i,j,ret,tlen;
	unsigned long offset;
	int wait_time=10;

	j=-1;
	ret=0;
	for (i=0; i<shm_headerp->request_highindex; i++)
	{
		if (shm_headerp->requests[i].state==STATE_INVALID)
		{
			j=i;
			break;
		}
	}
	DEBUG(" request high index %i \n",shm_headerp->request_highindex);
	if (j==-1)
	{
		j=shm_headerp->request_highindex;
		DEBUG(" client high index %i \n",shm_headerp->request_highindex);
		shm_headerp->request_highindex++;
		if (j >=MAX_REQUESTS)
		{
			ret=-1;
			goto error;
		}
		if (shm_headerp->requests[j].state!=STATE_INVALID)
		{
			DEBUG(" error in state  %x \n",shm_headerp->requests[j].state);
			ret=-2;
			goto error;
		}
	}

	ut_strcpy(shm_headerp->requests[j].filename,inode->filename);
	shm_headerp->requests[j].type=type;
	if (page != NULL)
	{
		shm_headerp->requests[j].file_offset=page->offset;
		shm_headerp->requests[j].shm_offset=(unsigned char *)to_ptr(page)-(unsigned char *)HOST_SHM_ADDR;
		DEBUG(" shm offset:%x page address :%x \n",shm_headerp->requests[j].shm_offset,to_ptr(page));
	}
	else 
	{
		shm_headerp->requests[j].file_offset=0;
		shm_headerp->requests[j].shm_offset=-1;
	}
	shm_headerp->requests[j].request_len=data_len;
	shm_headerp->requests[j].response=RESPONSE_NONE;
	shm_headerp->requests[j].flags=mode;
	shm_headerp->requests[j].state=STATE_VALID;
	shm_headerp->generate_interrupt=1;

	while(shm_headerp->requests[j].response==RESPONSE_NONE)
	{
		sc_wait(&g_hfs_waitqueue,wait_time);
		if (wait_time < 1000) wait_time=wait_time*2;	
		DEBUG(" After Wait : %d :\n",g_jiffies);
	}

	if (shm_headerp->requests[j].response == RESPONSE_FAILED)
	{
		DEBUG(" error in response    \n");
		shm_headerp->requests[j].state=STATE_INVALID;
		ret=-9;
		goto error;
	}

	ret=shm_headerp->requests[j].response_len;
	DEBUG(" Sucess in reading the file :%i %x \n",tlen,page);
	shm_headerp->requests[j].state=STATE_INVALID;
error:
	return ret;
}

static struct file *hfOpen(unsigned char *filename,int mode)
{
	struct file *filep;
	struct inode *inodep;
	int ret;

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
	if (inodep->length == -1 ) /* need to get info from host */
	{
		ret=request_hostserver(REQUEST_OPEN,inodep,0,0,mode);
		if (ret < 0) goto error;
		inodep->length=ret;
	}
	filep->inode=inodep;
	filep->offset=0;
	inodep->count++;
	return filep;
error:
	if (filep != NULL)  kmem_cache_free(g_slab_filep, filep);
	if (inodep != NULL)
	{ /* TODO */
	}
	return 0;
}

static int hfLseek(struct file *filep, unsigned long offset,int whence)
{ /* TODO */


}

static int hfFdatasync(struct file *filep)
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
			int len=inode->length;
			if (len < (page->offset+PC_PAGESIZE))
			{
				len=len-page->offset;
			}else
			{
				len=PC_PAGESIZE;
			}
			if (len > 0) 
			{
				ret=request_hostserver(REQUEST_WRITE,inode,page,len,0);
				if (ret > 0)
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
static int hfWrite(struct file *filep,unsigned char *buff, unsigned long len)
{ 
	int ret;
	int tmp_len,size;
	struct page *page;

	ret=0;
	if (filep ==0) return 0;
	DEBUG("Write  filename from hs  :%s: offset:%d inode:%x \n",filep->filename,filep->offset,filep->inode);
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
			page->offset=OFFSET_ALIGN(filep->offset);
			pc_insertInodePage(filep->inode,page);
		}
		size=PC_PAGESIZE;
		if (size > (len-tmp_len)) size=len-tmp_len;
		ut_memcpy(to_ptr(page),buff+tmp_len,size);
		pc_pageDirted(page);
		tmp_len=tmp_len+size;
		DEBUG("write memcpy :%x %x  %d \n",buff,to_ptr(page),size);
	}
error:
	if (tmp_len > 0){
		struct inode *inode=filep->inode;
		filep->offset=filep->offset+tmp_len;
		if (inode->length < filep->offset)
			inode->length=filep->offset;	
	}
	return tmp_len;
}

static int hfRead(struct file *filep,unsigned char *buff, unsigned long len)
{
	int ret;
	struct page *page;
	struct inode *inode;

	ret=0;
	if (filep ==0) return 0;
	DEBUG("Read filename from hs  :%s: offset:%d inode:%x \n",filep->filename,filep->offset,filep->inode);
	inode=filep->inode;
	if (inode->length <= filep->offset) return 0;
	page=pc_getInodePage(filep->inode,filep->offset);
	if (page == NULL)
	{
		page=pc_getFreePage();	
		if (page == NULL)
		{
			ret=-3;
			goto error;
		}
		page->offset=OFFSET_ALIGN(filep->offset);
		DEBUG("New  Page address : %x \n",page);

		ret=request_hostserver(REQUEST_READ,filep->inode,page,PC_PAGESIZE,0);
		if (ret > 0)
		{
			if (pc_insertInodePage(filep->inode,page) ==0)
			{
				pc_putFreePage(page);
				ret=-5;
				goto error;
			}
		}else
		{
			pc_putFreePage(page);
			ret=-4;
			goto error;
		}
	}else
	{
		ret=PC_PAGESIZE;
	}
	if ((filep->offset+ret) > inode->length)
	{
		ret=inode->length - filep->offset;
	}
	if (page > 0  && ret > 0)
	{
		filep->offset=filep->offset+ret;
		ut_memcpy(buff,to_ptr(page),ret);
		DEBUG(" memcpy :%x %x  %d \n",buff,to_ptr(page),ret);
	}

	return ret;

error:
	DEBUG(" Error in reading the file :%i \n",-ret);
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
		DEBUG(" ERROR : host_shm not Initialized \n");
		return 0;
	}

	shm_headerp=(fileCache_t *)HOST_SHM_ADDR;
	if (shm_headerp->magic_number!=FS_MAGIC)	
	{
		DEBUG("ERROR : host_fs magic number does not match \n");
		return 0;	
	}
	if (shm_headerp->version!=FS_VERSION)	
	{
		DEBUG("ERROR : host_fs version number does not  match :%x \n",shm_headerp->version);
		return 0;	
	}
	if (shm_headerp->state !=STATE_VALID)	
	{
		DEBUG("ERROR : host_fs  cache not valid state \n");
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
