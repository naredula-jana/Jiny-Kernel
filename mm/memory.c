#include "mm.h"
#include "interface.h"
int g_nr_swap_pages = 0;
int g_nr_free_pages = 0;

/*
 * Free area management
 *
 * The g_free_area_list arrays point to the queue heads of the free areas
 * of different sizes
 */


#define NR_MEM_LISTS 6


/* The start of this MUST match the start of "struct page" */
struct g_free_area_struct {
	struct page *next;
	struct page *prev;
	unsigned int * map;
	unsigned int stat_count; /* jana Added */
};
page_struct_t * g_mem_map = NULL;
unsigned long g_max_mapnr=0;

#define memory_head(x) ((struct page *)(x))

static struct g_free_area_struct g_free_area[NR_MEM_LISTS];

static inline void init_mem_queue(struct g_free_area_struct * head)
{
	head->next = memory_head(head);
	head->prev = memory_head(head);
	head->stat_count=0;
}

static inline void add_mem_queue(struct g_free_area_struct * head, struct page * entry)
{
	struct page * next = head->next;

	entry->prev = memory_head(head);
	entry->next = next;
	next->prev = entry;
	head->next = entry;
	head->stat_count++;
}

static inline void remove_mem_queue(struct g_free_area_struct *area,struct page * entry)
{
	struct page * next = entry->next;
	struct page * prev = entry->prev;
	next->prev = prev;
	prev->next = next;
	area->stat_count--;
}

/*
 * Free_page() adds the page to the free lists. This is optimized for
 * fast normal cases (no error jumps taken normally).
 *
 * The way to optimize jumps for gcc-2.2.2 is to:
 *  - select the "normal" case and put it inside the if () { XXX }
 *  - no else-statements if you can avoid them
 *
 * With the above two rules, you get a straight-line execution path
 * for the normal case, giving better asm-code.
 */

/*
 * Buddy system. Hairy. You really aren't expected to understand this
 *
 * Hint: -mask = 1+~mask
 */
spinlock_t g_page_alloc_lock = SPIN_LOCK_UNLOCKED;

static inline void free_pages_ok(unsigned long map_nr, unsigned long order)
{
	struct g_free_area_struct *area = g_free_area + order;
	unsigned long index = map_nr >> (1 + order);
	unsigned long mask = (~0UL) << order;
	unsigned long flags;

	spin_lock_irqsave(&g_page_alloc_lock, flags);

#define list(x) (g_mem_map+(x))

	map_nr &= mask;
	g_nr_free_pages -= mask;
	while (mask + (1 << (NR_MEM_LISTS-1))) {
		if (!test_and_change_bit(index, area->map))
			break;
		remove_mem_queue(area,list(map_nr ^ -mask));
		mask <<= 1;
		area++;
		index >>= 1;
		map_nr &= mask;
	}
	add_mem_queue(area, list(map_nr));

#undef list

	spin_unlock_irqrestore(&g_page_alloc_lock, flags);
}
/*
static void __free_page(struct page *page)
{
	if (!PageReserved(page) && atomic_dec_and_test(&page->count)) {
		if (PageSwapCache(page))
			ut_printf ("PANIC : Freeing swap cache page");
		page->flags &= ~(1 << PG_referenced);
		free_pages_ok(page - g_mem_map, 0);
		return;
	}
} */

void mm_putFreePages(unsigned long addr, unsigned long order)
{
	unsigned long map_nr = MAP_NR(addr);

	if (map_nr < g_max_mapnr) {
		page_struct_t * map = g_mem_map + map_nr;
		if (PageReserved(map))
			return;
		if (atomic_dec_and_test(&map->count)) {
			if (PageSwapCache(map))
				ut_printf ("PANIC Freeing swap cache pages");
			map->flags &= ~(1 << PG_referenced);
			free_pages_ok(map_nr, order);
			return;
		}
	}
}

/*
 * Some ugly macros to speed up mm_getFreePages()..
 */
#define MARK_USED(index, order, area) \
	change_bit((index) >> (1+(order)), (area)->map)
#define CAN_DMA(x) (PageDMA(x))
#define ADDRESS(x) (PAGE_OFFSET + ((x) << PAGE_SHIFT))

