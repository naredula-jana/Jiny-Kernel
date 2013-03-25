/*
 * mm/kmemleak.c
 *
 * Copyright (C) 2008 ARM Limited
 * Written by Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * For more information on the algorithm and kmemleak usage, please see
 * Documentation/kmemleak.txt.
 *
 * Notes on locking
 * ----------------
 *
 * The following locks and mutexes are used by kmemleak:
 *
 * - kmemleak_lock (rwlock): protects the object_list modifications and
 *   accesses to the object_tree_root. The object_list is the main list
 *   holding the metadata (struct kmemleak_object) for the allocated memory
 *   blocks. The object_tree_root is a priority search tree used to look-up
 *   metadata based on a pointer to the corresponding memory block.  The
 *   kmemleak_object structures are added to the object_list and
 *   object_tree_root in the create_object() function called from the
 *   kmemleak_alloc() callback and removed in delete_object() called from the
 *   kmemleak_free() callback
 * - kmemleak_object.lock (spinlock): protects a kmemleak_object. Accesses to
 *   the metadata (e.g. count) are protected by this lock. Note that some
 *   members of this structure may be protected by other means (atomic or
 *   kmemleak_lock). This lock is also held when scanning the corresponding
 *   memory block to avoid the kernel freeing it via the kmemleak_free()
 *   callback. This is less heavyweight than holding a global lock like
 *   kmemleak_lock during scanning
 *
 *
 * The kmemleak_object structures have a use_count incremented or decremented
 * using the get_object()/put_object() functions. When the use_count becomes
 * 0, this count can no longer be incremented and put_object() schedules the
 * kmemleak_object freeing via an RCU callback. All calls to the get_object()
 * function must be protected by rcu_read_lock() to avoid accessing a freed
 * structure.
 */

/*
 *  Largely Modified by Naredula Janardhana Reddy while porting the code to Jiny
 *
 *  TODO :
 *    1) rcu_locks removed: need to  put back or  locking need to be taken care.
 *    2) scanning for mem leak need to be improved:
 *        a) scan areas nodes to use while scanning, currently they are not used.
 *        b) global locking are used while scanning/malloc/free .
 *        c) traces need to be fine tune(removing kmemcheck functions in the trace)
 *        d) kmemcheck_is_obj_initialized needs architecture dependent code to check if the memory of initialized or not. .(http://lwn.net/Articles/260068/)
 *        e) using jiffies to record the timestamp of the object, and then avoiding scanning of certain objects.
 *        f) clearing the block when the freeing the block , this improves false postive/negarive results.
 *
 */
#include "os_dep.h"

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt


#include "misc.h"
#define pgoff_t unsigned long
#include "prio_tree.h"


//#include "prio_tree.h"
typedef unsigned int u32;
typedef unsigned char u8;
#define bool char
#define true 1
#define false 0

#define pr_warning pr_debug
#define pr_notice pr_debug
#define pr_info pr_debug

#define gfp_t int
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define ULONG_MAX       (~0UL)
/*
 * Kmemleak configuration and common defines.
 */
#define MAX_TRACE		8	/* stack trace length */
#define MSECS_MIN_AGE		5000	/* minimum object age for reporting */
#define SECS_FIRST_SCAN		60	/* delay before the first scan */
#define SECS_SCAN_WAIT		600	/* subsequent auto scanning delay */
#define MAX_SCAN_SIZE		4096	/* maximum size of a scanned block */

#define BYTES_PER_POINTER	sizeof(void *)
#define USE_STATIC_MEMORY 1
/* scanning area inside a memory block */
struct kmemleak_scan_area {
	struct hlist_node node;
	unsigned long start;
	size_t size;
#ifdef USE_STATIC_MEMORY
	struct kmemleak_scan_area *next_free;
#endif
};

#define KMEMLEAK_GREY	0
#define KMEMLEAK_BLACK	-1
extern int memleakHook_disable();
/*
 * Structure holding the metadata for each allocated memory block.
 * Modifications to such objects should be made while holding the
 * object->lock. Insertions or deletions from object_list, gray_list or
 * tree_node are already protected by the corresponding locks or mutex (see
 * the notes on locking above). These objects are reference-counted
 * (use_count) and freed using the RCU mechanism.
 */
