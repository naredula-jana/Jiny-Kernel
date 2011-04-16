#ifndef PAGING_H
#define PAGING_H

#include "common.h"
#include "mm.h"

#define PAGE_MASK	(~(PAGE_SIZE-1))
/* pte_t represents l1 directory
   Flags bits: only * are used for now
 *0 - present bit
 *1 - R/W bit
 2- U/S
 3- PWT
 4-PCD
 5- Access bit
 6- Dirty bit (ignored for 4k page)
 *7- Page size 0-for 4k 1-for 2M
 *8- Global bit (ignored for 4k page)
 12-PAT 
 */
typedef struct pte
{
	addr_t present    : 1;   // Page present in memory
	addr_t rw         : 1;   // Read-only if clear, readwrite if set
	addr_t user       : 1;   // Supervisor level only if clear
	addr_t pwt	  :1 ;
	addr_t pcd	  :1 ;
	addr_t accessed   : 1;   // Has the page been accessed since last refresh?
	addr_t dirty      : 1;   // Has the page been written to since last refresh?
	addr_t pat	  :1 ;
	addr_t global	  :1 ;
	addr_t avl     : 3;   // available
	addr_t frame      : 40;  // Frame address (shifted right 12 bits)
	addr_t count      :11;
	addr_t nx          :1;
} pte_t;

/* pde_t represents l2 directory
   Flags bits: only * are used for now
 *0 - present bit
 *1 - R/W bit
 2- U/S
 3- PWT
 4-PCD
 5- Access bit
 6- Dirty bit (ignored for 4k page)
 *7- Page size 0-for 4k 1-for 2M
 *8- Global bit (ignored for 4k page)
 12-PAT 
 */
typedef struct __pde {
	addr_t present    : 1;   // Page present in memory
	addr_t rw         : 1;   // Read-only if clear, readwrite if set
	addr_t user       : 1;   // Supervisor level only if clear
	addr_t pwt	  :1 ;
	addr_t pcd	  :1 ;
	addr_t accessed   : 1;   // Has the page been accessed since last refresh?
	addr_t dirty      : 1;   // Has the page been written to since last refresh? ignored for 2M
	addr_t ps	  :1 ; // page size 2M=1 4k=0
	addr_t global	  :1 ; // ignored in 4k
	addr_t avl     : 3;   // available 
	/*    addr_t pat	  :1 ; */
	/*  unsigned flags      :12; */
	addr_t frame  :40;
	addr_t count      :11;
	addr_t nx          :1;
} __attribute__((packed)) pde_t;

/*typedef struct pte_table
  {
  pte_t pte[1024];
  } page_table_t; */
typedef struct pde_table /* level2 table */
{
	pde_t pde[512];
} pde_table_t;

/* 
   virtual address is 48 bits : 9+9+9+9 + 12
   entries in page table = 2 pow 9 = 512
   4 levels : l4 l3 l2 l1  for 4k pages
   else 3 levele : l4 l3 l2 for 2M pages
   pde_t is a l2  page directory

   physical addess is 52 bit long.
   any entry in the page directory is 52-12=40 bit long to get the address of next level directory

   l2 entries or PDE for 2M pages: 
   21bits + 31 bits = 52 bit physical	
   31 bits present in PDE , out of 40 bits only 31 bits are used and rest are reserved

   l2 entries or PDE for 4k pages: 

	level3_table  : covers 512*512*2M = 512G 
	level2_table; : covers 512*2M = 1G  
*/
#define L4_INDEX(x) (((addr_t)x&0xff8000000000)>>39)
#define L3_INDEX(x) (((addr_t)x&0x007fc0000000)>>30)
#define L2_INDEX(x) (((addr_t)x&0x00003fe00000)>>21)
#define L1_INDEX(x) (((addr_t)x&0x0000001ff000)>>12)
extern addr_t g_kernel_page_dir;
/**
  Sets up the environment, page directories etc and
  enables paging.
 **/
addr_t initialise_paging();

/**
  Causes the specified page directory to be loaded into the
  CR3 register.
 **/
void switch_page_directory(addr_t *new);


/**
  Handler for page faults.
 **/
void page_fault();

#endif