#define RMQUEUE(order, gfp_mask) \
	do { struct g_free_area_struct * area = g_free_area+order; \
		unsigned long new_order = order; \
		do { struct page *prev = memory_head(area), *ret = prev->next; \
			while (memory_head(area) != ret) { \
				if ( CAN_DMA(ret)) { \
					unsigned long map_nr; \
					(prev->next = ret->next)->prev = prev; \
					map_nr = ret - g_mem_map; \
					MARK_USED(map_nr, new_order, area); \
					g_nr_free_pages -= 1 << order; \
					EXPAND(ret, map_nr, order, new_order, area); \
					spin_unlock_irqrestore(&g_page_alloc_lock, flags); \
					return ADDRESS(map_nr); \
				} \
				prev = ret; \
				ret = ret->next; \
			} \
			new_order++; area++; \
		} while (new_order < NR_MEM_LISTS); \
	} while (0)

#define EXPAND(map,index,low,high,area) \
	do { unsigned long size = 1 << high; \
		while (high > low) { \
			area--; high--; size >>= 1; \
			add_mem_queue(area, map); \
			MARK_USED(index, high, area); \
			index += size; \
			map += size; \
		} \
		atomic_set(&map->count, 1); \
	} while (0)

int low_on_memory = 0;

unsigned long mm_getFreePages(int gfp_mask, unsigned long order)
{
	unsigned long flags;

	if (order >= NR_MEM_LISTS)
		goto nopage;

#ifdef ATOMIC_MEMORY_DEBUGGING
	if ((gfp_mask & __GFP_WAIT) && in_interrupt()) {
		static int count = 0;
		if (++count < 5) {
			printk("gfp called nonatomically from interrupt %p\n",
					__builtin_return_address(0));
		}
		goto nopage;
	}
#endif

	/*
	 * If this is a recursive call, we'd better
	 * do our best to just allocate things without
	 * further thought.
	 */
	/* jana_TODO if (!(current->flags & PF_MEMALLOC)) {
	   int freed;

	   if (g_nr_free_pages > freepages.min) {
	   if (!low_on_memory)
	   goto ok_to_allocate;
	   if (g_nr_free_pages >= freepages.high) {
	   low_on_memory = 0;
	   goto ok_to_allocate;
	   }
	   }

	   low_on_memory = 1;
	   current->flags |= PF_MEMALLOC;
	   freed = try_to_free_pages(gfp_mask);
	   current->flags &= ~PF_MEMALLOC;

	   if (!freed && !(gfp_mask & (__GFP_MED | __GFP_HIGH)))
	   goto nopage;
	   }*/
//ok_to_allocate:
	spin_lock_irqsave(&g_page_alloc_lock, flags);
	RMQUEUE(order, gfp_mask);
	spin_unlock_irqrestore(&g_page_alloc_lock, flags);

	/*
	 * If we can schedule, do so, and make sure to yield.
	 * We may be a real-time process, and if kswapd is
	 * waiting for us we need to allow it to run a bit.
	 */
	/* JANA_TODO	if (gfp_mask & __GFP_WAIT) {
	   current->policy |= SCHED_YIELD;
	   sc_schedule();
	   }*/

nopage:
	return 0;
}

/*
 * Show free area list (used inside shift_scroll-lock stuff)
 * We also calculate the percentage fragmentation. We do this by counting the
 * memory on each free list with the exception of the first item on the list.
 */
void mm_printFreeAreas(void)
{
	unsigned long order, flags;
	unsigned long total = 0;


	spin_lock_irqsave(&g_page_alloc_lock, flags);
	for (order=0 ; order < NR_MEM_LISTS; order++) {
		struct page * tmp;
		unsigned long nr = 0;
		for (tmp = g_free_area[order].next ; tmp != memory_head(g_free_area+order) ; tmp = tmp->next) {
			nr ++;
		}
		total += nr * ((PAGE_SIZE>>10) << order);
		ut_printf("%d: count:%d  static count:%d\n", order,nr,g_free_area[order].stat_count);
	}
	spin_unlock_irqrestore(&g_page_alloc_lock, flags);
	ut_printf("total = %x\n", total);
}

#define LONG_ALIGN(x) (((x)+(sizeof(long))-1)&~((sizeof(long))-1))

/*
 * set up the free-area data structures:
 *   - mark all pages reserved
 *   - mark all memory queues empty
 *   - clear the memory bitmaps
 */