#define MAGIC_NUMBER 0xabcdef112233
struct kmemleak_object {
	struct list_head object_list;
	struct list_head gray_list;
	unsigned long magic;
	struct prio_tree_node tree_node;
	struct kmemleak_object *next;

	unsigned long pointer;
	size_t size;
	unsigned char flags; /* object status flags */

	/* the total number of pointers found pointing to this object */
	int count;
	unsigned long type;
#ifdef USE_STATIC_MEMORY
	struct kmemleak_object *next_free;
#endif
	unsigned long *trace[MAX_TRACE];
};

struct stack_trace {
	unsigned int nr_entries, max_entries;
	unsigned long *entries;
	int skip; /* input argument: How many entries to skip */
};
void kmemleak_scan(void);
/* flag representing the memory block allocation status */
#define OBJECT_ALLOCATED	(1 << 0)
/* flag set after the first reporting of an unreferenced object */
#define OBJECT_REPORTED		(1 << 1)
/* flag set to not scan the object */
#define OBJECT_NO_SCAN		(1 << 2)

/* number of bytes to print per line; must be 16 or 32 */
#define HEX_ROW_SIZE		16
/* number of bytes to print at a time (1, 2, 4, 8) */
#define HEX_GROUP_SIZE		1
/* include ASCII after the hex output */
#define HEX_ASCII		1
/* max number of lines to be printed */
#define HEX_MAX_LINES		2


int memleak_serious_bug=0;

/*
 * Print a warning and dump the stack trace.
 */
#define kmemleak_warn(x...)	do {		\
	pr_warning(x);				\
	dump_stack();				\
	atomic_set(&kmemleak_warning, 1);	\
} while (0)

/*
 * Macro invoked when a serious kmemleak condition occurred and cannot be
 * recovered from. Kmemleak will be disabled and further allocation/freeing
 * tracing no longer available.
 */
#define kmemleak_stop(x...)	do {	\
	kmemleak_warn(x);		\
	kmemleak_disable();		\
} while (0)

static inline long IS_ERR(const void *ptr) {/* TODO */
	return 0;
}
#define WARN_ON(x)
#define MM_BUG(x) stat_errors[x]++;
unsigned long err_arg1=0;
unsigned long err_arg2=0;

/************************   Start of any global variables should be declaired here */
static unsigned long dummy_start_uninitilized_var; /* some unitialized variable */
/* the list of all allocated objects */
static struct prio_tree_root object_tree_root;
static LIST_HEAD(object_list);
/* the list of gray-colored objects (see color_gray comment below) */
static LIST_HEAD(gray_list);

//static spinlock_t kmemleak_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t kmemleak_lock = {0};


/* set if tracing memory operations is enabled */
static atomic_t kmemleak_enabled = ATOMIC_INIT(0);
/* set in the late_initcall if there were no errors */


/* set if a fatal kmemleak error has occurred */
static atomic_t kmemleak_error = ATOMIC_INIT(0);
#define MAX_ERRORS 10
static int stat_obj_count,stat_errors[MAX_ERRORS];

#define MAX_TYPES 500
static struct {
	int count;
	unsigned long type;
	unsigned long mem_leak,mem_consumed;
	unsigned long *trace[MAX_TRACE];
}obj_types[MAX_TYPES];


#ifdef USE_STATIC_MEMORY
#define MAX_HASH (MAX_STATIC_OBJS-1)
//static struct kmemleak_object *hash_table[MAX_HASH+10];
static struct kmemleak_object objs[MAX_STATIC_OBJS+10];
static struct kmemleak_object *bhash_table[MAX_HASH+10]; /* TODO this is just added to avoid some memory corruption in hash_table */
static struct kmemleak_scan_area scan_areas[MAX_STATIC_SCAN_AREAS];
static void *object_cache,*scan_area_cache;
#else
/* allocation caches for kmemleak internal data */
static kmem_cache_t *object_cache;
static kmem_cache_t *scan_area_cache;
#endif

