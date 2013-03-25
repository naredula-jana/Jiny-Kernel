//#define DEBUG_ENABLE 1

#include "common.h"
#include "paging.h"
#include "isr.h"
#include "interface.h"


// The kernel's page directory
addr_t g_kernel_page_dir=0;

extern addr_t end; // end of code and data region
static int handle_mm_fault(addr_t addr,unsigned long faulting_ip, int writeFault);
static void mk_pte(pte_t *pte, addr_t fr,int global,int user,int rw)
{
	pte->present = 1;
	if (rw == 0)
	    pte->rw = 0;
	else
		pte->rw = 1;

	if (user == 1)
		pte->user = 1;
	else
		pte->user = 0;

	pte->pwt = 0;
	pte->pcd = 1;
	pte->accessed = 0;
	pte->dirty = 0;
	pte->pat = 0;
	if (global == 1)
		pte->global = 1;
	else
		pte->global = 0;

	pte->avl = 0;

	pte->count = 0;
	pte->nx = 0;
	if (pte->frame != 0) {
		/*ut_printf("ERROR : non zero value pte:%x value:%x \n", pte, pte->frame);
		BUG();*/
	}
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
	pde->pcd=1;
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
extern unsigned long VIDEO ;

addr_t initialise_paging(addr_t end_addr)
{
	// The size of physical memory. For the moment we 
	// assume it is 16MB big.
	addr_t end_mem = end_addr;
	addr_t i,j,fr,nframes;
	addr_t *level2_table;
	unsigned long curr_virt_end_addr;
	unsigned long *p;
	
	curr_virt_end_addr =(unsigned long) &end; /* till this point currently memory is in use, the rest can used for pagecache , heap etc. */
	end_mem &= PAGE_MASK;
	nframes  = (end_mem)>> PAGE_SHIFT;
	g_kernel_page_dir=0x00101000;
	curr_virt_end_addr= (curr_virt_end_addr+PAGE_SIZE)&(~0xfff);
//	ut_printf("Initializing Paging  %x: physical high addr:%x  \n",placement_address,end_addr);
	level2_table=(addr_t *)0x00103000+20; /* 20 entries(40M) already intialised */
	fr=0+20*512; /*  2M= 512 4Kpages already initialised */
	for (i=20; i<512; i++) /* 20 entres is already initialized, this loop covers for 1G */
	{
		if (fr > nframes) {
			break;
		}
		if (0) {/* use 2M size of pages */
			mk_pde(__va(level2_table), fr, 1, 1, 0);
		} else { /* use 4k  size pages */
			mk_pde(__va(level2_table), (__pa(curr_virt_end_addr))>>PAGE_SHIFT, 0, 1, 0);
			for (j = 0; j < 512; j++) {
				mk_pte(curr_virt_end_addr+j*8, fr + j, 1, 0, 1);
			}
			curr_virt_end_addr=curr_virt_end_addr+PAGE_SIZE;
		}
		level2_table++;
		fr=fr+512; /* 2M = 512*4K frames */	
	}

//	ut_printf("Initializing PAGING  nframes:%x  FR:%x l2:%x i=%d \n",nframes,fr,level2_table,i);
	p=(unsigned long *)0x00102000; /* TODO: the following two lines need to remove later */
	/* reset the L3 table first entry need only when the paging is enabled */

#ifdef SMP
	/* for SMP it is reset to zero after all the cpus are up */
#else
	*p=0;
#endif

	VIDEO=(unsigned long)__va(VIDEO);
	flush_tlb(0x101000);
	ut_printf("END address :%x\n",&end);
	return (addr_t)curr_virt_end_addr;
}
static inline void flush_tlb_entry(unsigned long  vaddr)
{
  __asm__ volatile("invlpg (%0)"
                   :: "r" (vaddr));
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
               //         "movq %2, %%cr4;  # turn PGE back on \n"
                        : "=&r" (tmpreg)                                
                        : "r" (mmu_cr4_features & ~(unsigned long )X86_CR4_PGE),   
                          "r" (mmu_cr4_features)                        
                        : "memory");       
	return 1;
}
void ar_pageFault(struct fault_ctx *ctx) {
	// The faulting address is stored in the CR2 register.
	addr_t faulting_address;
	asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

	// The error code gives us details of what happened.
	int present = !(ctx->errcode & 0x1); // Page not present
	int rw = ctx->errcode & 0x2; // Write operation?
	int us = ctx->errcode & 0x4; // Processor was in user-mode?
	int reserved = ctx->errcode & 0x8; // Overwritten CPU-reserved bits of page entry?
//	int id = ctx->errcode & 0x10; // Caused by an instruction fetch?
//	struct gpregs *gp = ctx->gprs;

	// Output an error message.
#if 0
	DEBUG("new PAGE FAULT  ip:%x  addr: %x \n",ctx->istack_frame->rip,faulting_address);
	DEBUG("rbp:%x rsi:%x rdi:%x rdx:%x rcx:%x rbx:%x \n",gp->rbp,gp->rsi,gp->rdi,gp->rdx,gp->rcx,gp->rbx);
	DEBUG("r15:%x r14:%x r13:%x r12:%x r11:%x r10:%x \n",gp->r15,gp->r14,gp->r13,gp->r12,gp->r11,gp->r10);
	DEBUG("r9:%x r8:%x rax:%x rsp:%x\n",gp->r9,gp->r8,gp->rax,ctx->istack_frame->rsp);
	DEBUG("PAGE FAULT ctx:%x  ip:%x  addr: %x ", ctx, ctx->istack_frame->rip, faulting_address);
#endif

	if (present) {
		DEBUG("page fault: Updating present \n");
		//mm_debug=1;
		handle_mm_fault(faulting_address, ctx->istack_frame->rip, 0);
		return;
	}
	if (rw) {
		DEBUG("Read-only \n");
		handle_mm_fault(faulting_address, ctx->istack_frame->rip, 1);
		return;
	}
	if (us) {
		DEBUG("user-mode \n");
	}
	if (reserved) {
		DEBUG("reserved \n");
	}
	if (g_current_task->mm != g_kernel_mm) /* user level thread */
	{
		SYS_sc_exit(101);
		return;
	}
	BUG();
}

