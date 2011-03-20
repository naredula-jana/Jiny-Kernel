#include "task.h"
#include "mm.h"
#include "paging.h"
#include "isr.h"
#include "interface.h"

// The kernel's page directory
addr_t g_kernel_page_dir=0;

// Defined in kheap.c
extern addr_t end; 
addr_t placement_address=(addr_t)&end;
static int handle_mm_fault(addr_t addr);
static void mk_pte(pte_t *pte, addr_t fr,int global)
{
        pte->present=1;
        pte->rw=1;
        pte->user=0;
        pte->pwt=0;
        pte->pcd=0;
        pte->accessed=0;
        pte->dirty=0;
        pte->pat=0;
        if (global==1)
                pte->global=1;
        else
                pte->global=0;

        pte->avl=0;

        pte->count=0;
        pte->nx=0;
        pte->frame = fr;
}
static void mk_pde(pde_t *pde, addr_t fr,int page_size,int global)  
{
	pde->present=1;
	pde->rw=1;
	pde->user=0;
	pde->pwt=0;
	pde->pcd=0;
	pde->accessed=0;
	pde->dirty=0;
	if (page_size ==1 )
		pde->ps=1;
	else
		pde->ps=0;
	if (global==1)
		pde->global=1;
	else
		pde->global=0;
	
	pde->avl=0;
	
	pde->count=0;
	pde->nx=0;
	pde->frame = fr;
}

/*addr_t temp_malloc(addr_t sz)
{
	// Align the placement address;
	placement_address &= 0xFFFFF000;
	placement_address += 0x1000;
	addr_t tmp = placement_address;
	placement_address += sz;
	return tmp;
} */

addr_t initialise_paging(addr_t end_addr)
{
	// The size of physical memory. For the moment we 
	// assume it is 16MB big.
	addr_t end_mem = end_addr;
	addr_t i,j,fr,nframes;
	addr_t *level2_table;

	end_mem &= PAGE_MASK;
	nframes  = MAP_NR(end_mem);
	g_kernel_page_dir=0x00101000;
	ut_printf(" Pageinit pacemaddr_t %x: endaddr:%x  \n",placement_address,end_addr);

	level2_table=(addr_t *)0x00103000+20; /* 20 entries(40M) already intialised */
	fr=0+20*512; /*  2M= 512 4Kpages already initialised */
	for (i=20; i<512; i++) /* 20 entres is already initialized, this loop covers for 1G */
	{
		if (fr > nframes) {
			break;
		}
		mk_pde(level2_table,fr,1,1);
		level2_table++;
		fr=fr+512; /* 2M = 512*4K frames */	
	}

	ut_printf("Pagetb :%x: endaddr:%x nframes:%x  FR:%x l2:%x \n",placement_address,end_addr,nframes,fr,level2_table);
	flush_tlb();
	return placement_address;
}
void flush_tlb()
{
	long dir=0x101000 ;
	asm volatile("mov %0, %%cr3":: "r"(dir));
}

unsigned long mmu_cr4_features;
#define X86_CR4_PGE		0x0080	/* enable global pages */
/*
 * Global pages have to be flushed a bit differently. Not a real
 * performance problem because this does not happen often.
 */
#define __flush_tlb_global()                                            \
        do {                                                            \
                unsigned long tmpreg;                                   \
                                                                        \
                __asm__ __volatile__(                                   \
                        "movq %1, %%cr4;  # turn off PGE     \n"        \
                        "movq %%cr3, %0;  # flush TLB        \n"        \
                        "movq %0, %%cr3;                     \n"        \
                        "movq %2, %%cr4;  # turn PGE back on \n"        \
                        : "=&r" (tmpreg)                                \
                        : "r" (mmu_cr4_features & ~(unsigned long )X86_CR4_PGE),   \
                          "r" (mmu_cr4_features)                        \
                        : "memory");                                    \
        } while (0)