static unsigned long dummy_end_uninitilized_var; /* end of uninitialized variable */
/************************   END of any global variables should be declaired here */
#ifdef USE_STATIC_MEMORY
int sanity_location=0;
int sanity_check(struct kmemleak_object *o,int loc){
	if (o->magic != MAGIC_NUMBER){
		memleak_serious_bug=1;
		sanity_location=loc;
		return 0;
	}
	return 1;
}
static void *memleak_kmem_cache_create(char *type, int unused1 , int unused2 , int unused3, int unused4, int unused5)
{
	int i;

	if (strcmp((unsigned char *)type, (unsigned char *)"kmemleak_objects") == 0) {
		for (i = 0; i < (MAX_STATIC_OBJS - 1); i++) {
			objs[i].next_free = &objs[i + 1];
			objs[i].magic=MAGIC_NUMBER;
			objs[i].next=0;
			objs[i].pointer=0;
			objs[i].flags=0;
		}
		objs[MAX_STATIC_OBJS - 1].next_free = 0;
		objs[MAX_STATIC_OBJS - 1].magic=MAGIC_NUMBER;
		objs[MAX_STATIC_OBJS - 1].next=0;
		objs[MAX_STATIC_OBJS - 1].pointer=0;
		objs[MAX_STATIC_OBJS - 1].flags=0;
		return (void *)&objs[0];
	} else if (strcmp((unsigned char*)type, (unsigned char *)"kmemleak_scan_area") == 0) {
		for (i = 0; i < (MAX_STATIC_SCAN_AREAS - 1); i++) {
			scan_areas[i].next_free = &scan_areas[i + 1];
		}
		scan_areas[MAX_STATIC_SCAN_AREAS - 1].next_free = 0;
		return (void *)&scan_areas[0];
	} else {
		MM_BUG(9);
	}
	return 0;
}
static void memleak_kmem_cache_free(void *cache, void *object) {
	if (cache == object_cache) {
		struct kmemleak_object *obj = object;
		obj->next_free = object_cache;
		object_cache = obj;
	} else if (cache == scan_area_cache) {
		struct kmemleak_scan_area *obj = object;
		obj->next_free = scan_area_cache;
		scan_area_cache = obj;
	} else {
		MM_BUG(8);
	}
	return;
}
static void *memleak_kmem_cache_alloc(void *cache, void *flag) {
	if (cache == object_cache) {
		struct kmemleak_object *obj = object_cache;
		if (object_cache == 0)
			return 0;
		object_cache = obj->next_free;
		if (object_cache != NULL &&  ((object_cache<((void *)&objs[0])) || (object_cache>(void *)(&objs[MAX_STATIC_OBJS-1]))) ){
			memleak_serious_bug=1;
			sanity_location=9011;
			return 0;
		}

		return obj;
	} else if (cache == scan_area_cache) {
		struct kmemleak_scan_area *obj = scan_area_cache;
		if (scan_area_cache == 0)
			return 0;
		scan_area_cache = obj->next_free;
		return obj;
	} else {
		pr_debug("Error: Cannot allocate kmemleak object \n");
	}
	return 0;
}
#endif
/************************************   Hash functions **********************************************
 */
#if 0
static int hash_func(unsigned long ptr){
	unsigned long p=ptr>>4;

	p=p%MAX_HASH;
	return ((int)p);
}

static struct kmemleak_object *hash_search(unsigned long ptr) {
	struct kmemleak_object *p;
	int i,loops;

