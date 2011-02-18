#include "../util/host_fs/filecache_schema.h"
#include "vfs.h"
struct filesystem host_fs;
static fileCache_t *filecachep=0;


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

static int hfRead(struct file *filep, unsigned long offset, unsigned long len,unsigned char *buff )
{
	int i,j,ret,tlen;

	ret=0;
	j=-1;
	if (filep ==0) return 0;
	for (i=0; i<filecachep->client_highindex; i++)	
	{
		if (filecachep->clientRequests[i].state==STATE_INVALID)
		{
			j=i;
			break;
		}
	}
	if (j==-1)
	{
		j=filecachep->client_highindex;
ut_printf(" clight high index %i \n",filecachep->client_highindex);
		filecachep->client_highindex++;
		if (j >=MAX_REQUESTS) 
		{
			ret=-1;
			goto error;
		}
		if (filecachep->clientRequests[j].state!=STATE_INVALID)
		{
ut_printf(" error in state  %x \n",filecachep->clientRequests[j].state);
			ret=-2;
			goto error;
		}
	}
ut_printf(" filename from hs  :%s: \n",filep->filename);
	ut_strcpy(filecachep->clientRequests[j].filename,filep->filename);
	filecachep->clientRequests[j].offset=offset;
	filecachep->clientRequests[j].len=len;
	filecachep->clientRequests[j].server_response=RESPONSE_NONE;
	filecachep->clientRequests[j].state=STATE_VALID;
	while(filecachep->clientRequests[j].server_response==RESPONSE_NONE) ;	
	if (filecachep->clientRequests[j].server_response == RESPONSE_FAILED)
	{
ut_printf(" error in response    \n");
		filecachep->clientRequests[j].state=STATE_INVALID;
		ret=-3;
		goto error;
	}

	filecachep->clientRequests[j].state=STATE_INVALID;
	for (i=0; i<filecachep->server_highindex; i++)	
	{
		if ((filecachep->serverFiles[i].state==STATE_VALID) && (ut_strcmp(filecachep->serverFiles[i].filename,filep->filename)==0))
		{
		if (filecachep->serverFiles[i].len < len) tlen=filecachep->serverFiles[i].len;
		else tlen=len;
		if (tlen < 1) 
		{
			ret=-4;
			goto error;
		}
	ut_printf(" Sucess in reading the file :%i \n",tlen);
		ut_memcpy(buff,filecachep->serverFiles[i].filePtr,tlen);		
		return tlen;
		}
	}
error:
	ut_printf(" Error in reading the file :%i \n",ret);
	return ret;
}

static int hfClose(struct file *filep)
{

}
int init_hostFs()
{
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
