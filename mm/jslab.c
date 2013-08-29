/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/jslab.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
//#define DEBUG_ENABLE
#include "common.h"
#include "task.h"
#include "interface.h"
#include "mm.h"

static struct list_head cache_list;
struct jcache_s {
	struct list_head	partial_list; /* slabs that have space */
	struct list_head	full_list; /* slabs that are full */
	const char *name;
	int obj_size;
	unsigned int flags;
	int page_order;
	int max_obj_per_slab;
	spinlock_t		spinlock;
#ifdef JSLAB_DEBUG
	int debug_enabled;
#endif

	int stat_total_objs;
	int stat_total_pages;
	int stat_allocs;
	int stat_frees;
	struct list_head cache_list;
};


#define JSLAB_MAGIC 0xabcdef12345678
typedef struct jslab {
	unsigned long magic_number[2];
	jcache_t *cachep;
	unsigned long bitmap;
}jslab_t;
static unsigned long stat_mallocs=0;
static unsigned long stat_frees=0;
//#define SLAB_MASK ((1<<(cachep->page_order+PAGE_SHIFT))-1)
#define SLAB_MASK PAGE_MASK

#ifdef JSLAB_DEBUG
#include "jslab_debug_c"
#else
void jslab_pagefault(unsigned long addr, unsigned long faulting_ip, struct fault_ctx *ctx){
	BUG();
}
#endif


#define sanity_check() do { \
	assert((unsigned long)cachep > 0x100000); \
	assert(slabp->magic_number[1] == JSLAB_MAGIC); \
	if (slabp->magic_number[0] != JSLAB_MAGIC){  \
		ut_log(" ERROR in jslab line:%d cachep:%s addr:%x slabp:%x bitmap:%x total_alloc:%d total_frees:%d\n",__LINE__,cachep->name,addr,slabp,slabp->bitmap,stat_mallocs,stat_frees); \
	}\
 } while (0)

static inline void _freepages(jcache_t *cachep, unsigned long addr) {
	unsigned long i = (1 << cachep->page_order);
	jslab_t *slabp = (jslab_t *) ((unsigned long) addr & SLAB_MASK); //TODO
	struct page *page = virt_to_page(slabp);
	assert(slabp->bitmap == 0);
	list_del(&(page->list));

	while (i--) {
		assert(PageSlab(page));
#ifdef JSLAB_DEBUG
		LEAVING_JSLAB_PERMANENTLY()
#endif
		PageClearSlab(page);
		page++;
		cachep->stat_total_pages--;
	}
	mm_putFreePages((unsigned long) ((unsigned long) addr & SLAB_MASK),
			cachep->page_order);
}

static inline void * _getpages(jcache_t *cachep) {
	unsigned long addr;
	unsigned int flags;
	unsigned long i = (1 << cachep->page_order);

	flags = cachep->flags;
	addr = (unsigned long) mm_getFreePages(flags, cachep->page_order);
	if (addr == 0)
		return 0;
	struct page *page = virt_to_page(addr);
	jslab_t *slabp = (jslab_t *)addr;

	/* attach slab to cache */
	slabp->bitmap = 0;
	slabp->cachep = cachep;
	slabp->magic_number[0] = JSLAB_MAGIC;
	slabp->magic_number[1] = JSLAB_MAGIC;
	list_add(&(page->list), &(cachep->partial_list));

	while (i--) {
#ifdef JSLAB_DEBUG
		LEAVE_JSLAB()
#endif
		PageSetSlab(page);
		cachep->stat_total_pages++;
		page++;
	}
	return (void *)addr;
}