	i = hash_func(ptr);
	p = hash_table[i];
	loops=0;
	if( (p!=NULL) &&  ((p>(&objs[MAX_STATIC_OBJS+10])) || (p<(&objs[0])))) {
		memleak_serious_bug=1;
		sanity_location=985;

		err_arg1=(unsigned long)i;
		err_arg2=(unsigned long)p;
		return NULL;
	}
	while (p != NULL) {
		if(sanity_check(p,10)==0){
			return NULL;
		}
		loops++;
		if (p->pointer == ptr)
			return p;
		p = p->next;
		if (loops==MAX_STATIC_OBJS){
			memleak_serious_bug=1;
			sanity_location=100;
			return NULL;
		}
	}
	return NULL;

}
static int hash_insert(struct kmemleak_object *obj) {
	int i;
	if (obj->next != 0) {
		memleak_serious_bug=1;
		sanity_location = 200;
		return 1;
	}
	i = hash_func(obj->pointer);
	obj->next = hash_table[i];
	hash_table[i] = obj;
	return 1;
}
static int max_hash_list=0;
static int hash_remove(struct kmemleak_object *obj) {
	int i;
	int loops=0;
	struct kmemleak_object *p, *prev;

	i = hash_func(obj->pointer);
	p = hash_table[i];
	prev = 0;

	while (p != 0) {
		if(sanity_check(p,11)==0){
			return 0;
		}
		loops++;
		if (loops==MAX_STATIC_OBJS){
			memleak_serious_bug=1;
			sanity_location=99;
			return 0;
		}


		if (p->pointer == obj->pointer) { /* found it */
			if (prev == 0) {
				hash_table[i] = p->next;
			} else {
				prev->next = p->next;
			}
			p->next = 0;
			if (loops > max_hash_list) max_hash_list=loops;
			return 1;
		}
		prev = p;
		p = p->next;
	}
	/* ERROR: should not reach here */
	memleak_serious_bug=1;
	sanity_location=201;
	return 0;
}
#endif
/************************************* end of Hash functions **************************/

extern void *code_region_start, *data_region_start,*data_region_end;
/*
 * Object colors, encoded with count and min_count:
 * - white - orphan object, not enough references to it (count < min_count)
 * - gray  - not orphan, not marked as false positive (min_count == 0) or
 *		sufficient references to it (count >= min_count)
 * - black - ignore, it doesn't contain references (e.g. text section)
 *		(min_count == -1). No function defined for this color.
 * Newly created objects don't have any color assigned (object->count == -1)
 * before the next memory scan when they become white.
 */
static bool color_white(const struct kmemleak_object *object) {
	return  object->count < 1;
}

static bool color_gray(const struct kmemleak_object *object) {
	return  object->count >= 1;
}

/*
 * Look-up a memory block metadata (kmemleak_object) in the priority search
 * tree based on a pointer value. If alias is 0, only values pointing to the
 * beginning of the memory block are allowed. The kmemleak_lock must be held
 * when calling this function.
 */
static struct kmemleak_object *_lookup_object(unsigned long ptr, int alias) {
    struct prio_tree_node *node;
    struct prio_tree_iter iter;
    struct kmemleak_object *object;

    prio_tree_iter_init(&iter, &object_tree_root, ptr, ptr);
    node = prio_tree_next(&iter);
    if (node) {
        object = prio_tree_entry(node, struct kmemleak_object, tree_node);
        if (!alias && object->pointer != ptr) {
            //kmemleak_warn("Found object by alias at 0x%08lx\n",ptr);
            //dump_object_info(object);
            object = NULL;
        }
    } else
        object = NULL;

    return object;
}


//#define PAGE_SIZE (0x1000)
/*
 * Save stack trace to the given array of MAX_TRACE size.
 */
static unsigned int save_stack_trace(unsigned long **trace_output) {
	struct stack_trace stack_trace;
	unsigned long addr;
	unsigned long *stack_top = &addr;
	unsigned long sz, stack_end;
	int i;
//return 0;

	stack_trace.max_entries = MAX_TRACE;
	stack_trace.nr_entries = 0;
	stack_trace.entries = (unsigned long *) trace_output;
	stack_trace.skip = 5;

	sz = (long) stack_top;
	sz = sz / 4;
	sz = sz * 4;

	stack_top = (unsigned long *) sz;
	i = 0;
	sz = ~(PAGE_SIZE - 1);
	stack_end = (unsigned long) stack_top & (sz);
	stack_end = stack_end + PAGE_SIZE -10;

	if (stack_end) {
		while (((unsigned long) stack_top < stack_end)
				&& i < (MAX_TRACE + stack_trace.skip)) {
			addr = *stack_top;
			stack_top = stack_top + 1;
			if ((addr > (unsigned long) code_region_start)
					&& (addr < (unsigned long) data_region_start)) {
				if (i >= stack_trace.skip) {
					stack_trace.entries[i - (stack_trace.skip)] = addr;
				}
				i++;
			}
		}
		stack_trace.nr_entries = i - stack_trace.skip;
	}

	return stack_trace.nr_entries;
}

/*
 * Create the metadata (struct kmemleak_object) corresponding to an allocated
 * memory block and add it to the object_list and object_tree_root.
 */