#define MAX_KERNEL_ENTRIES 10
static int kernel_pages_level=3;
static unsigned long kernel_level3_entries[MAX_KERNEL_ENTRIES];
static int kernel_level3_entries_count=0;
static void copy_kernel_pagetable_entries(){
	int i,j,k;
	unsigned long p4,p3,phy_entry; /* physical address */
	unsigned long *v4,*v3; /* virtual address */

	k=0;
	p4 = g_kernel_mm->pgd;
	for (i=0; i<512; i++)
	{
		v4=__va(p4+i*8);
		p3=(*v4 & (~0xfff));
		if (p3==0) continue;
		for (j=0; j<512; j++){
			v3=__va(p3+j*8);
			kernel_level3_entries[k] = (*v3 & (~0xfff));
			if (kernel_level3_entries[k]!=0) k++;
			else continue;
			if (k>=MAX_KERNEL_ENTRIES){
				BUG();
			}
		}
	}
	kernel_level3_entries_count=k;
	if (k==0){
		BUG();
	}
}
static int check_kernel_ptable_entry(unsigned long phy_addr){
	int i;
	for (i=0; i<kernel_level3_entries_count; i++)
	{
		if (kernel_level3_entries[i]==(phy_addr & (~0xfff))){
			return 1;
		}
	}
	return 0;
}