static void free_obj(unsigned long addr) {
	struct page *page = virt_to_page(addr & PAGE_MASK);
	jcache_t *cachep;
	jslab_t *slabp;

	assert(PageSlab(page));
	if (PageLargePage(page)) {/* large page object */
		int i;

		cachep = (jcache_t *) page->inode; /* TODO : overloaded , need to rename from inode to private */
		assert(cachep);
		for (i = 0; i < (1 << cachep->page_order); i++) {
			struct page *p = virt_to_page((addr+(i*PAGE_SIZE)) & PAGE_MASK);
			PageClearLargePage(p);
			PageClearSlab(p);
			assert(p->inode);
			p->inode = 0;
		}
		mm_putFreePages(addr, cachep->page_order);
		cachep->stat_frees++;
		cachep->stat_total_objs--;
		return;
	}

	slabp = (jslab_t *) (addr & SLAB_MASK);
	cachep = slabp->cachep;
	int index;
	unsigned long intr_flags;

	sanity_check();

	index = (addr - (addr & PAGE_MASK) - sizeof(jslab_t)) / cachep->obj_size; //TODO : need to SLAB_MASK for multipage
	assert(index<64);
	assert(test_bit(index,&(slabp->bitmap)));

#ifdef JSLAB_DEBUG
	ENTER_JSLAB()
#endif

	spin_lock_irqsave(&(cachep->spinlock), intr_flags);
	clear_bit(index, &(slabp->bitmap));
	if (slabp->bitmap == 0) {
		_freepages(cachep, (unsigned long)slabp);
	}
	spin_unlock_irqrestore(&(cachep->spinlock), intr_flags);
#ifdef JSLAB_DEBUG
	taddr = (unsigned long)slabp;
	LEAVE_JSLAB()
#endif
	cachep->stat_total_objs--;
	cachep->stat_frees++;
	sanity_check();
	stat_frees++;
}
static void * _alloc_obj(jcache_t *cachep) {
	int index;
	unsigned long addr=0;

	if(cachep->obj_size >= PAGE_SIZE){ /* large page object */
		int i;
		addr = (unsigned long) mm_getFreePages(0, cachep->page_order);
		if (addr == 0) return 0;
		for (i = 0; i < (1 << cachep->page_order); i++) {
			struct page *p = virt_to_page((addr+(i*PAGE_SIZE)) & PAGE_MASK);
			PageSetLargePage(p);
			PageSetSlab(p);
			p->inode = (struct inode *)cachep; // TODO : currently overloaded
		}
		cachep->stat_allocs++;
		cachep->stat_total_objs++;
		return (void *)addr;
	}

	if (list_empty(&(cachep->partial_list))) {
		_getpages(cachep);
		if (list_empty(&(cachep->partial_list))) {
			return 0;
		}
	}
	jslab_t *slabp;
	struct page *page = list_entry(cachep->partial_list.next, struct page, list);
	slabp = page_to_virt(page);
	sanity_check();
#ifdef JSLAB_DEBUG
	ENTER_JSLAB()
#endif
	index = find_first_zero_bit(&(slabp->bitmap), 64);
	assert(index >=0);
	set_bit(index, &(slabp->bitmap));
	addr = (unsigned long)slabp;
	addr = addr + sizeof(jslab_t) + (index * cachep->obj_size);

	index = find_first_zero_bit(&(slabp->bitmap), 64);
	if (((index + 1) > cachep->max_obj_per_slab) || index == -1) {
		list_del(&(page->list));
		list_add(&(page->list), &(cachep->full_list));
	}
	cachep->stat_total_objs++;
	ut_memset((unsigned char *)addr,0,cachep->obj_size);
#ifdef JSLAB_DEBUG
	taddr = (unsigned long)slabp;
	LEAVE_JSLAB()
#endif
	sanity_check();
	stat_mallocs++;
	cachep->stat_allocs++;
	return (void *)addr;
}
static int log_n(int n){
	int ret=0;
	while (n>0){
		n=n/2;
		ret++;
	}
	if (ret > 0) ret=ret -1;
	return ret;
}
static int create_jcache(jcache_t *cachep, const char *name, int obj_size,unsigned long flags) {
	int max_obj_space;

	if (cachep == 0)  return JFAIL;
	if (obj_size >= PAGE_SIZE) {
		max_obj_space = obj_size;
		cachep->page_order = log_n(obj_size >> PAGE_SHIFT) ;
	} else {
		max_obj_space = (PAGE_SIZE - sizeof(jslab_t));
		cachep->page_order = 0; //TODO : currently only one page can be used
		if (obj_size > max_obj_space) {
			assert(0);
			return JFAIL;
		}
	}
	cachep->name = name;
	cachep->obj_size = obj_size;
	INIT_LIST_HEAD(&(cachep->partial_list));
	INIT_LIST_HEAD(&(cachep->full_list));
	INIT_LIST_HEAD(&(cachep->cache_list));

	cachep->max_obj_per_slab = max_obj_space / obj_size;
	cachep->spinlock = SPIN_LOCK_UNLOCKED(0);

	cachep->stat_total_objs = 0;
	cachep->stat_total_pages = 0;
	cachep->stat_allocs = 0;
	cachep->stat_frees = 0;

	list_add(&(cachep->cache_list), &(cache_list));
#ifdef JSLAB_DEBUG
	if (flags & JSLAB_FLAGS_DEBUG){
		cachep->debug_enabled  = 1;
	}
#endif

	return JSUCCESS;
}