static struct kmemleak_object *create_object(unsigned long ptr, size_t size, int type,
		int min_count)
{
	unsigned long flags;
	struct kmemleak_object *object=NULL;
	struct prio_tree_node *node;

	if (ptr==0){
        MM_BUG(6);
		return NULL;
	}

	spin_lock_irqsave(&kmemleak_lock, flags); /* global lock  : create */
	object = _lookup_object(ptr, 0);
	if (object!=0){
		MM_BUG(5);
		object=NULL;
		goto out;
	}
	object = memleak_kmem_cache_alloc(object_cache, 0);
	if (!object) {
		memleak_serious_bug=1;
		sanity_location=801;
		goto out;
	}
	sanity_check(object,3);
	stat_obj_count++;
	INIT_LIST_HEAD(&object->object_list);
	INIT_LIST_HEAD(&object->gray_list);
	object->flags = OBJECT_ALLOCATED;
	object->pointer = ptr;
	object->size = size;
	object->type = (unsigned char)type;
	object->count = 0; /* white color initially */

	/* kernel backtrace */
    save_stack_trace(&object->trace[0]);

    INIT_PRIO_TREE_NODE(&object->tree_node);
    object->tree_node.start = ptr;
    object->tree_node.last = ptr + size - 1;
    node = prio_tree_insert(&object_tree_root, &object->tree_node);
    if (node != &object->tree_node)  /* Failed to insert in to tree */
    {
		memleak_serious_bug=1;
		sanity_location=800;
		goto out;
    }
	list_add_tail(&object->object_list, &object_list);

out:
	spin_unlock_irqrestore(&kmemleak_lock, flags);
	return object;
}

/*
 * Look up the metadata (struct kmemleak_object) corresponding to ptr and
 * delete it.
 */
static void delete_object(unsigned long ptr) {
	struct kmemleak_object *object;
	unsigned long flags;

	spin_lock_irqsave(&kmemleak_lock, flags);/* global lock : delete */
	object = _lookup_object(ptr, 0);
	if (!object) {
		int i;

		spin_unlock_irqrestore(&kmemleak_lock, flags);
		MM_BUG(2);
		for (i = 0; i < (MAX_STATIC_OBJS - 1); i++) {
			if (objs[i].pointer == ptr) {
				pr_debug("ERROR Found the pointer But Failed using Tree :%x  %d next=%x\n",ptr, i, objs[i].next);
				memleak_serious_bug = 1;
				sanity_location=809;
				return;
			}
		}
		return;
	} else {
		sanity_check(object, 2);
	    prio_tree_remove(&object_tree_root, &object->tree_node);
		list_del(&object->object_list);
		object->flags &= ~OBJECT_ALLOCATED;
		object->pointer = 0;
		memleak_kmem_cache_free(object_cache, object);
		spin_unlock_irqrestore(&kmemleak_lock, flags);

		stat_obj_count--;
		return;
	}
}
#if 0
/*
 * Add a scanning area to the object. If at least one such area is added,
 * kmemleak will only scan these ranges rather than the whole memory block.
 */
void add_scan_area(unsigned long ptr, size_t size) {
	struct kmemleak_object *object;
	struct kmemleak_scan_area *area;

	object = find_and_get_object(ptr, 1);
	if (!object) {
		kmemleak_warn("Adding scan area to unknown object at 0x%08lx\n", ptr);
		return;
	}

	area = memleak_kmem_cache_alloc(scan_area_cache, 0);
	if (!area) {
		pr_warning("Cannot allocate a scan area\n");
		goto out;
	}


	if (ptr + size > object->pointer + object->size) {
		kmemleak_warn("Scan area larger than object 0x%08lx\n", ptr);
		memleak_kmem_cache_free(scan_area_cache, area);
		goto out_unlock;
	}

	INIT_HLIST_NODE(&area->node);
	area->start = ptr;
	area->size = size;

	out_unlock:

	out:
	return;
}
#endif
extern int sysctl_memleak_scan;
/**
 * kmemleak_alloc - register a newly allocated object
 * @ptr:	pointer to beginning of the object
 * @size:	size of the object
 * @min_count:	minimum number of references to this object. If during memory
 *		scanning a number of references less than @min_count is found,
 *		the object is reported as a memory leak. If @min_count is 0,
 *		the object is never reported as a leak. If @min_count is -1,
 *		the object is ignored (not scanned and not reported as a leak)
 * @gfp:	kmalloc() flags used for kmemleak internal memory allocations
 *
 * This function is called from the kernel allocators when a new object
 * (memory block) is allocated (memleak_kmem_cache_alloc, kmalloc, vmalloc etc.).
 */
