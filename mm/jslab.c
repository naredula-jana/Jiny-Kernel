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
static void init_zeropage_cache();

#define sanity_check() do { \
	assert((unsigned long)cachep > 0x100000); \
	assert(slabp->magic_number[1] == JSLAB_MAGIC); \
	if (slabp->magic_number[0] != JSLAB_MAGIC){  \
		ut_log(" ERROR in jslab line:%d cachep:%s addr:%x slabp:%x bitmap:%x total_alloc:%d total_frees:%d\n",__LINE__,cachep->name,addr,slabp,slabp->bitmap,stat_mallocs,stat_frees); \
		while(1); \
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

		cachep = (jcache_t *) page->fs_inode; /* TODO : overloaded , need to rename from inode to private */
		assert(cachep);
		for (i = 0; i < (1 << cachep->page_order); i++) {
			struct page *p = virt_to_page((addr+(i*PAGE_SIZE)) & PAGE_MASK);
			PageClearLargePage(p);
			PageClearSlab(p);
			assert(p->fs_inode);
			p->fs_inode = 0;
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
			p->fs_inode = (struct inode *)cachep; // TODO : currently overloaded
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
	cachep->spinlock = SPIN_LOCK_UNLOCKED("jslab_cacehp");

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
		return JFAIL;
	INIT_LIST_HEAD(&cache_list);
	for (i = 0; i < MAX_PREDEFINED_CACHES; i++) {
		predefined_caches[i].obj_size = 0;
	}
	for (i = 0; cache_sizes[i] != 0 && i < MAX_PREDEFINED_CACHES; i++) {
		ret = create_jcache(&predefined_caches[i], "predefined", cache_sizes[i],0);
		if (ret == JFAIL ){
			ut_log("	ERROR: jslab cache create fails:  size:%d \n",cache_sizes[i]);
		}
	}
#ifdef JSLAB_DEBUG
	init_jslab_debug();
#endif
	init_zeropage_cache();
	init_percpu_page_cache();
	init_done = 1;
	return JSUCCESS;
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
/*****************************************  Zero Page Cache ******************************************************/
/*  Zero page cache layer: clear the free pages with zeros so that kvm KSM(Kernel Same page Merging) will compact intra vm zero pages efficiently.
 *  All the zero pages of the vm merged to one 4k page. If the VM have 1G free pages then it will be merged to 4K size saving 1G-4k memory.
 * Usefulness of maintaining zero pages:
 *  1) KSM compression of vm memory will be high if the guest OS maintains sufficient zero pages.
 *  2) calloc calls will be faster, if there is zero free page in the free list, otherwise need to memset the page with zeros syncronously.
 * TODO:
 *  1) Currently it is done in synchronously during jfree_page, it need to be done in asynchronously by an housekeeping thread.
 *  2) need to maintain seperate queues inside the free list, one for zero pages other for non-zero.
 *  3) To zero a page, a page from free list should  be picked in such way that,
 *    it should stay in the zero page list for a period of time otherwise zeroed page can be picked by malloc instead of calloc wasting cpu cycles.
 *    From the free list, malloc/calloc calls will pickup the page in LIFO order.
 *    so that zero pages stays in the zero page list for a period of time otherwise KSM will consume lot of CPU cycles in moving the pages from one state to another.
 *
 * */
int g_conf_zeropage_cache = 0;
#define MAX_ZEROLIST_SIZE 40000  /* 4k*40000 =160 Mb */
typedef struct {
	int cache_size; /* total number of pages */
	unsigned long dirtypages_list[MAX_ZEROLIST_SIZE]; /* */
	int max_dirtypage_index;
	unsigned long cleanpages_list[MAX_ZEROLIST_SIZE];  /*  this should be LIFO, so that the pages at the bottom of the stack will be picked by KSM */
	int max_cleanpage_index;
	spinlock_t		spinlock;
}zeropage_cache_t;
zeropage_cache_t zeropage_cache;
static int init_zeropage_cache_done=0;
static void init_zeropage_cache(){
	zeropage_cache.max_cleanpage_index =0;
	zeropage_cache.max_dirtypage_index = 0;
	zeropage_cache.spinlock = SPIN_LOCK_UNLOCKED("zeropage");
	init_zeropage_cache_done=1;
}
static unsigned long get_from_zeropagecache(int clear_flag){
	unsigned long intr_flags;
	unsigned long ret=0;
	int clear_page=0;
	if (init_zeropage_cache_done==0) return ret;

	spin_lock_irqsave(&(zeropage_cache.spinlock), intr_flags);
	if (clear_flag & MEM_CLEAR){
		if (zeropage_cache.max_cleanpage_index > 0){
			int i = zeropage_cache.max_cleanpage_index -1;
			ret = zeropage_cache.cleanpages_list[i];
			zeropage_cache.cleanpages_list[i] = 0;
			zeropage_cache.max_cleanpage_index = i;
			goto last;
		}
	}
	if (zeropage_cache.max_dirtypage_index > 0){
		int i = zeropage_cache.max_dirtypage_index -1;
		ret = zeropage_cache.dirtypages_list[i];
		zeropage_cache.dirtypages_list[i] = 0;
		zeropage_cache.max_dirtypage_index = i;
		if (clear_flag & MEM_CLEAR){
			clear_page = 1;
		}
#if 0
		if (clear_flag & MEM_CLEAR){
			ut_memset(ret,0,PAGE_SIZE);
		}
#endif
		goto last;
	}

last:
    spin_unlock_irqrestore(&(zeropage_cache.spinlock), intr_flags);

    if ((ret!= 0) &&  (clear_page == 1)){
    	//ut_log(" mmealloc :%x \n",ret);
		ut_memset(ret,0,PAGE_SIZE);
    }
	return ret;
}
static int insert_into_zeropagecache(unsigned long page, int flag){
	unsigned long intr_flags;
	int ret = JFAIL;
	if (init_zeropage_cache_done==0) return JFAIL;

	spin_lock_irqsave(&(zeropage_cache.spinlock), intr_flags);
	if ((flag&MEM_CLEAR) && zeropage_cache.max_cleanpage_index < MAX_ZEROLIST_SIZE){
		zeropage_cache.cleanpages_list[zeropage_cache.max_cleanpage_index] = page;
		zeropage_cache.max_cleanpage_index++;
		ret = JSUCCESS;
		goto last;
	}
	if ( zeropage_cache.max_dirtypage_index < MAX_ZEROLIST_SIZE){
		zeropage_cache.dirtypages_list[zeropage_cache.max_dirtypage_index] = page;
		zeropage_cache.max_dirtypage_index++;
		ret = JSUCCESS;
		goto last;
	}
last:
    spin_unlock_irqrestore(&(zeropage_cache.spinlock), intr_flags);
	return ret;
}
int housekeep_zeropage_cache() { /* clear the page at every call */
	int i;
	if (g_conf_zeropage_cache==0) return 0;
	for (i = 0; i < 100; i++) {
		if (!(zeropage_cache.max_cleanpage_index < MAX_ZEROLIST_SIZE)) {
			return 0;
		}
		unsigned long page = get_from_zeropagecache(0);
		if (page != 0) {
			ut_memset(page, 0, PAGE_SIZE);
			insert_into_zeropagecache(page, MEM_CLEAR);
		} else {
			page = mm_getFreePages(MEM_CLEAR, 0);
			insert_into_zeropagecache(page, MEM_CLEAR);
			//return 0;
		}
	}
	return 1;
}
static unsigned long stat_page_allocs=0;
static unsigned long stat_page_alloc_zero=0;
static unsigned long stat_page_frees=0;
struct page_bucket{
#define MAX_STACK_SIZE 1000
	unsigned long stack[MAX_STACK_SIZE+1];
	int top;
	struct page_bucket *next;
};
#define MAX_BUCKETS 32
static struct page_bucket *empty_buckets,*full_buckets;

struct percpu_pagecache {
	char inuse;

    struct page_bucket *buck1,*buck2;
	unsigned long stat_allocs,stat_frees,stat_miss_alloc,stat_miss_free;
};
static struct percpu_pagecache page_cache[MAX_CPUS];
static struct page_bucket raw_buckets[MAX_BUCKETS];
static spinlock_t jslab_cache_lock = SPIN_LOCK_UNLOCKED("jslab_cache");
static spinlock_t vmalloc_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"vmalloc");
void init_percpu_page_cache(){
	int i;

	arch_spinlock_link(&jslab_cache_lock);
	arch_spinlock_link(&vmalloc_lock);

	ut_memset(page_cache,0,MAX_CPUS*sizeof(struct percpu_pagecache));
	ut_memset(&raw_buckets[0],0,MAX_BUCKETS*sizeof(struct page_bucket));
	empty_buckets =0;
	full_buckets = 0;
	for (i=0; i<MAX_BUCKETS; i++){
		raw_buckets[i].next = empty_buckets;
		empty_buckets = &raw_buckets[i];
	}
	return;
}
static struct page_bucket *get_bucket(struct page_bucket *in){
	struct page_bucket *ret;
	int i;
	unsigned long irq_flags;

	spin_lock_irqsave(&jslab_cache_lock, irq_flags);
	ret =in;
	if (in == 0){/* get new empty bucket */
		if (empty_buckets != 0){
			ret = empty_buckets;
			empty_buckets = empty_buckets->next;
			ret->next =0;
		}
	}else if (in->top == 0){ /* get full bucket */
		if (full_buckets != 0){
			in->next = empty_buckets;
			empty_buckets = in;
			in=full_buckets;
			full_buckets = in->next;
			in->next =0;
			ret =in;
		}
	}else { /* get empty bucket */
		if (empty_buckets != 0){
			in->next = full_buckets;
			full_buckets = in;
			in=empty_buckets;
			empty_buckets = in->next;
			in->next =0;
			ret =in;
		}
	}
	spin_unlock_irqrestore(&jslab_cache_lock, irq_flags);

	return ret;
}
int g_conf_percpu_pagecache=1;
unsigned long jalloc_page(int flags){
	int cpu=getcpuid(); /* local for each cpu */

	stat_page_allocs++;
	if (flags&MEM_CLEAR){
		stat_page_alloc_zero++;
	}else if (g_conf_percpu_pagecache == 1) {
		//if (flags&MEM_NETBUF) {
		if (1) {
			if (page_cache[cpu].inuse == 0) {
				struct page_bucket *bucket;
				page_cache[cpu].inuse = 1;

				bucket = page_cache[cpu].buck1;
				if (bucket && bucket->top == 0) {
					bucket = page_cache[cpu].buck2;
					if (bucket && bucket->top == 0) {
						page_cache[cpu].buck1 = get_bucket(
								page_cache[cpu].buck1);
						bucket = page_cache[cpu].buck1;
					}
				}

				if (bucket && bucket->top > 0) {
					unsigned long ret = bucket->stack[bucket->top - 1];
					bucket->top--;
					page_cache[cpu].inuse = 0;
					page_cache[cpu].stat_allocs++;
					return ret;
				}
				page_cache[cpu].inuse = 0;
			}
			page_cache[cpu].stat_miss_alloc++;
		}
	}


	if (g_conf_zeropage_cache==1){
		unsigned long page;
		page = get_from_zeropagecache(flags);
		if (page != 0) return page;
	}
	return mm_getFreePages(flags, 0);
}
int jfree_page(unsigned long p){
	int cpu = getcpuid();

	if (PageNetBuf(virt_to_page(p))){
		BRK;
	}
	stat_page_frees++;
	//ut_log(" jfree_page:%x \n",p);
#if 1
	if (g_conf_percpu_pagecache == 1) {/* TODO:1)  we are adding the address without validation, 2) large page also into this cache which is wrong need to avoid. */
		if (page_cache[cpu].inuse == 0) {
			struct page_bucket *bucket;
			page_cache[cpu].inuse = 1;
			if (page_cache[cpu].buck1 == 0) {
				page_cache[cpu].buck1 = get_bucket(0);
				page_cache[cpu].buck2 = get_bucket(0);
			}

			bucket = page_cache[cpu].buck1;
			if (bucket && bucket->top >= MAX_STACK_SIZE) {
				bucket = page_cache[cpu].buck2;
				if (bucket && bucket->top >= MAX_STACK_SIZE) {
					bucket = get_bucket(page_cache[cpu].buck1);
					page_cache[cpu].buck1 = bucket;
				}
			}

			if (bucket && bucket->top < MAX_STACK_SIZE) {
				bucket->stack[bucket->top] = p & PAGE_MASK;
				bucket->top++;
				page_cache[cpu].inuse = 0;
				page_cache[cpu].stat_frees++;
				return 0;
			}
			page_cache[cpu].inuse = 0;
		}
		page_cache[cpu].stat_miss_free++;
	}
#endif
	if (g_conf_zeropage_cache==1){
		if (insert_into_zeropagecache(p,0) == JSUCCESS)
			return 0;
	}
	return mm_putFreePages(p, 0);
}

void *ut_calloc(size_t size){
	void *addr = ut_malloc(size);
	ut_memset(addr,0,size);
	return addr;
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
	return JSUCCESS;
}

void *vmalloc(int size, int flags){ /* TODO locks need to be addedd */
	int i,j;
	unsigned long ret=0;
	unsigned long intr_flags;

	if (vmalloc_initiated ==0) return 0;
	int total_blocks = g_vmalloc_size/vblock_size;

	spin_lock_irqsave(&vmalloc_lock, intr_flags);
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
			ret = vblocks[i].vaddr;
			goto last;
		}
	}