static int copy_pagetable(int level,unsigned long src_ptable_addr,unsigned long dest_ptable_addr)
{
	int i;
	unsigned long *src,*dest,*nv; /* virtual address */

	src=(unsigned long *)src_ptable_addr;
	dest=(unsigned long *)dest_ptable_addr;
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
				unsigned long phy_addr=(*src & (~0xfff));
				if (is_pc_paddr(phy_addr)){ /* page cache physical addr */
					*dest=*src;
				}else{/* anonymous pages need to be copied */
					/* TODO : need to implement Copy On Write instead of Simple Copy */
					nv = (unsigned long *) mm_getFreePages(0, 0);
					ut_memcpy(nv,__va(phy_addr),PAGE_SIZE);
					*dest=__pa(nv) | (*src&0xfff);
					DEBUG("Copying the page src:%x dest:%x \n",*dest,*src);
				}
			}else
			{
				if ((level == kernel_pages_level) && check_kernel_ptable_entry((*src & (~0xfff)))) /* for kernel pages , directly link the page table itself instead of copy recursively */
				{
					*dest = *src;
				} else {
					nv = (unsigned long *) mm_getFreePages(MEM_CLEAR, 0);
					if (nv == 0)
						BUG();
					DEBUG(" calling i:%d level:%d src:%x  child src:%x physrc:%x \n", i, level, src_ptable_addr, __va(*src), *src);
					copy_pagetable(level - 1,(unsigned long) __va((*src)&(~0xfff)),
							(unsigned long) nv);
					*dest = __pa(nv) | 0x7;
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
static  void pagetable_walk(int level,unsigned long ptable_addr){
	unsigned long p,phy_entry; /* physical address */
	unsigned long *v; /* virtual address */
	unsigned int i,max_i,j;

	p=ptable_addr;
	if (p == 0) return 0;

	if (level == 4){
		ut_printf("Page Table Walk : %x \n",ptable_addr);
	}
	max_i=512;
	for (i=0; i<max_i; i++){
		v=__va(p+i*8);
		phy_entry=(*v & (~0xfff));
		if (phy_entry == 0) continue;
		for (j=level; j<5; j++)
			ut_printf("   ");


		if ((level == kernel_pages_level) && check_kernel_ptable_entry(phy_entry)){
			ut_printf(" %d (%d):%x  **** \n",level,i,phy_entry);
			continue;
		}
		ut_printf(" %d (%d):%x\n",level,i,phy_entry);
		if (level > 3) pagetable_walk(level-1,phy_entry);
	}
	return;
}

/*
Return value : 1 - if entire table is cleared and deleted  else return 0
*/
static int clear_pagetable(int level,unsigned long ptable_addr,unsigned long addr, unsigned long len,unsigned long start_addr)
{ 
	unsigned long max_entry,table_len;	
	unsigned long p; /* physical address */
	unsigned long *v; /* virtual address */
	unsigned int i,max_i,cleared;

	DEBUG("Clear Page tableLevel :%x ptableaddr:%x addr:%x len:%x start_addr:%x \n",level,ptable_addr,addr,len,start_addr);	
	p=ptable_addr;
	if (p == 0 || len==0) return 0;
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
		table_len=len - (start_addr-addr);
	}else
	{
		i=(addr-start_addr)/max_entry ;
		table_len=(addr-start_addr) +len;

	}
	max_i=table_len/max_entry;
	if (max_i>=512) max_i=511;
	if (addr == 0 && len ==0) 
	{
		i=0;
		max_i=511;
	}

	v=__va(p+i*8);
	start_addr=start_addr+i*max_entry;
	DEBUG("            i:%d max_i:%d tableaddr: %x \n",i,max_i,p);
	while( i<=max_i )
	{
		unsigned long entry_end,entry_start;
		entry_start=start_addr;
		entry_end=start_addr+max_entry;
	//	DEBUG(" lev:%d i:%d entry:[%x-%x] addr+len:%x v:%x:%x\n",level,i,entry_start,entry_end,(addr+len),v,*v);
		if (*v != 0 )
		{
			pde_t *pde;
			unsigned long page;
			pde=(pde_t *)v;
			if ((level == kernel_pages_level) && check_kernel_ptable_entry((*v & (~0xfff)))) /* these are kernel pages , do not cleanup recuresvely */
			{
				DEBUG(" Not Clearing kernel entry %d:%d  entry end:%x addr+len:%x\n",level,i,entry_end,(addr+len));

				*v=0;
			}
			else if ((level==2 && pde->ps==1) || level ==1) /* Leaf level entries */
			{
				page=(*v & (~0xfff));
		//		DEBUG(" Freeing leaf entry from page table level:%d  vaddr:%x paddr:%x  \n",level,page,*v);
				if (is_pc_paddr(page))
				{ /* TODO : page cache, need to decrease the usage count*/
					DEBUG(" Not freeing PAGECACHE page  from page table level:%d  vaddr:%x paddr:%x  \n",level,page,*v);

				}else
				{
					DEBUG("Freeing ANONYMOUS  from page table level:%d  vaddr:%x paddr:%x  \n",level,page,*v);
					if (level ==1) /* Freeing the Anonymous page */
						mm_putFreePages((unsigned long)__va(page),0);
					else
					{ /* TODO */
					}
				}
				*v=0;
			}
			else /* Non leaf level entries */
			{
				DEBUG("  i=%d Call clear table : addr :%x len:%x start_addr:%x \n",i,addr,len,start_addr);
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
		if (*v ==0){
			cleared++;
		}else{
			DEBUG("Unable to CLEAR i:%d level:%d Value:%x tableaddr: %x\n",i,level,*v,p);
		}
		v++;
	}
	v=v-512;
	if (cleared==512) /* All entries are cleared then remove the table */
	{
		DEBUG("  Release PAGE during clear page tables: level:%d tableaddr: %x \n",level,p);
		mm_putFreePages((unsigned long)__va(p),0);
		return 1;
	}else
	{
		DEBUG("  Unable to clear all entries: %d level:%d tableaddr: %x\n",cleared,level,p);
		//mm_putFreePages((unsigned long)v,0);
	}

	return 0;	
}
int ar_pageTableCleanup(struct mm_struct *mm,unsigned long addr, unsigned long length)
{ 
	if (mm==0 || mm->pgd == 0) return 0;
	if (mm->pgd == g_kernel_page_dir) 
	{
		ut_printf(" ERROR : trying to free kernel page table \n");
		return 0;
	}
	DEBUG("Pagetable Start Page : %x len:%x\n",mm->pgd,length);
	//if (length == ~0)
	//     pagetable_walk(4,mm->pgd);
	if (clear_pagetable(4,mm->pgd,addr,length,0) == 1)
	{
		mm->pgd=0;
		return 1;
	}
	return 0;
}

int ar_dup_pageTable(struct mm_struct *src_mm,struct mm_struct *dest_mm)
{
	unsigned char *v;

	if (src_mm->pgd == 0) return 0;
	if (dest_mm->pgd != 0) BUG();
	v=(unsigned char *)mm_getFreePages(MEM_CLEAR,0);
	if (v == 0) BUG();
	dest_mm->pgd=__pa(v);
	if (kernel_level3_entries_count==0 && src_mm==g_kernel_mm){
		copy_kernel_pagetable_entries();
	}
	copy_pagetable(4,(unsigned long)__va(src_mm->pgd),(unsigned long)v);
#if 0
	pagetable_walk(4,src_mm->pgd);
	pagetable_walk(4,g_kernel_mm->pgd);
#endif
	return 1;
}

static int handle_mm_fault(addr_t addr,unsigned long faulting_ip, int write_fault)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	addr_t *pl4,*pl3,*pl2,*pl1,*p; /* physical address */
	addr_t *v;	/* virtual address */
	unsigned char user=0;

	mm=g_kernel_mm;
	if (g_current_task->mm != mm){
		/* this is case where user thread running in kernel */
		 user=1;
		 mm = g_current_task->mm;
	}

	if (mm==0 || mm->pgd == 0) BUG();

	vma=vm_findVma(mm,(addr & PAGE_MASK),8); /* length changed to just 8 bytes at maximum , instead of entire page*/
	if (vma == 0) 
	{
		if ( user == 1)
		{
			ut_printf("ERROR: user program Segmentaion Fault addr:%x  ip:%x \n",addr,faulting_ip);
			SYS_sc_exit(102);
			return 1;
		}
		BUG();
	}
	pl4=(unsigned long *)(mm->pgd);
	if (pl4 == 0) return 0;

	p=(pl4+(L4_INDEX(addr))) ;
	v=__va(p);
	pl3=(unsigned long *)((*v) & (~0xfff));
	if (pl3==0)
	{
		v=(unsigned long *)mm_getFreePages(MEM_CLEAR,0); /* get page of 4k size for page table */
		if (v ==0) BUG();
		ut_memset((unsigned char *)v,0,4096);
		pl3=(unsigned long *)__pa(v);
		p=(pl4+(L4_INDEX(addr))); /* insert into l4   */
		v=(unsigned long *)__va(p);
		*v=((addr_t) pl3 |((addr_t) 0x7));
		DEBUG(" Inserted into L4 :%x  p13:%x  pl4:%x addr:%x index:%x \n",p,pl3,pl4,addr,L4_INDEX(addr));
	}
	p=(pl3+(L3_INDEX(addr)));
	v=__va(p);
	pl2=(unsigned long *)((*v) & (~0xfff));
	DEBUG(" Pl3 :%x pl4 :%x p12:%x \n",pl3,pl4,pl2);	

	if (pl2==0)
	{
		v=(unsigned long *)mm_getFreePages(MEM_CLEAR,0); /* get page of 4k size for page table */
		if (v ==0) BUG();
		ut_memset((unsigned char *)v,0,4096);
		pl2=(unsigned long *)__pa(v);
		p=(pl3+(L3_INDEX(addr)));
		v=__va(p);
		*v=((addr_t) pl2 |((addr_t) 0x7));
		DEBUG(" INSERTED into L3 :%x  p12:%x  pl3:%x \n",p,pl2,pl3);
	}
	p=(pl2+(L2_INDEX(addr)));
	v=__va(p);
	pl1=(unsigned long *)((*v) & (~0xfff));
	DEBUG(" Pl2 :%x pl1 :%x\n",pl2,pl1);	

	if (pl1==0)
	{
		v=(unsigned long *)mm_getFreePages(MEM_CLEAR,0); /* get page of 4k size for page table */
		if (v ==0) BUG();
		ut_memset((unsigned char *)v,0,4096);
		pl1=(unsigned long *)__pa(v);
		p=(unsigned long *)(pl2+(L2_INDEX(addr)));
		v=__va(p);
		*v=((addr_t) pl1 |((addr_t) 0x7));
		DEBUG(" Inserted into L2 :%x :%x \n",p,pl1);
	}


	/* By Now we have pointer to all 4 tables , Now check the required page*/
    int writeFlag = vma->vm_prot&PROT_WRITE;
	if (vma->vm_flags & MAP_ANONYMOUS)
	{
		v=(unsigned long *)mm_getFreePages(MEM_CLEAR,0); /* get page of 4k size for actual page */
		p=(unsigned long *)__pa(v);
		DEBUG(" Adding to LEAF: clean Anonymous page paddr: %x vaddr: %x \n",p,addr);
	}else if (vma->vm_flags & MAP_FIXED)
	{
		if ( vma->vm_inode != NULL)
		{

			asm volatile("sti");
			p=(unsigned long *)pc_getVmaPage(vma,vma->vm_private_data+(addr-vma->vm_start));
			if (write_fault && (writeFlag!= 0)) {
				addr_t *fp;
				fp=(unsigned long *)mm_getFreePages(MEM_CLEAR,0);
				ut_memcpy((unsigned char *)fp,(unsigned char *)__va(p),4096);
				p=(unsigned long *)__pa(fp);
				writeFlag = 1 ;

			}else{
				writeFlag = 0 ; /* this should be a COW data pages */
			}
			asm volatile("cli");
			DEBUG(" Adding to LEAF: pagecache  paddr: %x vaddr: %x\n",p,addr);
		}else
		{
			p=(unsigned long *)(vma->vm_private_data + (addr-vma->vm_start)) ;
			DEBUG(" Adding to LEAF: private page paddr: %x vaddr: %x \n",p,addr);
			DEBUG(" page fault of anonymous page p:%x  private_data:%x vm_start::x \n",p,vma->vm_private_data,vma->vm_start);
		}
	}else
	{
		ut_printf("ERROR: unknown vm_flag: %x \n",vma->vm_flags);
		BUG();
	}
	if (p==0) BUG();
	pl1=(pl1+(L1_INDEX(addr)));
	if (addr > KERNEL_ADDR_START && user==0) /* then it is kernel address */
		mk_pte(__va(pl1),((addr_t)p>>12),1,0,1);/* global=on, user=off rw=on*/
	else	
		mk_pte(__va(pl1),((addr_t)p>>12), 0, 1, writeFlag); /* global=off, user=on rw=flag from vma*/

	ar_flushTlbGlobal();
	flush_tlb_entry(addr);
	DEBUG("Finally Inserted addr:%x  Pl4: %x pl3:%x  p12:%x pl1:%x p:%x \n",addr,pl4,pl3,pl2,pl1,p);	
	return 1;	
}

/************************ house keeping thread related function ****************/

unsigned long  ar_scanPtes(unsigned long start_addr, unsigned long end_addr,struct addr_list *page_access_list, struct addr_list *page_dirty_list)
{
	struct mm_struct *mm;
	addr_t *pl4,*pl3,*pl2,*pl1,*v;	
	pte_t *pte;
	pde_t *pde;
	unsigned long addr,total_pages_scan;
	int i;

	total_pages_scan=0;
	mm=g_current_task->mm;
	if (mm==0 || mm->pgd == 0) BUG();

	addr=start_addr;
	if (page_access_list != 0)
	    page_access_list->total=0;

	if (page_dirty_list != 0)
	    page_dirty_list->total=0;
	while (addr <end_addr)
	{
		pl4=(unsigned long *)mm->pgd;
		if (pl4 == 0) return 0;

		v=__va(pl4+(L4_INDEX(addr))) ;
		pl3=(unsigned long *)((*v) & (~0xfff));
		if (pl3==0)
		{
			goto last;
		}

		v=__va(pl3+(L3_INDEX(addr)));
		pl2=(unsigned long *)((*v) & (~0xfff));
		if (pl2==0)
		{
			goto last;
		}

		v=__va(pl2+(L2_INDEX(addr)));
		pl1=(unsigned long *)((*v) & (~0xfff));
		pde = __va(pl2+(L2_INDEX(addr)));

		if (pde->ps == 1)/* 2 MB page */
		{
			if (pde->dirty==1){
			   ut_printf(" Big page: start:%x  end:%x  dirty:%x \n",addr,(addr+PAGE_SIZE*512),pde->dirty);
			}
			addr=addr+PAGE_SIZE*512;
			pde->dirty = 0;
			continue;
		}

		pte=__va(pl1+(L1_INDEX(addr)));

		if (pte->dirty == 1 && page_dirty_list!=0 && page_dirty_list->total<ADDR_LIST_MAX)
		{
			pte->dirty=0;
			i=page_dirty_list->total;
			if (i<ADDR_LIST_MAX)
			{
				page_dirty_list->addr[i]=addr;
				page_dirty_list->total++;
			}else{
				ut_printf("Dirty list is full addr:%x\n",addr);
			}
		}

		if (pte->accessed == 1 && page_access_list != 0 && page_access_list->total<ADDR_LIST_MAX)
		{
			pte->accessed=0;
			i=page_access_list->total;
			if (i<ADDR_LIST_MAX)
			{
				page_access_list->addr[i]=addr;
				page_access_list->total++;
			}
		}
		total_pages_scan=total_pages_scan+1;
		addr=addr+PAGE_SIZE;
	}
last:
	if ((page_access_list && page_access_list->total > 0 ) || (page_dirty_list && page_dirty_list->total > 0 ))
	{
		ut_printf("Flushing entire page table : pages scan:%d last addr:%x\n",total_pages_scan,addr);
		ar_flushTlbGlobal(); /* TODO : need not flush entire table, flush only specific tables*/
	}
	return addr;
}