static void kmemleak_alloc(const void *ptr, int size, int type, void *cachep) {
	int min_count = 1;

	if (atomic_read(&kmemleak_enabled) && ptr && !IS_ERR(ptr)){
		create_object((unsigned long) ptr, size, type, min_count);
	}else{
		return;
	}

	if (memleak_serious_bug==1){
		printf("ERROR : inside the create\n");
		kmemleak_scan();
	}
#if 0
    if (sysctl_memleak_scan==2){
    	sysctl_memleak_scan=0;
		kmemleak_scan();
    }
#endif
}

/**
 * kmemleak_free - unregister a previously registered object
 * @ptr:	pointer to beginning of the object
 *
 * This function is called from the kernel allocators when an object (memory
 * block) is freed (memleak_kmem_cache_free, kfree, vfree etc.).
 */
static void kmemleak_free(const void *ptr, void *cachep) {

	if (atomic_read(&kmemleak_enabled) && ptr)
		delete_object((unsigned long) ptr);
	else
		return;

	if (memleak_serious_bug==1){
		printf("ERROR:    inside the delete\n");
		kmemleak_scan();
	}
#if 0
    if (sysctl_memleak_scan==2){
    	sysctl_memleak_scan=0;
		kmemleak_scan();
    }
#endif
}
static void kmemleak_update(const void *ptr,unsigned long type) {
	struct kmemleak_object *object;
	unsigned long flags;
	if (!atomic_read(&kmemleak_enabled)){
		return;
	}
	spin_lock_irqsave(&kmemleak_lock, flags);/* global lock : update */
	object = _lookup_object((unsigned long)ptr, 0);
	if (object!= 0) {
	   sanity_check(object,1);
       object->type =type;
	}else{
		MM_BUG(4);
	}
	spin_unlock_irqrestore(&kmemleak_lock, flags);


	if (memleak_serious_bug==1){
		printf("ERROR: inside the update\n");
		kmemleak_scan();
	}
	return;
}

/*
 * Scan a memory block (exclusive range) for valid pointers and add those
 * found to the gray list.
 */
static void _scan_block(void *_start, void *_end,
		struct kmemleak_object *scanned, int allow_resched) {
	unsigned char *ptr;
//	unsigned long *start = PTR_ALIGN(_start, BYTES_PER_POINTER);
	unsigned long *start,*p_long;
	unsigned long *end = _end - (BYTES_PER_POINTER - 1);

	start = (unsigned long *)(((long) _start / 8) * 8);
	for (ptr = (unsigned char*)start; ptr < (unsigned char *)end; ptr=ptr+4) {
		struct kmemleak_object *object;
		unsigned long pointer;

#if 0
		/* don't scan uninitialized memory */
		if (!kmemcheck_is_obj_initialized((unsigned long)ptr,
						BYTES_PER_POINTER))
		continue;
#endif
		p_long=(unsigned long*)ptr;
		pointer = *p_long;

		object = _lookup_object(pointer, 1);
		if (!object)
			continue;
		if (object == scanned) {
			continue;
		}

		/*
		 * Avoid the lockdep recursive warning on object->lock being
		 * previously acquired in scan_object(). These locks are
		 * enclosed by scan_mutex.
		 */

		if (!color_white(object)) {
			continue;
		}

		/*
		 * Increase the object's reference count (number of pointers
		 * to the memory block). If this count reaches the required
		 * minimum, the object's color will become gray and it will be
		 * added to the gray_list.
		 */
		object->count++;
		if (color_gray(object)) {
			list_add_tail(&object->gray_list, &gray_list);
			continue;
		}

	}
}
static void scan_block(void *_start, void *_end,
		struct kmemleak_object *scanned, int allow_resched) {

	if ((_start < (void *)&dummy_start_uninitilized_var) && ((void *)&dummy_end_uninitilized_var < _end)){
		_scan_block(_start, &dummy_start_uninitilized_var, scanned, 1);
		_scan_block(&dummy_end_uninitilized_var, _end, scanned, 1);
	}else{
	    _scan_block(_start, _end, scanned, 1);
	}
	return;
}
/*
 * Scan a memory block corresponding to a kmemleak_object. A condition is
 * that object->use_count >= 1.
 */
