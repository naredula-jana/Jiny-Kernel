#ifndef __MM_H__
#define __MM_H__

#include "common.h"
#include "paging.h"
#include "bitops.h"
#include "../util/host_fs/filecache_schema.h"
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE-1))
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)
#define __PAGE_OFFSET           (0x0) /* TODO   changed to  #define __PAGE_OFFSET           (0xC0000000) */
#define PAGE_OFFSET             ((unsigned long)__PAGE_OFFSET)
#define __pa(x)                 ((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)                 ((void *)((unsigned long)(x)+PAGE_OFFSET))
#define MAP_NR(addr)            (__pa(addr) >> PAGE_SHIFT)

#define virt_to_page(kaddr)	(g_mem_map + (__pa(kaddr) >> PAGE_SHIFT))
/*
 * vm_flags..
 */
#define VM_READ         0x00000001      /* currently active flags */
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008

#define PROT_READ       0x1             /* page can be read */
#define PROT_WRITE      0x2             /* page can be written */
#define PROT_EXEC       0x4             /* page can be executed */
#define PROT_NONE       0x0             /* page can not be accessed */

#define MAP_SHARED      0x01            /* Share changes */
#define MAP_PRIVATE     0x02            /* Changes are private */
#define MAP_TYPE        0x0f            /* Mask for type of mapping */
#define MAP_FIXED       0x10            /* Interpret addr exactly */
#define MAP_ANONYMOUS   0x20            /* don't use a file */
#define MAP_32BIT       0x40            /* only give out 32bit addresses */

struct vm_area_struct {
        struct mm_struct *vm_mm;       /* The address space we belong to. */
        unsigned long vm_start;         /* Our start address within vm_mm. */
        unsigned long vm_end;           /* The first byte after our end address
                                           within vm_mm. */

        /* linked list of VM areas per task, sorted by address */
        struct vm_area_struct *vm_next;

        unsigned long vm_prot;          /* Access permissions of this VMA. */
        unsigned long vm_flags;         /* Flags, listed below. */

        /* Information about our backing store: */
        unsigned long vm_pgoff;         /* Offset (within vm_file) in PAGE_SIZE
                                           units, *not* PAGE_CACHE_SIZE */
        struct file *vm_file;          /* File we map to (can be NULL). */
        void * vm_private_data;         /* was vm_pte (shared mem) */
};

typedef struct page {
        /* these must be first (free area handling) */
        struct page *next;
        struct page *prev;

        atomic_t count;
        unsigned long flags;    /* atomic flags, some possibly updated asynchronously */

	unsigned int age;

	struct inode *inode;
	unsigned long offset; /* offset in the inode */
	struct list_head lru_list;          /* LRU list: the page can be in freelist,active or inactive in of the list   */
	unsigned char list_type ; /* LRU list # is stored */
	struct list_head list;          /*TODO: currently used 1)  SLAB 2) pagecache:inodelist  */
} page_struct_t;

#define ADDR_LIST_MAX 50
struct addr_list {
	unsigned long addr[ADDR_LIST_MAX];
	int total;
};
extern page_struct_t *g_mem_map;

typedef struct kmem_cache_s kmem_cache_t;
/* SLAB cache for vm_area_struct structures */
extern kmem_cache_t *vm_area_cachep;

/* SLAB cache for mm_struct structures (tsk->mm) */
extern kmem_cache_t *mm_cachep;
extern page_struct_t *pagecache_map;
extern unsigned char *pc_startaddr;

/* Page flag bit values */
#define PG_locked                0
#define PG_error                 1
#define PG_referenced            2
#define PG_dirty                 3
#define PG_uptodate              4
#define PG_free_after            5
#define PG_decr_after            6
#define PG_swap_unlock_after     7
#define PG_DMA                   8
#define PG_slab                  9
#define PG_swap_cache           10
#define PG_skip                 11
#define PG_reserved             31

#define PageReserved(page)      (test_bit(PG_reserved, &(page)->flags))
#define PageSwapCache(page)     (test_bit(PG_swap_cache, &(page)->flags))
#define PageDMA(page)           (test_bit(PG_DMA, &(page)->flags))
#define PageClearSlab(page)	(clear_bit(PG_slab, &(page)->flags))
#define PageSetSlab(page)	set_bit(PG_slab, &(page)->flags)
#define PageDirty(page)      (test_bit(PG_dirty, &(page)->flags))
#define PageSetDirty(page)      set_bit(PG_dirty, &(page)->flags)
#define PageClearDirty(page)    clear_bit(PG_dirty, &(page)->flags)

#endif
