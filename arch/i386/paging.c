// paging.c -- Defines the interface for and structures relating to paging.
//             Written for JamesM's kernel development tutorials.

#include "paging.h"


// The kernel's page directory
extern addr initial_pg_directory;
page_directory_t *kernel_directory=0;

// The current page directory;
page_directory_t *current_directory=0;

// Defined in kheap.c
extern addr end;
addr placement_address=(addr)&end;
void mk_pte(pte_t *pte, addr fr)
{
	int is_writeable=1;
	int is_kernel=1;

	pte->present = 1;
	pte->rw = (is_writeable)?1:0;
	pte->user = (is_kernel)?0:1;
	pte->frame = fr;
}

addr temp_malloc(addr sz)
{
	// Align the placement address;
	placement_address &= 0xFFFFF000;
	placement_address += 0x1000;
	addr tmp = placement_address;
	placement_address += sz;
	return tmp;
}

addr initialise_paging(addr end_addr)
{
	// The size of physical memory. For the moment we 
	// assume it is 16MB big.
	addr end_mem = end_addr;
	addr i,j,fr,nframes;


	end_mem &= PAGE_MASK;
	nframes  = MAP_NR(end_mem);

	// Let's make a page directory.
/*	kernel_directory = (page_directory_t*)temp_malloc(sizeof(page_directory_t));
	kernel_directory = (page_directory_t*)initial_pg_directory; */
	kernel_directory = (page_directory_t*)0x00101000;
	current_directory = kernel_directory;

	printf("SecondTIME  Pageinit%x: %x %x sizeof pte:%d  szof dir:%d \n",placement_address,kernel_directory,&kernel_directory->pte_tables,sizeof(pte_t),sizeof(page_directory_t));
	fr=0+1024; /* 1024 pages already initialised */
	for (i=1; i<1024; i++) /* first entry is already initialized */
	{
		if (fr > nframes) {
			kernel_directory->pte_tables[i]=0;
			continue;
		}
		kernel_directory->pte_tables[i]=temp_malloc(4*1024) ;
		for (j=0; j<1024; j++)
		{
			if (fr < nframes)
			{
				mk_pte(&(kernel_directory->pte_tables[i]->pte[j]),fr);
			}else
			{
				kernel_directory->pte_tables[i]->pte[j].present=0;
				kernel_directory->pte_tables[i]->pte[j].frame=0;

			}
			fr=fr+1;
		}
		kernel_directory->pte_tables[i]=((addr)kernel_directory->pte_tables[i]) |  0x7; // PRESENT, RW, US.
	}

	printf("new  LATEST After Page tables installed :%x: endaddr:%x\n",placement_address,end_addr);
	// Before we enable paging, we must register our page fault handler.
	register_interrupt_handler(14, page_fault);

	// Now, enable paging!
//	switch_page_directory(kernel_directory);
	return placement_address;
}
/*
void switch_page_directory(page_directory_t *dir)
{
	current_directory = dir;
	asm volatile("mov %0, %%cr3":: "r"(dir));
	addr cr0;
	asm volatile("mov %%cr0, %0": "=r"(cr0));
	cr0 |= 0x80000000; // Enable paging!
	asm volatile("mov %0, %%cr0":: "r"(cr0));
}
*/


void page_fault(registers_t regs)
{
	// A page fault has occurred.
	// The faulting address is stored in the CR2 register.
	addr faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	// The error code gives us details of what happened.
	int present   = !(regs.err_code & 0x1); // Page not present
	int rw = regs.err_code & 0x2;           // Write operation?
	int us = regs.err_code & 0x4;           // Processor was in user-mode?
	int reserved = regs.err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
	int id = regs.err_code & 0x10;          // Caused by an instruction fetch?

	// Output an error message.
	printf("Page fault! ( ");
	if (present) {printf("Updating present ");
		//mm_debug=1;
		//alloc_frame( get_page(faulting_address, 1, kernel_directory), 0, 0);         
	}
	if (rw) {printf("read-only ");}
	if (us) {printf("user-mode ");}
	if (reserved) {printf("reserved ");}
	printf(") at 0x%x \n",faulting_address);
	while(1)
	{
	}
}