static void scan_object(struct kmemleak_object *object) {
	/*
	 * Once the object->lock is acquired, the corresponding memory block
	 * cannot be freed (the same lock is acquired in delete_object).
	 */
	if (object->flags & OBJECT_NO_SCAN)
		goto out;
	if (!(object->flags & OBJECT_ALLOCATED)) {
		/* already freed object */
		goto out;
	}
	if (1) {
		void *start = (void *) object->pointer;
		void *end = (void *) (object->pointer + object->size);

		while (start < end && (object->flags & OBJECT_ALLOCATED)
				&& !(object->flags & OBJECT_NO_SCAN)) {
			scan_block(start, min(start + MAX_SCAN_SIZE, end), object, 0);
			start += MAX_SCAN_SIZE;
		}
	}
	out:
	return;
}

/*
 * Scan the objects already referenced (gray objects). More objects will be
 * referenced and, if there are no memory leaks, all the objects are scanned.
 */
static void scan_gray_list(void) {
	struct kmemleak_object *object, *tmp;
	/*
	 * The list traversal is safe for both tail additions and removals
	 * from inside the loop.
	 */
	object = list_entry(gray_list.next, typeof(*object), gray_list);
	while (&object->gray_list != &gray_list) {

		scan_object(object);
		tmp = list_entry(object->gray_list.next, typeof(*object),
				gray_list);

		/* remove the object from the list and release it */
		list_del(&object->gray_list);

		object = tmp;
	}
	WARN_ON(!list_empty(&gray_list));
}

/*
 * Scan data sections and all the referenced memory blocks allocated via the
 * kernel's standard allocators. This function must be called with the
 * scan_mutex held.
 */

