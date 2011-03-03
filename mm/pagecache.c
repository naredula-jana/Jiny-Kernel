#include "mm.h"
#include "vfs.h"
#include "list.h"
#include "../util/host_fs/filecache_schema.h"
page_struct_t *pagecache_map;
unsigned char *pc_startaddr;
typedef struct 
{
	page_struct_t *list; /* pointer to the first node of the doubly linked list */
	spinlock_t lock;
	int length;
}page_list_t;

static page_list_t free_list,active_list,inactive_list;


/*********************** local function *************************/
static int add_to_list(page_list_t *list ,page_struct_t *page)/* add to the front */
{
	page_struct_t *next;
	if (list->list == NULL) /* 1. list is empty */
	{
		page->prev=page->next=page;
		list->list=page;
	} else
	{
		next=list->list;
		page->next=next;
		page->prev=next->prev;
		next->prev=page; 
		list->list=page;
	}
	return 1;
}

static int remove_from_list(page_list_t *list ,page_struct_t *page) /* if page is empty then remove from the last node */
{
	page_struct_t *ret=NULL;
	page_struct_t *prev,*next;

	if (list->list == NULL) return 0; /* 1. list is empty */
	if (page ==0)
	{
		page=list->list;
		page=page->prev; /* remove from the last */
	}
	if (page->next == page->prev)  /* 2. list is having one node */
	{
		if (page != list->list) BUG();
		list->list=NULL;
		page->next=page->prev=NULL;
	}else 
	{ /* 3. list is having more then 1 node */
		prev=page->prev;
		next=page->next;
		prev->next=next;
		next->prev=prev;
		page->next=page->prev=NULL;
	}	
	ret=page;
last:
	return ret;
}
static int init_list(page_list_t *list)
{
	list->list=NULL;
	list->length=0;
}
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


	init_list(&free_list);
	init_list(&active_list);
	init_list(&inactive_list);

	for( i=0; i<total_pages; i++)
	{
		p=pagecache_map+i;
		atomic_set(&p->count, 0);
		if ( i < (reserved_size/PC_PAGESIZE))
		{
			p->flags =  (1 << PG_reserved);
		}else
		{
			add_to_list(&free_list,p);
			//			p->next=free_list;
			//			free_list=p;
		}

	}
	ut_printf(" startaddr: %x totalpages:%d reserved size:%d \n",pc_startaddr,total_pages,reserved_size);	
	return 1;
}
unsigned char *pc_getInodePage(struct inode *inode,unsigned long offset)
{
        struct list_head *p;
	struct page *page;
        unsigned long page_offset=(offset/PC_PAGESIZE)*PC_PAGESIZE ;

        list_for_each(p, &(inode->page_list)) {
                page=list_entry(p, struct page, list);
                if (page->offset == page_offset)
                {
                        return page;
                }
        } 
        return NULL;

}
unsigned char *pc_insertInodePage(struct inode *inode,struct page *page)
{
        struct list_head *p;
	struct page *tmp_page;

        list_for_each(p, &(inode->page_list))
	 {
                tmp_page=list_entry(p, struct page, list);
                if (page->offset < tmp_page->offset)
                {
                      list_add(&page->list, &tmp_page->list); 
                } 
        } 
        return NULL;

}
int pc_putFreePage(struct page *page) /* TODO : to implement */
{
}
unsigned char *pc_getFreePage()
{
	page_struct_t *p;
	int pn=10;
	unsigned char *addr=0;

	p=remove_from_list(&free_list,0);
	ut_printf("New Getpage p :%x pagecacmap:%x startaddr:%x pn:%x \n",p,pagecache_map,pc_startaddr,pn);	
	if (p != NULL)
	{
		pn=p-pagecache_map;
		addr=pc_startaddr+pn*PC_PAGESIZE;
	}
	ut_printf("New Getpage :%x %x\n",addr,pn);	
	return addr;
}