last:
	spin_unlock_irqrestore(&vmalloc_lock, intr_flags);
	return ret;;
}
int  vfree(addr_t addr){
	int i;
	int ret=JFAIL;
	unsigned long intr_flags;

	if (vmalloc_initiated ==0) return ret;
	if (addr==0) return ret;
	int total_blocks = g_vmalloc_size/vblock_size;

	spin_lock_irqsave(&vmalloc_lock, intr_flags);
	for (i=0; i<MAX_VBLOCKS && i<total_blocks; i++){
		if (vblocks[i].vaddr ==(addr_t *)addr){
			vblocks[i].is_free = 1;
			ret = JSUCCESS;
			goto last;
		}
	}
	ut_log(" ERROR: vfree failed : %x \n",addr);
last:
	spin_unlock_irqrestore(&vmalloc_lock, intr_flags);
	return ret;
}
#endif
/********************************* jcmd's ******************************************************************************/

jcache_t test_cache;
int Jcmd_jslabmalloc(unsigned char *arg1, unsigned char *arg2) {
	unsigned long addr;
	int type;
	int i;
	static int test_created=0;

	if (arg1 == 0)
		return 0;
	if (test_created==0){
		 create_jcache(&test_cache,"test", 100, JSLAB_FLAGS_DEBUG);
		 test_created =1;
	}
	type=ut_atoi(arg1, FORMAT_DECIMAL);

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
	int i;
	int allocs,frees;

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
	ut_printf("malloc :%d frees:%d  pagefault_write:%d\n",stat_mallocs,stat_frees,g_stat_pagefaults_write);
	ut_printf("single page allocs(zero_alloc):%d(%d) page frees:%d zeropagecache dirty/clean: %d/%d\n",stat_page_allocs,
			stat_page_alloc_zero,stat_page_frees,zeropage_cache.max_dirtypage_index,zeropage_cache.max_cleanpage_index);
	allocs=0;
	frees=0;
	for (i=0; i <MAX_CPUS; i++){
		ut_printf(" Alloc: %d Frees:%d missalloc:%d  missfrees:%d \n",page_cache[i].stat_allocs,page_cache[i].stat_frees,page_cache[i].stat_miss_alloc,page_cache[i].stat_miss_free);
		allocs=allocs+page_cache[i].stat_allocs;
		frees=frees+page_cache[i].stat_frees;
	}
	ut_printf(" total pages in cpu caches: %d (%dM)\n",(frees-allocs),((frees-allocs)*4096)/(1024*1024));
	return 1;
}