void kmemleak_scan(void) {
	static int scanned = 0;
	struct kmemleak_object *object;
	unsigned long flags;
	int new_leaks = 0;
	int total_obj = 0;
	int i,k;

    if (scanned==1) return ;
	printf("sanity location:%d  seriousbug:%d  arg1: %x arg2:%x total object count:%d\n", sanity_location,memleak_serious_bug, err_arg1,err_arg2,stat_obj_count);


	if (sanity_location != 0 || memleak_serious_bug==1) {
		atomic_set(&kmemleak_enabled, 0);
		memleakHook_disable();
	//	goto out;
	}

	scanned=1;

	spin_lock_irqsave(&kmemleak_lock, flags); /* global lock: scan to freeze entire memory*/
	/* global lock , this is to freeze memory while scanning */
	/* prepare the kmemleak_object's */
	list_for_each_entry(object, &object_list, object_list) {
		/* reset the reference count (whiten the object) */
		if (object == 0) {
			spin_unlock_irqrestore(&kmemleak_lock, flags);
			pr_debug("ERROR : object null- scanning aborted\n");
			return;
		}
		object->count = 0;
		total_obj++;
	}

	dummy_start_uninitilized_var = 0;
	dummy_end_uninitilized_var = 0;
	/* data/bss scanning */
	scan_block(data_region_start, data_region_end + 8, NULL, 1);
	/*
	 * Scan the objects already referenced from the sections scanned
	 * above.
	 */
	scan_gray_list();

	scan_gray_list();

	int totaltypes = 0;
	int withtype = 0;
	int total_mem=0;
	list_for_each_entry(object, &object_list, object_list) {

		int found = 0;

		for (i = 0; i < totaltypes && i < MAX_TYPES; i++) {
			if (obj_types[i].type == object->type) {
				obj_types[i].count++;
				obj_types[i].mem_consumed = obj_types[i].mem_consumed
						+ object->size;
				if (object->count == 0)
					obj_types[totaltypes].mem_leak = obj_types[totaltypes].mem_leak+object->size;
				found = 1;
				break;
			}
		}
		if (found == 0 && (totaltypes < (MAX_TYPES - 1))) {
			if (object->count == 0){
				obj_types[totaltypes].mem_leak = object->size;
				for (k = 0; k < MAX_TRACE; k++)
								obj_types[totaltypes].trace[k] = object->trace[k];
			}
			obj_types[totaltypes].type = object->type;
			obj_types[totaltypes].count = 1;
			obj_types[totaltypes].mem_consumed = object->size;
			totaltypes++;
		}
		if (object->type != 0)
			withtype++;
		if (object->count == 0){
			pr_debug(" Leak Object addr: %x size:%d trace:",object->pointer,object->size);
			for (k = 0; k < MAX_TRACE; k++){
				pr_debug(" %x ",object->trace[k]);
			}
			pr_debug("\n");
			new_leaks++;
		}

		total_mem = total_mem + object->size;
	}
	spin_unlock_irqrestore(&kmemleak_lock, flags); /* unlock the global lock */



	for (i = 0; (i < totaltypes) && (i < MAX_TYPES); i++) {
		if (obj_types[i].type != 0)
			pr_debug(" cnt:%d mem:%d K leak:%d type:%s  trace:%x-%x-%x-%x\n",
					obj_types[i].count, obj_types[i].mem_consumed/1024, obj_types[i].mem_leak, obj_types[i].type, obj_types[i].trace[0], obj_types[i].trace[1],obj_types[i].trace[2],obj_types[i].trace[3]);
		else
			pr_debug("cnt:%d mem:%d K leak:%d type:NOTYPE  trace:%x-%x-%x-%x\n", obj_types[i].count, obj_types[i].mem_consumed/1024, obj_types[i].mem_leak, obj_types[i].trace[0],obj_types[i].trace[1],obj_types[i].trace[2],obj_types[i].trace[3]);

	}
	pr_debug("New2.0 Total Leaks : %d  total obj:%d stat_total:%d withtype:%d totalmem:%d K\n",
			new_leaks, total_obj, stat_obj_count, withtype,total_mem/1024);

	for (i = 0; i < MAX_ERRORS; i++)
		if (stat_errors[i] != 0)
			pr_debug("memleak error-%d  : %d\n", i, stat_errors[i]);
}

extern int memleakHook_copyFromEarlylog(void (*palloc)(const void *ptr, int size, int type, void *cachep), void (*pfree)(const void *ptr, void *cachep),void (*pupdate)(const void *ptr, unsigned long type));
/*
 * Kmemleak initialization.
 */
void kmemleak_init(void) {
	unsigned long flags;
	int i;

	pr_debug("kmemleak init version 2.16 :%x \n",object_cache);
	prio_tree_init();
	stat_obj_count=0;
	for (i=0; i<MAX_ERRORS; i++)
	   stat_errors[i]=0;

	object_cache = memleak_kmem_cache_create("kmemleak_objects",
			sizeof(struct kmemleak_object), 0, 0, 0, 0);
	scan_area_cache = memleak_kmem_cache_create("kmemleak_scan_area",
			sizeof(struct kmemleak_scan_area), 0, 0, 0, 0);
	INIT_PRIO_TREE_ROOT(&object_tree_root);

	for (i=0; i<MAX_HASH;i++){
		//hash_table[i]=0x0;
		bhash_table[i]=0x0;
	}

	/* the kernel is still in UP mode, so disabling the IRQs is enough */
	local_irq_save(flags);
	if (atomic_read(&kmemleak_error)) {
		local_irq_restore(flags);
		return;
	} else
		atomic_set(&kmemleak_enabled, 1);
	local_irq_restore(flags);

	pr_debug("Memleak : Start copying back log object\n");
	if (memleakHook_copyFromEarlylog(kmemleak_alloc, kmemleak_free, kmemleak_update)==0){
		atomic_set(&kmemleak_enabled, 0);
	}
	pr_debug("Init kmemleak code: %x-%x data: %x-%x  OBJECT CACHE:%x\n",code_region_start,data_region_start,data_region_start,data_region_end,object_cache);
	pr_debug("Init kmemleak max objects:%d total objects:%d  object size:%d\n",MAX_STATIC_OBJS,stat_obj_count,sizeof(struct kmemleak_object));
}