void ar_pageFault(struct fault_ctx *ctx)
{
	// The faulting address is stored in the CR2 register.
	addr_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	// The error code gives us details of what happened.
	int present   = !(ctx->errcode & 0x1); // Page not present
	int rw = ctx->errcode & 0x2;           // Write operation?
	int us = ctx->errcode & 0x4;           // Processor was in user-mode?
	int reserved = ctx->errcode & 0x8;     // Overwritten CPU-reserved bits of page entry?
	int id = ctx->errcode & 0x10;          // Caused by an instruction fetch?

	// Output an error message.
	DEBUG("new PAGE FAULT  ip:%x  addr: %x \n",ctx->istack_frame->rip,faulting_address);
	DEBUG("PAGE FAULT  ip:%x  addr: %x ",ctx->istack_frame->rip,faulting_address);
	if (present) {
		ut_printf("page fault: Updating present \n");
		//mm_debug=1;
		handle_mm_fault(faulting_address);         
		return ;
	}
	if (rw) {
		ut_printf("Read-only \n");
		BUG();
		}
	if (us) {DEBUG("user-mode \n");}
	if (reserved) {DEBUG("reserved \n");}

	BUG();
}
static int debug_paging=0;
static int handle_mm_fault(addr_t addr)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	addr_t *pl4,*pl3,*pl2,*pl1,*p;	
	unsigned int index;

	mm=g_current_task->mm;
	
	if (mm==0 || mm->pgd == 0) BUG();
	
	//vma=vm_findVma(mm,(addr & PAGE_MASK),(4*1024)-1);
	vma=vm_findVma(mm,(addr & PAGE_MASK),8); /* length changed to just 8 bytes at maximum , instead of entire page*/
	if (vma == 0) BUG();

	pl4=mm->pgd;
	if (pl4 == 0) return 0;
 		
	p=(pl4+(L4_INDEX(addr))) ;
	pl3=(*p) & (~0xfff);
	if (pl3==0)
	{
		p=mm_getFreePages(0,0); /* get page of 4k size for page table */	
		if (p ==0) BUG();
		ut_memset(p,0,4096);
		pl3=p;
		p=(pl4+(L4_INDEX(addr))); /* insert into l4   */
		*p=((addr_t) pl3 |((addr_t) 0x3));
		if (debug_paging ==1) DEBUG(" Inserted into L4 :%x  p13:%x  pl4:%x \n",p,pl3,pl4);
	}
	p=(pl3+(L3_INDEX(addr)));
	pl2=(*p) & (~0xfff);
	if (debug_paging ==1) DEBUG(" Pl3 :%x pl4 :%x p12:%x \n",pl3,pl4,pl2);	

	if (pl2==0)
	{
		p=mm_getFreePages(0,0); /* get page of 4k size for page table */	
		if (p ==0) BUG();
		ut_memset(p,0,4096);
		pl2=p;
		p=(pl3+(L3_INDEX(addr)));
		*p=((addr_t) pl2 |((addr_t) 0x3));
		if (debug_paging ==1 )DEBUG(" INSERTED into L3 :%x  p12:%x  pl3:%x \n",p,pl2,pl3);
	}
	p=(pl2+(L2_INDEX(addr)));
	pl1=(*p) & (~0xfff);
	if (debug_paging == 1)DEBUG(" Pl2 :%x pl1 :%x\n",pl2,pl1);	
	
	
	if (pl1==0)
	{
		p=mm_getFreePages(0,0); /* get page of 4k size for page table */	
		if (p ==0) BUG();
		ut_memset(p,0,4096);
		pl1=p;
		p=(pl2+(L2_INDEX(addr)));
		*p=((addr_t) pl1 |((addr_t) 0x3));
		if (debug_paging == 1) DEBUG(" Inserted into L2 :%x :%x \n",p,pl1);
	}
	/* By Now we have pointer to all 4 tables , Now check th erequired page*/

	if (vma->vm_flags & MAP_ANONYMOUS)
	{
		p=mm_getFreePages(0,0); /* get page of 4k size for actual page */	
	}else if (vma->vm_flags & MAP_FIXED)
	{
		if ( vma->vm_inode != NULL)
		{
			DEBUG(" page fault of mmapped page \n");
			asm volatile("sti");
			p=(unsigned long *)pc_mapInodePage(vma,addr-vma->vm_start);
			asm volatile("cli");
		}else
		{
			DEBUG(" page fault of anonymous page \n");
			p=vma->vm_private_data + (addr-vma->vm_start) ; 	
		}
	}
	if (p==0) BUG();
	pl1=(pl1+(L1_INDEX(addr)));
	mk_pte(pl1,((addr_t)p>>12),0);

	asm volatile("movq %%cr4,%0" : "=r" (mmu_cr4_features));
	__flush_tlb_global();  /* TODO : need not flush entire table, flush only spoecific tables*/

	if (debug_paging == 1)
	DEBUG("FINALLY Inserted and global fluished into pagetable :%x pte :%x \n",pl1,p);
	return 1;	
}

/************************ house keeping thread related function ****************/
unsigned long  ar_scanPtes(unsigned long start_addr, unsigned long end_addr,struct addr_list *addr_list )
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	addr_t *pl4,*pl3,*pl2,*pl1,*p;	
	unsigned int index;
	pte_t *pte;
	unsigned long addr;
	int i;

	mm=g_current_task->mm;
	if (mm==0 || mm->pgd == 0) BUG();

	addr=start_addr;
	addr_list->total=0;
	while (addr <end_addr)
	{
		pl4=mm->pgd;
		if (pl4 == 0) return 0;

		p=(pl4+(L4_INDEX(addr))) ;
		pl3=(*p) & (~0xfff);
		if (pl3==0)
		{
			return 0;
		}

		p=(pl3+(L3_INDEX(addr)));
		pl2=(*p) & (~0xfff);
		if (pl2==0)
		{
			return 0;
		}

		p=(pl2+(L2_INDEX(addr)));
		pl1=(*p) & (~0xfff);
		if (pl1==0)
		{
			return 0;
		}
		pte=(pl1+(L1_INDEX(addr)));
		if (pte->accessed == 1) 
		{
			pte->accessed=0;
			i=addr_list->total;
			if (i<ADDR_LIST_MAX)
			{
				addr_list->addr[i]=addr;
				addr_list->total++;
			}else
			{
				break;
			}	
		}
		addr=addr+PAGE_SIZE;
	}
	if (addr_list->total > 0)
	{
		asm volatile("movq %%cr4,%0" : "=r" (mmu_cr4_features));
		__flush_tlb_global();  /* TODO : need not flush entire table, flush only spoecific tables*/
	}
	return addr;
}
