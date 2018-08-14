/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   include/mm.h
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#ifndef __MM_H__
#define __MM_H__


#include "paging.h"
#include "bitops.h"
#include "../util/host_fs/filecache_schema.h"
#define PAGE_MASK       (~(PAGE_SIZE-1))
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

typedef struct jcache_s jcache_t;
#define JSLAB_FLAGS_DEBUG 0x10000
#ifdef JINY_SLAB
#if 0
jcache_t *
jslab_create_cache (const uint8_t *name, size_t size, size_t offset,
	unsigned long flags, void (*ctor)(void*, jcache_t *, unsigned long),
	void (*dtor)(void*, jcache_t *, unsigned long));
#endif
#define kmem_cache_t jcache_t
#define mm_slab_cache_alloc jslab_alloc_from_cache
#define mm_malloc jslab_alloc_from_predefined
#define mm_slab_cache_free jslab_free1
#define mm_free jslab_free2
#define kmem_cache_create jslab_create_cache
#define kmem_cache_destroy jslab_destroy_cache
#else
typedef struct kmem_cache_s kmem_cache_t;
#endif


/* kerne virtual address space */
#define KADDRSPACE_START       (0xffffc90000000000)
#define KERNEL_CODE_START      (0xffffffff80000000) /* Note This should be multiples 1GB , otherwise page tables copying will break */
#define KADDRSPACE_END         (0xffffffffc0000000) /*  */
#define HIGHPRIORITY_APP_START (0xffffffffb0000000)
#define HIGHPRIORITY_APP_LEN 0x100000

#define HIGHPRIORITY_APP_SYSCALLTABLE 0x3DD000
//#define HIGHPRIORITY_APP_SYSCALLTABLE 0x800000

extern unsigned long g_kernelspace_starting_address;


#define  __va(addr)  (unsigned long)({ (g_kernelspace_starting_address == 1) ? ((unsigned long)(addr)+KERNEL_CODE_START) : ((unsigned long)(addr)+g_kernelspace_starting_address) ;})
#define  __pa(addr)  (unsigned long)({ (g_kernelspace_starting_address == 1) ? ((unsigned long)(addr)-KERNEL_CODE_START) : ((addr > KERNEL_CODE_START) ? ((unsigned long)(addr)-KERNEL_CODE_START) : ((unsigned long)(addr)-g_kernelspace_starting_address)) ;})


#define USERANONYMOUS_ADDR 0x7fff00000000
#define USERSTACK_ADDR     0x7fffdbaac000
#define USERSTACK_LEN  0x100000
#define USER_SYSCALL_PAGE 0xffffffffff600000
#define USER_FSYNC_SHM_PAGE 0xffffffffff700000


#define MAP_NR(addr)            (__pa(addr) >> PAGE_SHIFT)

#define virt_to_page(kaddr)	(g_mem_map + (__pa(kaddr) >> PAGE_SHIFT))
#define page_to_virt(page) (__va(((page_struct_t *)page - (page_struct_t *)g_mem_map)<<PAGE_SHIFT))
/*
 * vm_flags..
 */
#define VM_READ         0x00000001      /* currently active flags */
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008

#define MEM_CLEAR      0x8000000
#define MEM_FOR_CACHE  0x4000000
#define MEM_NETBUF     0x0200000
//#define MEM_FOR_GLOBAL 0x2000000  /* pages for global , not allocated from task context */
#define MEM_NETBUF_SIZE 16384  /* 4096*4 =

/* protection flags matches to that elf flags */
#define PROT_READ       0x4             /* page can be read */
#define PROT_WRITE      0x2             /* page can be written */
#define PROT_EXEC       0x1             /* page can be executed */
#define PROT_NONE       0x0             /* page can not be accessed */

#define MAP_SHARED      0x01            /* Share changes */
#define MAP_PRIVATE     0x02            /* Changes are private */
#define MAP_TYPE        0x0f            /* Mask for type of mapping */
#define MAP_FIXED       0x10            /* Interpret addr exactly */
#define MAP_ANONYMOUS   0x20            /* don't use a file */
#define MAP_32BIT       0x40            /* only give out 32bit addresses */
#define MAP_DENYWRITE  0x800        /* deny file for writing or deleting */

#define MAP_EXECUTABLE  0x1000          /* mark it as an executable */
#define MAP_LOCKED      0x2000          /* pages are locked */
#define MAP_NORESERVE   0x4000          /* don't check for reservations */
#define MAP_POPULATE    0x8000          /* populate (prefault) pagetables */
#define MAP_NONBLOCK    0x10000         /* do not block on IO */
#define MAP_STACK       0x20000         /* give out an address that is best suited for process/thread stacks */
#define MAP_HUGETLB     0x40000         /* create a huge page mapping */

/***********************************
  
  page->inode<-vma->mm
  when vma deleted :
      1) ptes are cleaned
      2) vma node is deleted from inode list

  when page is deleted :
	1) deleted from inode list
	2) scan the vmas coreesponding to inode, for each  vma clear pte corresponding to inode
	3) removde from lru list 

 

************************************/
#define MAX_MMAP_LOG_SIZE 10
struct vm_area_struct {
	struct mm_struct *vm_mm; /* The address space we belong to. */
	unsigned long vm_start; /* Our start address within vm_mm. */
	unsigned long vm_end; /* The first byte after our end address  within vm_mm. */
	const char *name;

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next;