/****************************************** standard/predefined caches ************/

#define MAX_PREDEFINED_CACHES 25
int cache_sizes[] = { 32, 64, 128, 256, 512, 1024, 2048,3700, PAGE_SIZE, PAGE_SIZE*2, PAGE_SIZE*4, PAGE_SIZE*8, PAGE_SIZE*16, PAGE_SIZE*32, PAGE_SIZE*64, PAGE_SIZE*128, PAGE_SIZE*256, PAGE_SIZE*512, PAGE_SIZE*1024, 0 };
jcache_t predefined_caches[MAX_PREDEFINED_CACHES];
int init_jslab(unsigned long arg1) {
	int i,ret;
	static int init_done = 0;

	if (init_done == 1)
		return 0;
	INIT_LIST_HEAD(&cache_list);
	for (i = 0; i < MAX_PREDEFINED_CACHES; i++) {
		predefined_caches[i].obj_size = 0;
	}
	for (i = 0; cache_sizes[i] != 0 && i < MAX_PREDEFINED_CACHES; i++) {
		ret = create_jcache(&predefined_caches[i], "predefined", cache_sizes[i],0);
		if (ret == JFAIL ){
			ut_log("	ERROR: jslab cacahe create fails:  size:%d \n",cache_sizes[i]);
		}
	}
#ifdef JSLAB_DEBUG
	init_jslab_debug();
#endif

	init_done = 1;
	return 0;
}
/************************************** API *************************************************************************************/
void *  jslab_alloc_from_cache(jcache_t *cachep, int flags){
	void * ret;
	unsigned long intr_flags;

	if (cachep==0) return 0;

	spin_lock_irqsave(&(cachep->spinlock), intr_flags);
	ret = _alloc_obj(cachep);
	spin_unlock_irqrestore(&(cachep->spinlock), intr_flags);

	return ret;
}
void * jslab_alloc_from_predefined(size_t obj_size, int flags) {
	jcache_t *cachep = NULL;
	void *ret;
	int i;
	unsigned long intr_flags;

	for (i = 0; i < MAX_PREDEFINED_CACHES && predefined_caches[i].obj_size != 0;
			i++) {
		if (obj_size <= predefined_caches[i].obj_size) {
			cachep = &predefined_caches[i];
			break;
		}
	}
	if (cachep == NULL){
		BUG();
		return 0;
	}

	spin_lock_irqsave(&(cachep->spinlock), intr_flags);
	ret = _alloc_obj(cachep);
	spin_unlock_irqrestore(&(cachep->spinlock), intr_flags);

	return  ret;
}

void jslab_free1(jcache_t *cachep, void *obj) {
	free_obj((unsigned long)obj);
}
void jslab_free2(const void *objp){
	if (objp==0) return;
	free_obj((unsigned long)objp);
}

jcache_t *
jslab_create_cache (const uint8_t *name, size_t size, size_t offset,
	unsigned long flags, void (*ctor)(void*, jcache_t *, unsigned long),
	void (*dtor)(void*, jcache_t *, unsigned long))
{
	jcache_t *cachep;
	int ret;

	cachep = (jcache_t *)jslab_alloc_from_predefined(sizeof(jcache_t),0);
	assert(cachep!=0);
	ret = create_jcache(cachep, name, size, flags);
	if (ret == JFAIL) {
		ut_log(" ERROR: jslab cache creation fails for :%s: lenght:%d \n",name,size);
		BUG();
		jslab_free2(cachep);
		return 0;
	}
	return cachep;
}

