// paging.h -- Defines the interface for and structures relating to paging.
//             Written for JamesM's kernel development tutorials.

#ifndef PAGING_H
#define PAGING_H

#include "common.h"
#include "mm.h"
#define PAGE_MASK	(~(PAGE_SIZE-1))
typedef struct pte
{
    addr present    : 1;   // Page present in memory
    addr rw         : 1;   // Read-only if clear, readwrite if set
    addr user       : 1;   // Supervisor level only if clear
    addr accessed   : 1;   // Has the page been accessed since last refresh?
    addr dirty      : 1;   // Has the page been written to since last refresh?
    addr unused     : 7;   // Amalgamation of unused and reserved bits
    addr frame      : 20;  // Frame address (shifted right 12 bits)
} pte_t;

typedef struct pte_table
{
    pte_t pte[1024];
} page_table_t;

typedef struct page_directory
{
    page_table_t *pte_tables[1024];
} page_directory_t;

/**
   Sets up the environment, page directories etc and
   enables paging.
**/
addr initialise_paging();

/**
   Causes the specified page directory to be loaded into the
   CR3 register.
**/
void switch_page_directory(page_directory_t *new);


/**
   Handler for page faults.
**/
void page_fault(registers_t regs);

#endif
