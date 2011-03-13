/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   mm/paging.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "mm.h"
#include "vfs.h"
#include "list.h"
#include "../util/host_fs/filecache_schema.h"
#include "interface.h"

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
unsigned char *pc_endaddr;
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
static inline unsigned char *to_ptr(struct page *p) /* TODO remove me later the function is duplicated in fs/host_fs.c */
{
        unsigned char *addr;
        unsigned long pn;
        pn=p-pagecache_map;
        addr=pc_startaddr+pn*PC_PAGESIZE;
        return addr;
}
static inline struct page *to_page(unsigned char *addr) /* TODO remove me later the function is duplicated in fs/host_fs.c */
{
	struct page *p;
        unsigned long pn;
	pn=(addr-pc_startaddr)/PC_PAGESIZE;
	return pagecache_map+pn;
}
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
static int init_pagelist(page_list_t *list,unsigned char type)
{
	INIT_LIST_HEAD(&(list->head));
	list->count.counter=0;
	list->list_type=type;
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
	DEBUG(" List INCREASE :%d: type:%d: \n",list->count.counter,list->list_type); 
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
	DEBUG(" List DECREASE :%d: type:%d: \n",list->count.counter,list->list_type); 
	return page;
}
/***************************** API function **********************/

int pc_init(unsigned char *start_addr,unsigned long len)
{
	int total_pages,reserved_size;
	page_struct_t *p;
	int i;

	pc_startaddr=(unsigned char *)start_addr;
	pc_endaddr=(unsigned char *)start_addr+len;
	total_pages=len/PC_PAGESIZE;
	reserved_size=sizeof(fileCache_t)+sizeof(page_struct_t)*total_pages;
	pagecache_map=(page_struct_t *)(start_addr + sizeof(fileCache_t));
	ut_memset((unsigned char *)pagecache_map, 0, sizeof(page_struct_t)*total_pages);

	init_pagelist(&free_list,FREE_LIST);
	init_pagelist(&active_list,ACTIVE_LIST);
	init_pagelist(&dirty_list,DIRTY_LIST);
	init_pagelist(&inactive_list,INACTIVE_LIST);

	for( i=0; i<total_pages; i++)
	{
		p=pagecache_map+i;
		page_init(p);
		if ( i < (reserved_size/PC_PAGESIZE))
		{
			p->flags =  (1 << PG_reserved);
		}else
		{
			pagelist_add(p,&free_list,1);
		}

	}
	pc_totalpages=free_list.count.counter;
	DEBUG(" startaddr: %x totalpages:%d reserved size:%d \n",pc_startaddr,total_pages,reserved_size);	
	return 1;
}
int pc_stats(char *arg1,char *arg2)
{
	ut_printf(" Total Pages : %d \n",pc_totalpages);
	ut_printf(" Free List        : %d \n",free_list.count.counter);
	ut_printf(" Active clean List: %d \n",active_list.count.counter);
	ut_printf(" Dirty List       : %d \n",dirty_list.count.counter);
	ut_printf(" Inactive List    : %d \n",inactive_list.count.counter);
	return 1;
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
		if (i>200) return NULL ; /* TODO : remove me later ; just for debugging purpose */
		DEBUG(" %d: get page address: %x  addr:%x offset :%d \n",i,page,to_ptr(page),page->offset);
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
	int i=0;

	if (page->offset > inode->length) return ret;
	list_for_each(p, &(inode->page_list))
	{
		tmp_page=list_entry(p, struct page, list);
		i++;
		DEBUG("insert page address: %x %d \n",tmp_page,i);
		if (ret > 200 ) return 0; /* TODO : remove later */
		if (page->offset < tmp_page->offset)
		{
			inode->nrpages++;
			page->inode=inode;
			list_add_tail(&page->list, &tmp_page->list); /* add page before tmp_Page */
			pagelist_add(page,&active_list,TAIL); /* add to the tail of ACTIVE list */
			ret=1;
			goto last;
		} 
	} 
	if (ret == 0)
	{
		inode->nrpages++;
		DEBUG("insert page address at end : %x  \n",page);
		list_add_tail(&page->list, &(inode->page_list)); 
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

/***************************** House keeping functionality ******************/
static struct addr_list acc_list;
int scan_pagecache(char *arg1 , char *arg2)
{
	int i,ret;
	acc_list.total=0;
	ret=ar_scanPtes(pc_startaddr,pc_endaddr,&acc_list);
	DEBUG(" ScanPtes  ret:%x total:%d \n",ret,acc_list.total);
	for (i=0; i<acc_list.total; i++)
	{
		struct page *p;
	
		if (!(acc_list.addr[i] >= pc_startaddr && acc_list.addr[i] <= pc_endaddr ))
		{
			BUG();
		}	
		p=to_page(acc_list.addr[i]);	
		p->age=0;
		DEBUG("%d: page addr :%x \n",i,acc_list.addr[i]);
	}
}