int jslab_destroy_cache(jcache_t *cachep) {
	if (!list_empty(&(cachep->partial_list)))
		return JFAIL;
	if (!(list_empty(&(cachep->full_list))))
		return JFAIL;

	list_del(&(cachep->cache_list));
	return JSUCCESS;
}
/*************************** simple vmalloc subsystem ******************************************/
#if 1
#define MAX_VBLOCKS 100
 struct {
	void *vaddr;
	int size;
	int is_free;
}vblocks[MAX_VBLOCKS];
extern unsigned long g_vmalloc_start;
extern unsigned long g_vmalloc_size;
static int vmalloc_initiated=0;
static int vblock_size=0x200000;
int init_jslab_vmalloc(){
	int i;

	int total_blocks = g_vmalloc_size/vblock_size;
	for (i=0; i<MAX_VBLOCKS ; i++){
		if (i<total_blocks){
			vblocks[i].vaddr = (void *)(g_vmalloc_start + i*vblock_size);
			vblocks[i].size = vblock_size;
			vblocks[i].is_free = 1;
		}else{
			vblocks[i].vaddr = 0;
			vblocks[i].size = 0;
			vblocks[i].is_free = 0;
		}
	}
	uint8_t *p=(uint8_t *)g_vmalloc_start;
	*p='1';
	p=p+g_vmalloc_size-10;
	*p='1';

	vmalloc_initiated = 1;
	return 0;
}
void *vmalloc(int size, int flags){
	int i,j;

	if (vmalloc_initiated ==0) return 0;
	int total_blocks = g_vmalloc_size/vblock_size;
	for (i=0; i<MAX_VBLOCKS && i<total_blocks; i++){
		if (vblocks[i].vaddr == 0 ) return 0;
		if (vblocks[i].size > size && vblocks[i].is_free == 1){
			vblocks[i].is_free = 0;

			/* touch the memory mouch so that page faults will not happend when a nonrecursive global spin lock is taken */
			unsigned char *p=vblocks[i].vaddr;
			for (j=0; j<vblocks[i].size; j=j+PAGE_SIZE){
			 	p[j]=0;
			}
			if (flags & MEM_CLEAR){
				ut_memset(vblocks[i].vaddr,0,size);
			}
			return vblocks[i].vaddr;
		}
	}

	return 0;
}
void vfree(addr_t addr){
	int i;

	if (vmalloc_initiated ==0) return;
	if (addr==0) return;
	int total_blocks = g_vmalloc_size/vblock_size;
	for (i=0; i<MAX_VBLOCKS && i<total_blocks; i++){
		if (vblocks[i].vaddr ==(addr_t *)addr){
			vblocks[i].is_free = 1;
			return;
		}
	}
	return;
}
#endif
/********************************* jcmd's ******************************************************************************/
#if 0
int Jcmd_jslabfree(unsigned char *arg1, unsigned char *arg2) {
	if (arg1 == 0)
		return 0;

	jslab_free2((const void *)ut_atoi(arg1));

	return 1;
}
#endif
jcache_t test_cache;
int Jcmd_jslabmalloc(unsigned char *arg1, unsigned char *arg2) {
	unsigned long addr;
	int type;
	static int test_created=0;

	if (arg1 == 0)
		return 0;
	if (test_created==0){
		 create_jcache(&test_cache,"test", 100, JSLAB_FLAGS_DEBUG);
		 test_created =1;
	}
	type=ut_atoi(arg1);

	addr = (unsigned long)jslab_alloc_from_cache(&test_cache,0);
	ut_printf("New addr :%x \n", addr);
	if (type==1){
		unsigned char *p=(unsigned char *)addr;
		ut_printf(" Legal modified the contents in the address :%x\n",p);
		*p='1';
	}
	if (type==2){
		unsigned char *p=(unsigned char *)(addr&PAGE_MASK);
		ut_printf(" Legal modified the contents in the address @ :%x\n",p);
		*p='2';
	}
	return 1;
}
extern int g_stat_pagefaults_write;
int Jcmd_jslab(unsigned char *arg1, unsigned char *arg2) {
	jcache_t *cachep;
	struct list_head *c;

	ut_printf(" malloc :%d frees:%d  pagefault_write:%d\n",stat_mallocs,stat_frees,g_stat_pagefaults_write);
	list_for_each(c, &(cache_list)) {
		cachep = list_entry(c, jcache_t , cache_list);
#ifdef JSLAB_DEBUG
		if (cachep->debug_enabled) {
			ut_printf("***");
		}
#endif
		ut_printf("%s %d objs/pages:%d/%d allocs/free:%d/%d order:%x\n", cachep->name, cachep->obj_size,
				cachep->stat_total_objs, cachep->stat_total_pages,cachep->stat_allocs,cachep->stat_frees,cachep->page_order);
	}
	return 1;
}

