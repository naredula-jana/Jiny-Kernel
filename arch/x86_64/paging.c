#define DEBUG_ENABLE 1
#include "task.h"
#include "mm.h"
#include "paging.h"
#include "isr.h"
#include "interface.h"
#include "common.h"

// The kernel's page directory
addr_t g_kernel_page_dir=0;
// Defined in kheap.c
extern addr_t end; 
addr_t placement_address=(addr_t)&end;
static int handle_mm_fault(addr_t addr);
static void mk_pte(pte_t *pte, addr_t fr,int global,int user)
{
        pte->present=1;
        pte->rw=1;
	if (user == 1) 
		pte->user=1;
	else
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
static void mk_pde(pde_t *pde, addr_t fr,int page_size,int global,int user)  
{
	pde->present=1;
	pde->rw=1;
	if (user == 1)
		 pde->user=1;
	else
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


addr_t initialise_paging(addr_t end_addr)
{
	// The size of physical memory. For the moment we 
	// assume it is 16MB big.
	addr_t end_mem = end_addr;
	addr_t i,j,fr,nframes;
	addr_t *level2_table;
	unsigned long *p;
	
	end_mem &= PAGE_MASK;
	nframes  = (end_mem)>> PAGE_SHIFT;
	g_kernel_page_dir=0x00101000;
	ut_printf("Initializing Paging  %x: physical high addr:%x  \n",placement_address,end_addr);
	level2_table=(addr_t *)0x00103000+20; /* 20 entries(40M) already intialised */
	fr=0+20*512; /*  2M= 512 4Kpages already initialised */
	for (i=20; i<512; i++) /* 20 entres is already initialized, this loop covers for 1G */
	{
		if (fr > nframes) {
			break;
		}
		mk_pde(__va(level2_table),fr,1,1,0);
		level2_table++;
		fr=fr+512; /* 2M = 512*4K frames */	
	}

	ut_printf("Initializing PAGING  nframes:%x  FR:%x l2:%x i=%d \n",nframes,fr,level2_table,i);
	p=0x00102000; /* TODO: the following two lines need to remove later */
	*p=0; /* reset the L3 table first entry need only when the paging is enabled */
	flush_tlb(0x101000);
	return placement_address;
}
void flush_tlb(unsigned long dir)
{
	asm volatile("mov %0, %%cr3":: "r"(dir));
}

#define X86_CR4_PGE		0x0080	/* enable global pages */
/*
 * Global pages have to be flushed a bit differently. Not a real
 * performance problem because this does not happen often.
 */
int ar_flushTlbGlobal()
{ 
	unsigned long mmu_cr4_features;
        unsigned long tmpreg;    

	asm volatile("movq %%cr4,%0" : "=r" (mmu_cr4_features));

	mmu_cr4_features=mmu_cr4_features | X86_CR4_PGE;	

	 __asm__ __volatile__(                                   
                        "movq %1, %%cr4;  # turn off PGE     \n"        
                        "movq %%cr3, %0;  # flush TLB        \n"        
                        "movq %0, %%cr3;                     \n"        
                        "movq %2, %%cr4;  # turn PGE back on \n"        
                        : "=&r" (tmpreg)                                
                        : "r" (mmu_cr4_features & ~(unsigned long )X86_CR4_PGE),   
                          "r" (mmu_cr4_features)                        
                        : "memory");       
	return 1;
}
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
	struct gpregs *gp=ctx->gprs;

	// Output an error message.
	DEBUG("new PAGE FAULT  ip:%x  addr: %x \n",ctx->istack_frame->rip,faulting_address);
        DEBUG("rbp:%x rsi:%x rdi:%x rdx:%x rcx:%x rbx:%x \n",gp->rbp,gp->rsi,gp->rdi,gp->rdx,gp->rcx,gp->rbx);
        DEBUG("r15:%x r14:%x r13:%x r12:%x r11:%x r10:%x \n",gp->r15,gp->r14,gp->r13,gp->r12,gp->r11,gp->r10);
        DEBUG("r9:%x r8:%x rax:%x\n",gp->r9,gp->r8,gp->rax);	
	ut_printf("PAGE FAULT ctx:%x  ip:%x  addr: %x ",ctx,ctx->istack_frame->rip,faulting_address);
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

static int kernel_pages_level=3;
static int kernel_pages_entry=(KERNEL_ADDR_START/0x40000000);
static int copy_pagetable(int level,unsigned long src_ptable_addr,unsigned long dest_ptable_addr)
{
	int i;
	unsigned long *src,*dest,*nv; /* virtual address */

	src=src_ptable_addr;	
	dest=dest_ptable_addr;
	DEBUG("S level: %d src:%x  dest:%x \n",level,src,dest);	

	for (i=0; i<512; i++)
	{
		if (((*src))!= 0)	
		{
			pde_t *pde;

			pde=(pde_t *)src;		
			nv=0;
			if ((level==2 && pde->ps==1) || level==1) /* 2nd level page with large pages or 1st level page table */
			{
				if (level ==2)
				{
				}else
				{
				}
				*dest=*src;

			}else
			{
				if ((level == kernel_pages_level) && (i>=kernel_pages_entry)) /* for kernel pages , directly link the page table itself instead of copy recursively */
				{
					*dest=*src;
				}else
				{
				nv=mm_getFreePages(MEM_CLEAR,0);	
				if (nv == 0) BUG();
				DEBUG(" calling i:%d level:%d src:%x  child src:%x physrc:%x \n",i,level,src_ptable_addr,__va(*src),*src);
				copy_pagetable(level-1,__va((*src)&(~0xfff)),nv);
				*dest=__pa(nv)|0x7;
				}
			}
			DEBUG("updated  level: %d i:%d *src:%x  dest:%x *dest:%x %x nv:%x  \n",level,i,*src,dest,*dest,__pa(nv),nv);	
		}
		src++;	
		dest++;	
	}
	DEBUG("E level: %d src:%x  dest:%x \n",level,src,dest);	
	return 1;
}
/*
Return value : 1 - if entire table is cleared and deleted  else retuen 0
*/
static int clear_pagetable(int level,unsigned long ptable_addr,unsigned long addr, unsigned long len,unsigned long start_addr)
{ 
	unsigned long max_entry,table_len;	
	unsigned long p; /* physical address */
	unsigned long *v; /* virtual address */
	int i,max_i,cleared;
	DEBUG("Clear Page tableLevel :%x ptableaddr:%x addr:%x len:%x start_addr:%x \n",level,ptable_addr,addr,len,start_addr);	
	p=ptable_addr;
	if (p == 0) return 0;
	switch (level)
	{
		case 4 : i=L4_INDEX(addr); max_entry=512*512*512*PAGE_SIZE; break;
		case 3 : i=L3_INDEX(addr); max_entry=512*512*PAGE_SIZE;  break;
		case 2 : i=L2_INDEX(addr); max_entry=512*PAGE_SIZE; break;
		case 1 : i=L1_INDEX(addr); max_entry=PAGE_SIZE; break;
		default : BUG();
	}

	if (addr < start_addr)
	{
		i=0;
		if ( len > 0)
		table_len=len - (start_addr-addr)-1;
		else table_len=0;
	}else
	{
		i=(addr-start_addr)/max_entry ;
		table_len=(addr-start_addr) +len-1;

	}
	max_i=table_len/max_entry;
	if (max_i>=512) max_i=511;
	if (addr == 0 && len ==0) 
	{
		i=0;
		max_i=511;
	}

	v=__va(p+i);
	start_addr=start_addr+i*max_entry;
	DEBUG(" i:%d max_i:%d \n",i,max_i);
	while( i<=max_i )
	{
		if (*v != 0 )
		{
			pde_t *pde;
			unsigned long page;
			pde=v;
			if ((level == kernel_pages_level) && (i>=kernel_pages_entry)) /* these are kernel pages , do not cleanup recuresvely */
			{
				*v=0;
			}
			else if ((level==2 && pde->ps==1) || level ==1) /* Leaf level entries */
			{
#if 1 
				page=(*v & (~0xfff));
				DEBUG(" Freeing leaf entry from page table vaddr:%x paddr:%x pc_start:%x %x \n",page,*v,pc_phy_startaddr,pc_phy_endaddr);
				if (is_pc_paddr(page))
				{ /* TODO : page cache*/

				}else
				{
					if (level ==1) /* Freeing the Anonymous page */
						mm_putFreePages(__va(page),0);				
					else
					{ /* TODO */
					}
				}
#endif
				*v=0;
			}
			else /* Non leaf level entries */
			{
				if (clear_pagetable(level-1,((*v)&(~0xfff)),addr,len,start_addr)==1) *v=0;
			}
		}
		start_addr=start_addr+max_entry;
		v++;
		i++;		
	}
	cleared=0;/* how many entries are cleared */
	v=__va(p);
	for (i=0; i<512; i++)
	{
		if (*v ==0) cleared++;
		v++;
	}

	if (cleared==512) /* All entries are cleared then remove the table */
	{
		v=v-512;
		DEBUG("Release PAGE during clear page tables: level:%d tableaddr: %x \n",level,p);
		mm_putFreePages(v,0);				
		return 1;
	}

	return 0;	
}
int ar_pageTableCleanup(struct mm_struct *mm,unsigned long addr, unsigned long length)
{ 
	if (mm==0 || mm->pgd == 0) BUG();
	if (mm->pgd == g_kernel_page_dir) 
	{
		ut_printf(" ERROR : trying to free kernel page table \n");
		return 0;
	}
	if (clear_pagetable(4,mm->pgd,addr,length,0) == 1)
	{
		mm->pgd=0;
		return 1;
	}
	return 0;
}

int ar_pageTableCopy(struct mm_struct *src_mm,struct mm_struct *dest_mm)
{
	unsigned char *v;

	if (src_mm->pgd == 0) return 0;
	if (dest_mm->pgd != 0) BUG();
	v=mm_getFreePages(MEM_CLEAR,0);	
	if (v == 0) BUG();
	dest_mm->pgd=__pa(v);
	copy_pagetable(4,__va(src_mm->pgd),v);	
	return 1;
}

static int handle_mm_fault(addr_t addr)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	addr_t *pl4,*pl3,*pl2,*pl1,*p; /* physical address */
	addr_t *v;	/* virtual address */
	unsigned int index;

	mm=g_current_task->mm;
	if (mm==0 || mm->pgd == 0) BUG();

	vma=vm_findVma(mm,(addr & PAGE_MASK),8); /* length changed to just 8 bytes at maximum , instead of entire page*/
	if (vma == 0) BUG();
	pl4=(mm->pgd);
	if (pl4 == 0) return 0;

	p=(pl4+(L4_INDEX(addr))) ;
	v=__va(p);
	pl3=(*v) & (~0xfff);
	if (pl3==0)
	{
		v=mm_getFreePages(0,0); /* get page of 4k size for page table */	
		if (v ==0) BUG();
		ut_memset(v,0,4096);
		pl3=__pa(v);
		p=(pl4+(L4_INDEX(addr))); /* insert into l4   */
		v=__va(p);
		*v=((addr_t) pl3 |((addr_t) 0x3));
		DEBUG(" Inserted into L4 :%x  p13:%x  pl4:%x addr:%x index:%x \n",p,pl3,pl4,addr,L4_INDEX(addr));
	}
	p=(pl3+(L3_INDEX(addr)));
	v=__va(p);
	pl2=(*v) & (~0xfff);
	DEBUG(" Pl3 :%x pl4 :%x p12:%x \n",pl3,pl4,pl2);	

	if (pl2==0)
	{
		v=mm_getFreePages(0,0); /* get page of 4k size for page table */	
		if (v ==0) BUG();
		ut_memset(v,0,4096);
		pl2=__pa(v);
		p=(pl3+(L3_INDEX(addr)));
		v=__va(p);
		*v=((addr_t) pl2 |((addr_t) 0x3));
		DEBUG(" INSERTED into L3 :%x  p12:%x  pl3:%x \n",p,pl2,pl3);
	}
	p=(pl2+(L2_INDEX(addr)));
	v=__va(p);
	pl1=(*v) & (~0xfff);
	DEBUG(" Pl2 :%x pl1 :%x\n",pl2,pl1);	

	if (pl1==0)
	{
		v=mm_getFreePages(0,0); /* get page of 4k size for page table */	
		if (v ==0) BUG();
		ut_memset(v,0,4096);
		pl1=__pa(v);
		p=(pl2+(L2_INDEX(addr)));
		v=__va(p);
		*v=((addr_t) pl1 |((addr_t) 0x3));
		DEBUG(" Inserted into L2 :%x :%x \n",p,pl1);
	}


	/* By Now we have pointer to all 4 tables , Now check the required page*/

	if (vma->vm_flags & MAP_ANONYMOUS)
	{
		v=mm_getFreePages(0,0); /* get page of 4k size for actual page */	
		p=__pa(v);
		DEBUG(" Adding to LEAF: Anonymous page paddr: %x vaddr: %x \n",p,addr);
	}else if (vma->vm_flags & MAP_FIXED)
	{
		if ( vma->vm_inode != NULL)
		{
			asm volatile("sti");
			p=(unsigned long *)pc_getVmaPage(vma,vma->vm_private_data+(addr-vma->vm_start));
			asm volatile("cli");
			DEBUG(" Adding to LEAF: pagecache  paddr: %x vaddr: %x\n",p,addr);
		}else
		{
			p=vma->vm_private_data + (addr-vma->vm_start) ; 	
			DEBUG(" Adding to LEAF: private page paddr: %x vaddr: %x \n",p,addr);
			DEBUG(" page fault of anonymous page p:%x  private_data:%x vm_start::x \n",p,vma->vm_private_data,vma->vm_start);
		}
	}
	if (p==0) BUG();
	pl1=(pl1+(L1_INDEX(addr)));
	if (addr > KERNEL_ADDR_START ) /* then it is kernel address */
		mk_pte(__va(pl1),((addr_t)p>>12),1,0);/* global=on, user=off */
	else	
		mk_pte(__va(pl1),((addr_t)p>>12),0,1); /* global=off, user=on */

	ar_flushTlbGlobal();

	DEBUG("FINALLY Inserted and global fluished into pagetable :%x pte :%x \n",pl1,p);
	return 1;	
}

/************************ house keeping thread related function ****************/
unsigned long  ar_scanPtes(unsigned long start_addr, unsigned long end_addr,struct addr_list *addr_list )
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	addr_t *pl4,*pl3,*pl2,*pl1,*v;	
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

		v=__va(pl4+(L4_INDEX(addr))) ;
		pl3=(*v) & (~0xfff);
		if (pl3==0)
		{
			return 0;
		}

		v=__va(pl3+(L3_INDEX(addr)));
		pl2=(*v) & (~0xfff);
		if (pl2==0)
		{
			return 0;
		}

		v=__va(pl2+(L2_INDEX(addr)));
		pl1=(*v) & (~0xfff);
		if (pl1==0)
		{
			return 0;
		}
		pte=__va(pl1+(L1_INDEX(addr)));
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
		ar_flushTlbGlobal(); /* TODO : need not flush entire table, flush only spoecific tables*/
	}
	return addr;
}
