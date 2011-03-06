#include "mm.h"
#include "vfs.h"
#include "list.h"
#include "../util/host_fs/filecache_schema.h"
enum {
        FRONT=0,
        TAIL=1
};
enum {
	FREE_LIST=1,
	ACTIVE_LIST=2,
	INACTIVE_LIST=3,
	DIRTY_LIST=4
};

typedef struct page_list{
        struct list_head head;
	unsigned char list_type;
        atomic_t count;
}page_list_t;


page_struct_t *pagecache_map;
unsigned char *pc_startaddr;
int pc_totalpages=0;
static page_list_t free_list,active_list,dirty_list,inactive_list;
/**
page life:  page moves from free->active->inactive->free

free->active : 
	1) ONLINE - pc_insertInodePage :
active->inactive : 
	1) HOUSEKEEP -  when the active exceeds upper limit
active,inactive->dirty : 
	1) ONLINE - pc_pageDirted : when page is made dirty
dirty -> active,inactive :
	1) ONLINE - pc_pageCleaned : After flushing the page
inactive->free :
	1) HOUSEKEEP - when free below the required target

**/

/*********************** local function *************************/
static int pagelist_add(page_struct_t *page, page_list_t *list,int tail);
static int page_init(page_struct_t *p)
{
	p->next=p->prev=NULL;
	p->count.counter=0;
	p->flags=0;
	p->inode=NULL;
	p->offset=0;
	p->list_type=0;
	INIT_LIST_HEAD(&(p->lru_list));
	INIT_LIST_HEAD(&(p->list));
	return 1;
}
static int init_pagelist(page_list_t *list)
{
	INIT_LIST_HEAD(&(list->head));
	list->count.counter=0;
	return 1;
}
static int pagelist_move(page_struct_t *page, page_list_t *list)
{ 
	if (page->list_type != 0)
	{
		list_del(&(page->lru_list));	
		switch(page->list_type)
		{
	        	case FREE_LIST: atomic_dec(&free_list.count); break;
        		case ACTIVE_LIST: atomic_dec(&active_list.count); break;
        		case INACTIVE_LIST: atomic_dec(&inactive_list.count); break;
        		case DIRTY_LIST: atomic_dec(&dirty_list.count); break;
		}
		page->list_type=0;
	}
	return pagelist_add(page,list,TAIL);
}
static int pagelist_add(page_struct_t *page, page_list_t *list,int tail)
{
	if (tail)
	{
		list_add_tail(&page->lru_list, &(list->head)); 
	}else
	{
		list_add(&page->lru_list, &(list->head)); 
	}	
	page->list_type=list->list_type;
	atomic_inc(&list->count);
	return 1;
}
static page_struct_t *pagelist_remove(page_list_t *list)
{
	page_struct_t *page;
	struct list_head *node;

        node=list->head.next;
	if (node == &list->head) return 0;

	list_del(node); /* delete the node from free list */
	page=list_entry(node, struct page, lru_list);	
	page->list_type=0;
	atomic_dec(&list->count);
	return page;
}
/***************************** API function **********************/

int pc_init(unsigned char *start_addr,unsigned long len)
{
	int total_pages,reserved_size;
	page_struct_t *p;
	int i;

	pc_startaddr=(unsigned char *)start_addr;
	total_pages=len/PC_PAGESIZE;
	reserved_size=sizeof(fileCache_t)+sizeof(page_struct_t)*total_pages;
	pagecache_map=(page_struct_t *)(start_addr + sizeof(fileCache_t));
	ut_memset(pagecache_map, 0, sizeof(page_struct_t)*total_pages);

	init_pagelist(&free_list);
	init_pagelist(&active_list);
	init_pagelist(&dirty_list);
	init_pagelist(&inactive_list);

	for( i=0; i<total_pages; i++)
	{
		p=pagecache_map+i;
		atomic_set(&p->count, 0);
		if ( i < (reserved_size/PC_PAGESIZE))
		{
			p->flags =  (1 << PG_reserved);
		}else
		{
			pagelist_add(p,&free_list,1);
		}

	}
	pc_totalpages=free_list.count.counter;
	ut_printf(" startaddr: %x totalpages:%d reserved size:%d \n",pc_startaddr,total_pages,reserved_size);	
	return 1;
}
void pc_stats()
{
	ut_printf(" Total Pages : %d \n",pc_totalpages);
	ut_printf(" Free List        : %d \n",free_list.count.counter);
	ut_printf(" Active clean List: %d \n",active_list.count.counter);
	ut_printf(" Dirty List       : %d \n",dirty_list.count.counter);
	ut_printf(" Inactive List    : %d \n",inactive_list.count.counter);
	return;
}
int pc_pageDirted(struct page *page) 
{
	int ret;

	ret=pagelist_move(page,&dirty_list);
	PageSetDirty(page);
	return 1;
}
int pc_pagecleaned(struct page *page) 
{
	int ret;

	ret=pagelist_move(page,&active_list);
	PageClearDirty(page);
	return 1;
}
struct page *pc_getInodePage(struct inode *inode,unsigned long offset)
{
	struct list_head *p;
	struct page *page;
	int i;
	unsigned long page_offset=(offset/PC_PAGESIZE)*PC_PAGESIZE ;

	i=0;
	list_for_each(p, &(inode->page_list)) {
		page=list_entry(p, struct page, list);
		i++;
		ut_printf("get page address: %x %d \n",page,i);
		if (page->offset == page_offset)
		{
			return page;
		}
	} 
	return NULL;

}
int pc_insertInodePage(struct inode *inode,struct page *page)
{
	struct list_head *p;
	struct page *tmp_page;
	int ret=0;

	if (page->offset > inode->length) return ret;
	list_for_each(p, &(inode->page_list))
	{
		tmp_page=list_entry(p, struct page, list);
		ut_printf("insert page address: %x \n",tmp_page);
		if (page->offset < tmp_page->offset)
		{
			inode->nrpages++;
			page->inode=inode;
			list_add(&page->list, &tmp_page->list); 
			pagelist_add(page,&active_list,TAIL); /* add to the tail of ACTIVE list */
			ret=1;
			goto last;
		} 
	} 
	if (ret == 0)
	{
		inode->nrpages++;
		list_add_tail(&page->list, &inode->page_list); 
		pagelist_add(page,&active_list,TAIL); /* add to the tail of ACTIVE list */
		ret=1;
	}
last:
	return ret;

}

int pc_putFreePage(struct page *page) 
{
	pagelist_add(page,&free_list,TAIL); /* add to the tail of list */

	return 1;
}

page_struct_t *pc_getFreePage()
{
	page_struct_t *p;

	p=pagelist_remove(&free_list);
	if (p)
		page_init(p);
	return p;
}

