#include "../util/host_fs/filecache_schema.h"
#include "vfs.h"
#include "common.h"
#include "task.h"
struct filesystem host_fs;
static fileCache_t *filecachep=0;
struct wait_struct g_hfs_waitqueue;
unsigned char * pc_getPage();
static struct file *hfOpen(unsigned char *filename)
{
	struct file *filep;
	filep = kmem_cache_alloc(vfs_cachep, 0);
	if (filep == 0) return 0;
	if (filename != 0) 
	{	
		ut_strcpy(filep->filename,filename);
	}else
	{
		goto error;
	}	

	return filep;
error:
	kmem_cache_free(vfs_cachep, filep);	
	return 0;
}

static int hfRead(struct file *filep,unsigned char *buff, unsigned long len)
{
	int i,j,ret,tlen;
	unsigned long offset;
	unsigned char *p;

	ret=0;
	j=-1;
	if (filep ==0) return 0;
	for (i=0; i<filecachep->request_highindex; i++)	
	{
		if (filecachep->requests[i].state==STATE_INVALID)
		{
			j=i;
			break;
		}
	}
	if (j==-1)
	{
		j=filecachep->request_highindex;
		ut_printf(" clight high index %i \n",filecachep->request_highindex);
		filecachep->request_highindex++;
		if (j >=MAX_REQUESTS) 
		{
			ret=-1;
			goto error;
		}
		if (filecachep->requests[j].state!=STATE_INVALID)
		{
			ut_printf(" error in state  %x \n",filecachep->requests[j].state);
			ret=-2;
			goto error;
		}
	}
	ut_printf(" filename from hs  :%s: \n",filep->filename);
	ut_strcpy(filecachep->requests[j].filename,filep->filename);
	filecachep->requests[j].offset=offset;
	if (len > PC_PAGESIZE) 
		filecachep->requests[j].request_len=PC_PAGESIZE;
	else
		filecachep->requests[j].request_len=len;

	p=pc_getPage();	
	if (p == NULL)
	{
		ret=-3;
		goto error;
	}
	ut_printf(" Page address : %x \n",p);
	filecachep->requests[j].shm_offset=p-HOST_SHM_ADDR;
	filecachep->requests[j].response=RESPONSE_NONE;
	filecachep->requests[j].state=STATE_VALID;
	while(filecachep->requests[j].response==RESPONSE_NONE) 	
	{
		//ut_printf(" Before Wait : %d :\n",g_jiffies);
		sc_wait(&g_hfs_waitqueue,1000);
		ut_printf(" After Wait : %d :\n",g_jiffies);
	}
	if (filecachep->requests[j].response == RESPONSE_FAILED)
	{
		ut_printf(" error in response    \n");
		filecachep->requests[j].state=STATE_INVALID;
		ret=-4;
		goto error;
	}

	tlen=filecachep->requests[j].response_len;
	ut_printf(" Sucess in reading the file :%i %x \n",tlen,p);
	if (tlen > 0)
		ut_memcpy(buff,p,tlen);		
	filecachep->requests[j].state=STATE_INVALID;
	return tlen;

error:
	ut_printf(" Error in reading the file :%i \n",-ret);
	return ret;
}

static int hfClose(struct file *filep)
{

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

	filecachep=HOST_SHM_ADDR;
	if (filecachep->magic_number!=FS_MAGIC)	
	{
		ut_printf("ERROR : host_fs magic number does not match \n");
		return 0;	
	}
	if (filecachep->version!=FS_VERSION)	
	{
		ut_printf("ERROR : host_fs version number does not  match :%x \n",filecachep->version);
		return 0;	
	}
	if (filecachep->state !=STATE_VALID)	
	{
		ut_printf("ERROR : host_fs  cache not valid state \n");
		return 0;	
	}
	host_fs.open=hfOpen;
	host_fs.read=hfRead;
	host_fs.close=hfClose;
	fs_registerFileSystem(&host_fs);
	return 1;	
}
