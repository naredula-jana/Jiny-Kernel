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
enum {
	AGE_YOUNGEST=1,
	AGE_ELDEST=100
};

typedef struct page_list{
        struct list_head head;
	unsigned char list_type;
        atomic_t count;
}page_list_t;

extern unsigned long g_hostShmPhyAddr;
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
        unsigned long pn;
	pn=(addr-pc_startaddr)/PC_PAGESIZE;
	return pagecache_map+pn;
}
static int page_init(page_struct_t *p)
{
	p->next=p->prev=NULL;
	p->count.counter=0;
	p->flags=0;
	p->age=0;
	p->inode=NULL;
	p->offset=0;
	p->list_type=0;

	p->lru_list.prev=p->lru_list.next=0;
	p->list.prev=p->list.next=0;
/*	INIT_LIST_HEAD(&(p->lru_list));
	INIT_LIST_HEAD(&(p->list)); */
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
	if (list != 0)
		return pagelist_add(page,list,TAIL);
	else
		return 0;
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
/*  Remove page from page cache */
int put_into_freelist(struct page *page)
{
	struct inode *inode;

	if (page->lru_list.next!= 0 || page->lru_list.prev!= 0)
	{
		pagelist_move(page,0); /* remove page from any list present */
	}
	if (page->list.next==0 || page->list.prev==0)
	{
		BUG();
	}
	list_del(&(page->list));
	inode=page->inode;
	if (inode == 0) BUG();
	inode->nrpages--;
	page->inode=0;
	pc_putFreePage(page);
	return 1;
}
static int reclaim_freepages()
{
	struct page *page;
	struct inode *inode;

	DEBUG(" In the RECLAIN FRee pages \n");
	page=pagelist_remove(&inactive_list);
	if (page == 0)
	{
		page=pagelist_remove(&active_list);
	}
	if (page != 0)
	{
		put_into_freelist(page);
	}
	return 1;
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
		if ( i < ((reserved_size+PC_PAGESIZE)/PC_PAGESIZE))
		{
			p->flags =  (1 << PG_reserved);
		}else
		{
			pagelist_add(p,&free_list,TAIL);
		}

	}
	pc_totalpages=free_list.count.counter;
	DEBUG(" startaddr: %x endaddr:%x totalpages:%d reserved size:%d \n",pc_startaddr,pc_endaddr,total_pages,reserved_size);	
	return 1;
}
static int list_count(struct list_head *head)
{
	struct list_head *pos;
	int i=0;

	list_for_each(pos, head) {
		i++;
		if (i> 2000 || pos==0) /* TODO : remove me later */
		{
			DEBUG(" ERROR..: list count :%x \n",pos);
			return i;
		}
	}
	return i;
}

int pc_stats(char *arg1,char *arg2)
{
	ut_printf(" Total Pages : %d \n",pc_totalpages);
	ut_printf(" FREE List        : %d %d \n",free_list.count.counter,list_count(&free_list.head));
	ut_printf(" Active clean List: %d %d \n",active_list.count.counter,list_count(&active_list.head));
	ut_printf(" Dirty List       : %d %d\n",dirty_list.count.counter,list_count(&dirty_list.head));
	ut_printf(" Inactive List    : %d %d\n",inactive_list.count.counter,list_count(&inactive_list.head));
	return 1;
}
int pc_pageDirted(struct page *page) /* TODO : split the dirty list into LRU and MRU */
{
	int ret;

	ret=pagelist_move(page,&dirty_list);
	PageSetDirty(page);
	return 1;
}
int pc_pagecleaned(struct page *page) 
{
	int ret;
	struct inode *inode;
	inode=page->inode;
	if (inode == 0) BUG();
	if (inode->type == TYPE_SHORTLIVED)
	{
		ret=pagelist_move(page,&active_list);
	}else
	{
		ret=pagelist_move(page,&inactive_list);
	}
	PageClearDirty(page);
	return 1;
}
unsigned long pc_mapInodePage(struct vm_area_struct *vma,unsigned long offset)
{
	struct page *page;
	struct inode *inode;
	unsigned long ret;

	inode=vma->vm_inode;
	page=fs_generic_read(inode,offset);
	if (page == NULL) return 0;
	ret=to_ptr(page)-pc_startaddr + g_hostShmPhyAddr;
	DEBUG(" mapInodepage phy addr :%x \n",ret);
	return ret;
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
		//	DEBUG(" %d: get page address: %x  addr:%x offset :%d \n",i,page,to_ptr(page),page->offset);
		if (page->offset == page_offset)
		{
			return page;
		}
	} 
	return NULL;

}
/* page leaves from page cache */
int pc_removePage(struct page *page) 
{
	put_into_freelist(page);
	return 1;
}
/* page enters in to page cache here */
int pc_insertPage(struct inode *inode,struct page *page)
{
	struct list_head *p;
	struct page *tmp_page;
	int ret=0;
	int i=0;

	if (page->offset > inode->length) return ret;
	if (!(page->list.next==0 && page->list.prev==0 &&page->lru_list.next==0 && page->lru_list.prev==0 ))
	{
		BUG();
	}
	/*  1. link the page to inode */
	list_for_each(p, &(inode->page_list))
	{
		tmp_page=list_entry(p, struct page, list);
		i++;
		DEBUG("%d :insert page addr: %x stack addr:%x task:%x  \n",i,tmp_page,&ret,g_current_task);
		if (i > 300 ) goto last; /* TODO : remove later */
		if (page->offset < tmp_page->offset)
		{
			inode->nrpages++;
			list_add_tail(&page->list, &tmp_page->list); /* add page before tmp_Page */
			ret=1;
			break;
		} 
	} 
	if (ret == 0)
	{
		inode->nrpages++;
		DEBUG("insert page address at end : %x  \n",page);
		list_add_tail(&page->list, &(inode->page_list)); 
		ret=1;
	}
	/* 2. link the page to active or inactive list */
	if (ret == 1)
	{
		page->inode=inode;
		if (inode->type == TYPE_SHORTLIVED)
		{
			page->age=AGE_ELDEST;
			pagelist_add(page,&inactive_list,TAIL); /* add to the tail of ACTIVE list */
		}
		else
		{
			page->age=AGE_YOUNGEST;
			pagelist_add(page,&active_list,TAIL); /* add to the tail of ACTIVE list */
		}
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
	if (p == NULL)
	{
		reclaim_freepages();
		p=pagelist_remove(&free_list);
	}
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
	ret=ar_scanPtes((unsigned long)pc_startaddr,(unsigned long)pc_endaddr,&acc_list);
	DEBUG(" ScanPtes  ret:%x total:%d \n",ret,acc_list.total);
	for (i=0; i<acc_list.total; i++)
	{
		struct page *p;

		if (!(acc_list.addr[i] >= (unsigned long)pc_startaddr && acc_list.addr[i] <= (unsigned long)pc_endaddr ))
		{
			BUG();
		}	
		p=to_page((unsigned char *)acc_list.addr[i]);	
		p->age=0;
		DEBUG("%d: page addr :%x \n",i,acc_list.addr[i]);
	}
	return 1;
}