unsigned long init_free_area(unsigned long start_mem, unsigned long end_mem)
{
	page_struct_t * p;
	unsigned long mask = PAGE_MASK;
	unsigned long i;

	/*
	 * Select nr of pages we try to keep free for important stuff
	 * with a minimum of 10 pages and a maximum of 256 pages, so
	 * that we don't waste too much memory on large systems.
	 * This is fairly arbitrary, but based on some behaviour
	 * analysis.
	 */
	ut_printf(" Beforeut_memset start_mem: %x endmem:%x   \n",start_mem,end_mem);
	i = (end_mem - PAGE_OFFSET) >> (PAGE_SHIFT+7);
	if (i < 10)
		i = 10;
	if (i > 256)
		i = 256;
	/*TODO freepages.min = i;
	  freepages.low = i * 2;
	  freepages.high = i * 3;*/
	g_mem_map = (page_struct_t *) LONG_ALIGN(start_mem+8);
	p = g_mem_map + MAP_NR(end_mem);
	start_mem = LONG_ALIGN((unsigned long) p);
	ut_printf(" Beforeut_memset mem map: %x diff:%x   \n",g_mem_map,(start_mem -(unsigned long) g_mem_map));
	ut_memset((unsigned char *)g_mem_map, 0, start_mem -(unsigned long) g_mem_map);
	do {
		--p;
		atomic_set(&p->count, 0);
		p->flags = (1 << PG_DMA) | (1 << PG_reserved);
	} while (p > g_mem_map);

	for (i = 0 ; i < NR_MEM_LISTS ; i++) {
		unsigned long bitmap_size;
		init_mem_queue(g_free_area+i);
		mask += mask;
		end_mem = (end_mem + ~mask) & mask;
		bitmap_size = (end_mem - PAGE_OFFSET) >> (PAGE_SHIFT + i);
		bitmap_size = (bitmap_size + 7) >> 3;
		bitmap_size = LONG_ALIGN(bitmap_size);
		g_free_area[i].map = (unsigned int *) start_mem;
		ut_memset((void *) start_mem, 0, bitmap_size);
		start_mem += bitmap_size;
	}
	return start_mem;
}

void init_mem(unsigned long start_mem, unsigned long end_mem)
{
	//unsigned long start_low_mem = PAGE_SIZE;
	int reservedpages = 0;
	/*int codepages = 0;
	int datapages = 0;
	int initpages = 0; */
	unsigned long tmp;

	end_mem &= PAGE_MASK;
	g_max_mapnr  = MAP_NR(end_mem);

	/* clear the zero-page */
	//ut_memset(empty_zero_page, 0, PAGE_SIZE);

	/* mark usable pages in the g_mem_map[] */
	//start_low_mem = PAGE_ALIGN(start_low_mem)+PAGE_OFFSET;


	start_mem = PAGE_ALIGN(start_mem);

	/*
	 * IBM messed up *AGAIN* in their thinkpad: 0xA0000 -> 0x9F000.
	 * They seem to have done something stupid with the floppy
	 * controller as well..
	 */
	/*while (start_low_mem < 0x9f000+PAGE_OFFSET) {
	  clear_bit(PG_reserved, &g_mem_map[MAP_NR(start_low_mem)].flags);
	  start_low_mem += PAGE_SIZE;
	  }*/

	while (start_mem < end_mem) {
		clear_bit(PG_reserved, &g_mem_map[MAP_NR(start_mem)].flags);
		start_mem += PAGE_SIZE;
	}
	for (tmp = PAGE_OFFSET ; tmp < (end_mem - 0x2000) ; tmp += PAGE_SIZE) {
		/*if (tmp >= MAX_DMA_ADDRESS)
		  clear_bit(PG_DMA, &g_mem_map[MAP_NR(tmp)].flags);*/
		if (PageReserved(g_mem_map+MAP_NR(tmp))) {
			reservedpages++;
			continue;
		}
		atomic_set(&g_mem_map[MAP_NR(tmp)].count, 1);
		mm_putFreePages(tmp,0);

	}
	ut_printf(" Release to FREEMEM : %x \n",(end_mem - 0x2000));


}
extern unsigned long g_multiboot_mod_addr;
extern unsigned long g_multiboot_mod_len;
void init_memory(addr_t end_addr)
{
	addr_t start_addr;

	ut_printf(" Initializing memory endaddr : %x \n",end_addr);
	start_addr=initialise_paging( end_addr);
	if (g_multiboot_mod_len > 0) /* symbol file  reside at the end of memory, it can acess only when page table is initialised */
	{
		g_symbol_table=(unsigned char *)start_addr;
		g_total_symbols=(g_multiboot_mod_len)/sizeof(symb_table_t);
		ut_memcpy((unsigned char *)g_symbol_table,(unsigned char *)g_multiboot_mod_addr,g_multiboot_mod_len);
		start_addr=(unsigned long)start_addr+g_multiboot_mod_len;
		ut_printf(" symbol:  %x : %d  :%d :%x : \n", g_symbol_table,g_total_symbols,sizeof(symb_table_t),g_symbol_table[5].address);
		ut_printf(" symbol:  %s \n",&g_symbol_table[5].name[0]);
	}

	start_addr=init_free_area( start_addr, end_addr);
	init_mem(start_addr, end_addr);
}
