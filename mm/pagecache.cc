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
//#define DEBUG_ENABLE 1
#include "file.hh"
extern "C" {
#include "common.h"
#include "mm.h"
#include "vfs.h"
#include "list.h"
#include "../util/host_fs/filecache_schema.h"
#define DEBUG_ENABLE 1
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

typedef struct page_list {
	struct list_head head;
	uint8_t list_type;
	atomic_t count;
} page_list_t;

#define NULL  0
page_struct_t *pagecache_map;
uint8_t *pc_startaddr;
uint8_t *pc_endaddr;
static int pc_totalpages=0;
static page_list_t free_list,active_list,dirty_list,inactive_list;
//static spinlock_t pc_lock  = SPIN_LOCK_UNLOCKED("pagecache"); /* protects page cache */
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
static int _pagelist_add(page_struct_t *page, page_list_t *list,int tail);

static inline struct page *to_page(uint8_t *addr){ /* TODO remove me later the function is duplicated in fs/host_fs.c   */
	unsigned long pn;
	pn = (addr - pc_startaddr) / PC_PAGESIZE;
	return pagecache_map + pn;
}
static int page_init(page_struct_t *p, int clear_page) {
	p->next = p->prev = NULL;
	p->count.counter = 0;
	p->flags = 0;
	p->age = 0;
	p->fs_inode = NULL;
	p->offset = 0;
	p->list_type = 0;

	p->lru_link.prev = p->lru_link.next = 0;
	p->list.prev = p->list.next = 0;
	/*	INIT_LIST_HEAD(&(p->lru_link));
	 INIT_LIST_HEAD(&(p->list)); */
	if (clear_page == 1){
		ut_memset(pcPageToPtr(p), 0, PC_PAGESIZE); /* TODO : this is performance penality to clear the page , this is to clear the BSS part of page , it is read part from the disk rest will be junk */
	}
	return 1;
}

/* this function does not need lock since it is used only during initlization */
static int init_pagelist(page_list_t *list, uint8_t type) {
	INIT_LIST_HEAD(&(list->head));
	list->count.counter = 0;
	list->list_type = type;
	return 1;
}
static int _pagelist_move(page_struct_t *page, page_list_t *list) {
	unsigned long flags;
	if (page == NULL) return 0;

	if (page->list_type != 0) {
		list_del(&(page->lru_link));
		switch (page->list_type) {
		case FREE_LIST:
			atomic_dec(&free_list.count);
			break;
		case ACTIVE_LIST:
			atomic_dec(&active_list.count);
			break;
		case INACTIVE_LIST:
			atomic_dec(&inactive_list.count);
			break;
		case DIRTY_LIST:
			atomic_dec(&dirty_list.count);
			break;
		}
		page->list_type = 0;
	}

	if (list != 0)
		return _pagelist_add(page, list, TAIL);
	else
		return 0;
}
static int _pagelist_add(page_struct_t *page, page_list_t *list, int tail) {

	if (page == NULL) return 0;

	if (tail) {
		list_add_tail(&page->lru_link, &(list->head));
	} else {
		list_add(&page->lru_link, &(list->head));
	}
	page->list_type = list->list_type;
	atomic_inc(&list->count);

//	DEBUG(" List INCREASE :%d: type:%d: \n",list->count.counter,list->list_type);
	return 1;
}

static page_struct_t *_pagelist_remove(page_list_t *list) {
	page_struct_t *page = 0;
	struct list_head *node;


	node = list->head.next;
	if (node == &list->head)
		goto last;

	list_del(node); /* delete the node from free list */
	page = list_entry(node, struct page, lru_link);
	page->list_type = 0;
	atomic_dec(&list->count);
	DEBUG(" List DECREASE :%d: type:%d: \n", list->count.counter, list->list_type);

last:
	return page;
}

#if 0
static int _reclaim_freepages(int count) {
	struct page *page;
	int i;
	int ret=0;

	DEBUG(" In the RECLAIM Free pages \n");

	for (i = 0; i < count; i++) {
		page = _pagelist_remove(&inactive_list);
		if (page == 0) {
			page = _pagelist_remove(&active_list);
		}
		if (page != 0  ) {
			if (page_inuse(page)==0 && vma_page_remove(page) == 0) {
				if (pc_deletePage(page) == JFAIL){
					BUG();
				}
				ret++;
			}else{
				_pagelist_add(page, &active_list, TAIL);
			}
		}
	}

	return 1;
}
#endif
static int list_count(struct list_head *head) {
	struct list_head *pos;
	int i = 0;

	mutexLock(g_inode_lock);
	list_for_each(pos, head) {
		i++;
	}
	mutexUnLock(g_inode_lock);

	return i;
}

/***************************** API function **********************/
uint8_t *pc_page_to_ptr(struct page *p){ /* TODO remove me later the function is duplicated in fs/host_fs.c */
        uint8_t *addr;
        unsigned long pn;
        pn=p-pagecache_map;
        addr=pc_startaddr+pn*PC_PAGESIZE;
        return addr;
}

int pc_check_valid_addr(uint8_t *addr, int len){
	unsigned long pn;
	struct page *page;

	pn = (addr - pc_startaddr) / PC_PAGESIZE;
	page = pagecache_map + pn;
	if (page->magic_number != PAGE_MAGIC){
		while(1);
	}
	if (len > PAGE_SIZE){
		while(1);
	}
	if ((addr<pc_startaddr) || (addr>pc_endaddr)){
		while(1);
	}
	return 1;
}
int pc_get_page(struct page *page){
	struct fs_inode *inode;

//	mutexLock(g_inode_lock);
	inode=(struct fs_inode *)page->fs_inode;
	if (inode != 0) {
		atomic_inc(&(inode->stat_locked_pages));
	}else{
		BUG();
	}
	atomic_inc(&page->count);
//	mutexUnLock(g_inode_lock);

	return JSUCCESS;
}
int pc_put_page(struct page *page){
	struct fs_inode *inode;
	inode=(struct fs_inode *)page->fs_inode;

//	mutexLock(g_inode_lock);
	if (inode != 0) {
		atomic_dec(&(inode->stat_locked_pages));
	}else{
		BUG();
	}

	atomic_dec(&page->count);
//	mutexUnLock(g_inode_lock);

	return JSUCCESS;
}
wait_queue *read_ahead_waitq;
int pc_read_ahead_complete(unsigned long addr){
	struct page *page = to_page(addr);

	PageClearReadinProgress(page);
	pc_put_page(page);
	read_ahead_waitq->wakeup();
}
int page_inuse(struct page *page){
	if (page->count.counter ==0 ){
		return 0;
	}else{
		return 1;
	}
}

int pc_init(uint8_t *start_addr, unsigned long len) {
	int total_pages, reserved_size;
	page_struct_t *p;
	int i;

	unsigned long s = (unsigned long) start_addr;
	start_addr = (uint8_t *) (((s + PC_PAGESIZE) / PC_PAGESIZE)
			* PC_PAGESIZE);
	pc_startaddr = (uint8_t *) (start_addr);
	pc_endaddr = (uint8_t *) start_addr + len;
	total_pages = len / PC_PAGESIZE;
	reserved_size = sizeof(fileCache_t) + sizeof(page_struct_t) * total_pages;
	pagecache_map = (page_struct_t *) (start_addr + sizeof(fileCache_t));
	ut_memset((uint8_t *) pagecache_map, 0,
			sizeof(page_struct_t) * total_pages);

	init_pagelist(&free_list, FREE_LIST);
	init_pagelist(&active_list, ACTIVE_LIST);
	init_pagelist(&dirty_list, DIRTY_LIST);
	init_pagelist(&inactive_list, INACTIVE_LIST);

	for (i = 0; i < total_pages; i++) {
		p = pagecache_map + i;
		page_init(p,1);
		p->magic_number = PAGE_MAGIC;
		if (i < ((reserved_size + PC_PAGESIZE) / PC_PAGESIZE)) {
			p->flags = (1 << PG_reserved);
		} else {
			_pagelist_add(p, &free_list, TAIL);
		}

	}
	pc_totalpages = free_list.count.counter;
	DEBUG(
			"Pagecache orgstart:%x startaddr: %x endaddr:%x totalpages:%d reserved size:%d \n", s, pc_startaddr, pc_endaddr, total_pages, reserved_size);
	return 1;
}
/*  Remove page from page cache */
int pc_deletePage(struct page *page) {
	struct fs_inode *inode;
	int ret=JFAIL;

	inode =(struct fs_inode *) page->fs_inode;
	if (inode == 0) {
		BUG();
	}

	mutexLock(g_inode_lock);
	if (page->count.counter!=0 || (inode->flags & INODE_EXECUTING)){ /* the page is  active */
		goto last;
	}
	if (page->lru_link.next != 0 || page->lru_link.prev != 0) {
		_pagelist_move(page, 0); /* remove page from any list present */
	}
	if (page->list.next == 0 || page->list.prev == 0) {
		BUG();
	}



	list_del(&(page->list));
	inode->nrpages--;
	page->fs_inode = 0;
	pc_putFreePage(page);
	ret = JSUCCESS;

last:
	mutexUnLock(g_inode_lock);
	return ret;
}
page_struct_t *pc_get_dirty_page() {
	page_struct_t *page = 0;
	struct list_head *node;

	mutexLock(g_inode_lock);
	node = dirty_list.head.next;
	if (node != &dirty_list.head) {
		page = list_entry(node, struct page, lru_link);
		pc_get_page(page);
	}
	mutexUnLock(g_inode_lock);
	return page;
}

int pc_pageDirted(struct page *page) { /* TODO : split the dirty list into LRU and MRU */
	int ret;
	assert(page !=0);

	mutexLock(g_inode_lock);
	ret = _pagelist_move(page, &dirty_list);
	PageSetDirty(page);
	mutexUnLock(g_inode_lock);
	return 1;
}
int pc_pagecleaned(struct page *page) {
	int ret;
	struct fs_inode *inode;

	assert(page !=0);
	inode = (struct fs_inode *)page->fs_inode;
	assert (inode != 0);

	mutexLock(g_inode_lock);
	if (inode->flags & INODE_SHORTLIVED) {
		ret = _pagelist_move(page, &active_list);
	} else {
		ret = _pagelist_move(page, &inactive_list);
	}
	PageClearDirty(page);
	mutexUnLock(g_inode_lock);

	return 1;
}
void pc_printInodePages(struct fs_inode *inode) {
	struct list_head *p;
	struct page *page = NULL;
	int k,i,list_index;
	unsigned long *addr;
	unsigned long cksum;

	i = 0;
	ut_printf(" file: %s \n",inode->filename);
	mutexLock(g_inode_lock);
	//list_index=get_pagelist_index(offset);
	for (list_index=0; list_index<PAGELIST_HASH_SIZE; list_index++){
		list_for_each(p, &(inode->page_list[list_index])) {
			page = list_entry(p, struct page, list);
			i++;
		//	DEBUG(" %d: get page address: %x  addr:%x offset :%d \n",i,page,to_ptr(page),page->offset);
			addr = (unsigned long *)pcPageToPtr(page);
			cksum=0;
			for (k=0; k<PAGE_SIZE; k=k+8){
				cksum = cksum+*addr;
				addr++;
			}
			ut_printf("   %d : offset:%d cksum:%d\n",i,page->offset,cksum);
		}
	}
	mutexUnLock(g_inode_lock);

	return ;
}
struct page *pc_getInodePage(struct fs_inode *inode, unsigned long offset) {
	struct list_head *p;
	struct page *page = NULL;
	int i,list_index;
	unsigned long page_offset = (offset / PC_PAGESIZE) * PC_PAGESIZE;

	i = 0;
	mutexLock(g_inode_lock);
	list_index=get_pagelist_index(offset);
	list_for_each(p, &(inode->page_list[list_index])) {
		page = list_entry(p, struct page, list);
		i++;
		//	DEBUG(" %d: get page address: %x  addr:%x offset :%d \n",i,page,to_ptr(page),page->offset);
		if (page->offset == page_offset) {
			pc_get_page(page);
			goto last;
		}else if (page_offset > page->offset){
			page = NULL;
			goto last;
		}else {
			page = NULL;
		}
	}

last:
	mutexUnLock(g_inode_lock);
	//ut_log(" list index : %x offset: %x\n",list_index,offset);
	return page;
}
struct fs_inode *latest_inode;
extern "C" {
void Jcmd_inode_data(unsigned char *arg1,unsigned char *arg2){
	struct list_head *p;
	struct page *tmp_page,*prev_page;
	unsigned long flags;
	int ret = JFAIL;
	int i = 0;
	int k;
	int list_index;
	struct fs_inode *inode = latest_inode;

	ut_printf(" filename: %s \n",inode->filename);
	for (k=0; k<PAGELIST_HASH_SIZE; k++){
		int first=0;

		mutexLock(g_inode_lock);
		list_for_each(p, &(inode->page_list[k])) {
			tmp_page = list_entry(p, struct page, list);
			if (first ==0){
				ut_printf(" %d :: ",k);
			}
			first =1;
			ut_printf(" %d,",tmp_page->offset);
		}
		mutexUnLock(g_inode_lock);
		if (first != 0){
			ut_printf("\n");
		}
	}

}
}
/* page enters in to page cache here */
int pc_insertPage(struct fs_inode *inode, struct page *page) {
	struct list_head *p;
	struct page *tmp_page,*prev_page;
	unsigned long flags;
	int ret = JFAIL;
	int i = 0;
	int list_index;
	latest_inode = inode;
	assert(page!=0);

	if (page->offset > inode->fileStat.st_size)
		return ret;
	if (!(page->list.next == 0 && page->list.prev == 0
			&& page->lru_link.next == 0 && page->lru_link.prev == 0)) {
		BUG();
	}
	prev_page=0;
	/*  1. link the page to inode */
	mutexLock(g_inode_lock);
	list_index=get_pagelist_index(page->offset);
	list_for_each(p, &(inode->page_list[list_index])) {
		tmp_page = list_entry(p, struct page, list);
		i++;
		DEBUG("%d :insert page addr: %x stack addr:%x task:%x  \n", i, tmp_page, &ret, g_current_task);
		if (page->offset == tmp_page->offset) {
			mutexUnLock(g_inode_lock);
			//ut_log("ERROR: pc fail : %x offset:%x list_index:%x\n",tmp_page,page->offset,list_index);
			return JFAIL;
		}
		if (page->offset > tmp_page->offset) {
			inode->nrpages++;
			if (prev_page == 0){
				list_add_tail(&page->list, &tmp_page->list);  /* add before the current page */
			}else{
				list_add(&page->list, &prev_page->list); /* add page after prev Page */
			}
			//list_add_tail(&page->list, &tmp_page->list); /* add page before tmp_Page */
			ret = JSUCCESS;
			break;
		}
		prev_page = tmp_page;
	}
	if (ret == JFAIL) {  /* add at the tail */
		inode->nrpages++;
		DEBUG("insert page address at end : %x  \n", page);
		if (prev_page != 0){
			list_add(&page->list, &prev_page->list); /* add page after prev Page */
		}else{
			list_add_tail(&page->list, &(inode->page_list[list_index]));
		}
		ret = JSUCCESS;
	}

	/* 2. link the page to active or inactive list */
	if (ret == JSUCCESS) {
		page->fs_inode = inode;
		if (inode->flags & INODE_EXECUTING) {
			page->age = AGE_YOUNGEST;
			_pagelist_add(page, &active_list, TAIL); /* add to the tail of ACTIVE list */
		} else {
			page->age = AGE_ELDEST;
			_pagelist_add(page, &inactive_list, TAIL); /* add to the tail of ACTIVE list */
		}
	}
	mutexUnLock(g_inode_lock);

	return ret;
}

int pc_putFreePage(struct page *page) {
	mutexLock(g_inode_lock);
	_pagelist_add(page, &free_list, TAIL); /* add to the tail of list */
	mutexUnLock(g_inode_lock);
	return 1;
}

page_struct_t *pc_getFreePage(int clear_page) {
	page_struct_t *p;
	int restarted = 0;

	mutexLock(g_inode_lock);
	p = _pagelist_remove(&free_list);
	mutexUnLock(g_inode_lock);

	if (p == NULL) {
		pc_housekeep();

		mutexLock(g_inode_lock);
		p = _pagelist_remove(&free_list);
		mutexUnLock(g_inode_lock);
	}

	if (p == NULL) {
		BUG();
	}
	if (p)
		page_init(p,clear_page);
	return p;
}
#define MAX_PAGES_SYNC 100
#define TARGET_FREELIST (0.10*pc_totalpages)
#define TARGET_INACTIVELIST (0.40*pc_totalpages)
#define TARGET_DIRTYLIST (0.20*pc_totalpages)
int pc_housekeep(void){
	int ret;
	int count,count_success;
	page_struct_t *page;

	/* sync the dirty pages if the level of free pages fall below the target or dirtlist exceed a limit*/
	ret=MAX_PAGES_SYNC;
	while (ret == MAX_PAGES_SYNC && (free_list.count.counter<TARGET_FREELIST || dirty_list.count.counter>TARGET_DIRTYLIST)){
		ret=fs_data_sync(MAX_PAGES_SYNC);
	}

	/* moves the page from active to inactive if inactive capacity falls below target */
    if (inactive_list.count.counter < TARGET_INACTIVELIST){
    	int i;
    	for (i=0; i<(0.10*active_list.count.counter); i++){

    		mutexLock(g_inode_lock);
			page = _pagelist_remove(&active_list);
			struct fs_inode *inode=(struct fs_inode *)page->fs_inode;
			if (inode->flags & INODE_EXECUTING){ /* TODO : other parameters like age of page need to be included */
				_pagelist_add(page, &active_list, TAIL);
			}else{
				_pagelist_add(page, &inactive_list, TAIL);
			}
    		mutexUnLock(g_inode_lock);
    	}
    }

    count=0;
    count_success=0;
    /* finally move the pages from inactive to freelist */
    while (free_list.count.counter < TARGET_FREELIST){

    	mutexLock(g_inode_lock);
		page = _pagelist_remove(&inactive_list);
		_pagelist_add(page, &inactive_list, TAIL);// add to the the tail and delete the page
		ret = pc_deletePage(page);
		mutexUnLock(g_inode_lock);
		if (ret == JSUCCESS){
			count_success++;
		}
		count++;
		if (count > inactive_list.count.counter){
			ut_log(" pc housekeep : count:%d sucess_count:%d \n",count,count_success);
			return 1;
		}
    }

	return 1;
}
int pc_is_freepages_available(void){
	if (free_list.count.counter<TARGET_FREELIST){
		return 0;
	}else{
		return 1;
	}
}
/***************************** House keeping functionality ********************************/


int Jcmd_pc(char *arg1,char *arg2)
{
	ut_printf("c++ PageCache Total Pages : %d = %dM\n",pc_totalpages,(pc_totalpages*4)/1024);
	ut_printf(" FREE List        : %3d %3d \n",free_list.count.counter,list_count(&free_list.head));
	ut_printf(" Active clean List: %3d %3d \n",active_list.count.counter,list_count(&active_list.head));
	ut_printf(" Dirty List       : %3d %3d\n",dirty_list.count.counter,list_count(&dirty_list.head));
	ut_printf(" Inactive List    : %3d %3d\n",inactive_list.count.counter,list_count(&inactive_list.head));
	ut_printf(" Total Inuse Pages : %d \n",(active_list.count.counter+inactive_list.count.counter+dirty_list.count.counter));
	return 1;
}
}
