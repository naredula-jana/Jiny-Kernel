#include "mm.h"
#include "../util/host_fs/filecache_schema.h"
static page_struct_t *pagecache_map;
static unsigned char *pc_startaddr;
static page_struct_t *free_list=NULL;

/*********************** local function *************************/


/***************************** API function **********************/

int pc_init(addr_t start_addr,unsigned long len)
{
	int total_pages,reserved_size;
	page_struct_t *p;
	int i;
	
	pc_startaddr=start_addr;
	total_pages=len/PC_PAGESIZE;
	reserved_size=sizeof(fileCache_t)+sizeof(page_struct_t)*total_pages;
	pagecache_map=start_addr + (unsigned char *)sizeof(fileCache_t);
	ut_memset(pagecache_map, 0, sizeof(page_struct_t)*total_pages);

        for( i=0; i<total_pages; i++)
	{
		p=pagecache_map+i;
                atomic_set(&p->count, 0);
		if ( i < (reserved_size/PC_PAGESIZE))
		{
                	p->flags =  (1 << PG_reserved);
		}else
		{
			p->next=free_list;
			free_list=p;
		}
        }
 ut_printf(" startaddr: %x totalpages:%d reserved size:%d \n",pc_startaddr,total_pages,reserved_size);	
	return 1;
}

unsigned char * pc_getPage()
{
	page_struct_t *p;
	int pn;
	unsigned char *addr=0;

	if (free_list != NULL)
	{
		p=free_list;
		free_list=p->next;
		pn=p-pagecache_map;
		addr=pc_startaddr+pn*PC_PAGESIZE;
	}
        ut_printf("New Getpage :%x \n",addr);	
	return addr;
}