	unsigned long vm_prot; /* Access permissions of this VMA. */
	unsigned long vm_flags; /* Flags, listed below. */

	/* Information about our backing store: */
	unsigned long vm_pgoff; /* Offset (within vm_file) in PAGE_SIZE
	 units, *not* PAGE_CACHE_SIZE */
	void *vm_inode; /* File we map to (can be NULL). */
	unsigned long vm_private_data; /* was vm_pte (shared mem) */
	struct list_head inode_vma_link; /* vmas connected to inode */
	int hugepages_enabled; /* set this flag if enabled */

	int stat_page_faults,stat_page_wrt_faults;
	int stat_page_count;
	int stat_log_index;
	struct {
		unsigned long vaddr;
		unsigned long fault_addr;
		unsigned long optional;
		unsigned char rw_flag;
	}stat_log[MAX_MMAP_LOG_SIZE+1];
};
#define MEMORY_DEBUG 1
#define MAX_BACKTRACE_LENGTH 20
#define PAGE_MAGIC 0xabab123456
typedef struct page {
	/* these must be first (free area handling) */
	struct page *next;
	struct page *prev;

	atomic_t count;
	unsigned long flags; /* atomic flags, some possibly updated asynchronously */

	unsigned int age; /* youngest =1 or eldest =100 */
	unsigned int large_page_size; /* used for large page or contegous pages */

	void *fs_inode; /* overloaded used by slab for large page implementataion */
	uint64_t offset; /* offset in the inode */
	struct list_head lru_link; /* LRU list: the page can be in freelist,active or inactive in of the list   */
	unsigned char list_type; /* LRU list # is stored */
	struct list_head list; /*TODO: currently used 1)  SLAB 2) pagecache:inodelist  */
#ifdef MEMORY_DEBUG
	unsigned long bt_addr_list[MAX_BACKTRACE_LENGTH];
	unsigned long option_data;
#endif
	unsigned long magic_number;
} page_struct_t;

#define ADDR_LIST_MAX 500
struct addr_list {
	unsigned long addr[ADDR_LIST_MAX];
	int total;
};
extern page_struct_t *g_mem_map;

/* SLAB cache for vm_area_struct structures */
extern kmem_cache_t *vm_area_cachep;

/* SLAB cache for mm_struct structures (tsk->mm) */
extern kmem_cache_t *mm_cachep;
extern page_struct_t *pagecache_map;
extern unsigned char *pc_startaddr;
extern unsigned char *pc_endaddr;

extern struct mm_struct *g_kernel_mm;
#define is_pc_paddr(paddr) (paddr>__pa(pc_startaddr) && paddr<=__pa(pc_endaddr))
/* Page flag bit values */
#define PG_locked                0
#define PG_error                 1
#define PG_referenced            2
#define PG_dirty                 3
#define PG_uptodate              4
#define PG_DMA                   8
#define PG_slab                  9
#define PG_swap_cache           10
#define PG_skip                 11
#define PG_large_page           12
#define PG_readinprogress       13
#define PG_netbuf               14  /* these pages are used for network buffer */
#define PG_reserved             31

#define PageReserved(page)      (test_bit(PG_reserved, &(page)->flags))
#define PageReferenced(page)      (test_bit(PG_referenced, &(page)->flags))
#define PageSwapCache(page)     (test_bit(PG_swap_cache, &(page)->flags))
#define PageDMA(page)           (test_bit(PG_DMA, &(page)->flags))
#define PageSlab(page)           (test_bit(PG_slab, &(page)->flags))
#define PageLargePage(page)           (test_bit(PG_large_page, &(page)->flags))
#define PageDirty(page)      (test_bit(PG_dirty, &(page)->flags))
#define PageNetBuf(page)      (test_bit(PG_netbuf, &(page)->flags))
#define PageReadinProgress(page)      (test_bit(PG_readinprogress, &(page)->flags))

#define PageSetSlab(page)	set_bit(PG_slab, &(page)->flags)
#define PageSetNetBuf(page)	set_bit(PG_netbuf, &(page)->flags)
#define PageSetLargePage(page)	set_bit(PG_large_page, &(page)->flags)
#define PageSetDirty(page)      set_bit(PG_dirty, &(page)->flags)
#define PageSetReferenced(page)      (set_bit(PG_referenced, &(page)->flags))
#define PageSetReadinProgress(page)      (set_bit(PG_readinprogress, &(page)->flags))

#define PageClearReferenced(page)      (clear_bit(PG_referenced, &(page)->flags))
#define PageClearDirty(page)    clear_bit(PG_dirty, &(page)->flags)
#define PageClearNetBuf(page)    clear_bit(PG_netbuf, &(page)->flags)
#define PageClearSlab(page)	(clear_bit(PG_slab, &(page)->flags))
#define PageClearLargePage(page)	(clear_bit(PG_large_page, &(page)->flags))
#define PageClearReadinProgress(page)      (clear_bit(PG_readinprogress, &(page)->flags))

static inline unsigned char *pcPageToPtr(struct page *p) {
	unsigned char *addr;
	unsigned long pn;
	pn = p - pagecache_map;
	addr = pc_startaddr + pn * PC_PAGESIZE;
	return addr;
}

#endif
