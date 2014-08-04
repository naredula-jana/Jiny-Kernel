//#define DEBUG_ENABLE 1

#include "common.h"
#include "paging.h"
#include "isr.h"
#include "interface.h"


// The kernel's page directory
addr_t g_kernel_page_dir=0;

extern addr_t end; // end of code and data region
static int handle_mm_fault(addr_t addr,unsigned long faulting_ip, int writeFault, struct fault_ctx *ctx);
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
static void mk_pde(pde_t *pde, addr_t fr,int large_page,int global,int user)
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
	if (large_page ==1 )
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

static unsigned long g_kernel_address_space_starts=1;
unsigned long __va(unsigned long addr){
	if (g_kernel_address_space_starts==1){
		return ((void *)((unsigned long)(addr)+KERNEL_CODE_START));
	}else{
		return ((void *)((unsigned long)(addr)+g_kernel_address_space_starts));
	}
}
unsigned long __pa(unsigned long addr){
	if (g_kernel_address_space_starts==1){
		return ((unsigned long)(addr)-KERNEL_CODE_START);
	}else{
		if (addr > KERNEL_CODE_START)
			return ((unsigned long)(addr)-KERNEL_CODE_START);
		else
			return ((unsigned long)(addr)-g_kernel_address_space_starts);
	}
}
/* stich the page tables for entire kernel address space */
addr_t initialise_paging_new(addr_t physical_mem_size, unsigned long virt_image_end, unsigned long *virt_addr_start, unsigned long *virt_addr_end) {
	unsigned long virt_addr;
	unsigned long curr_virt_end_addr; /* page tables are stored at the end of images */
	addr_t i, j, fr, max_fr,level4_index,level3_index,level2_index,level2_table,level3_table;
	unsigned long image_size;

	curr_virt_end_addr = (virt_image_end + PAGE_SIZE) & (~0xfff);

	virt_addr = KADDRSPACE_START;
	g_kernel_page_dir=0x00101000;

	level4_index = L4_INDEX(virt_addr);
	ut_log("	paging init :addr:%x -> %x Lindex ( %x : %x : %x :%x )\n",virt_addr,L4_INDEX(virt_addr)*8,L4_INDEX(virt_addr),L3_INDEX(virt_addr),L2_INDEX(virt_addr),L1_INDEX(virt_addr));
	*virt_addr_start = virt_addr;
	*virt_addr_end = virt_addr + physical_mem_size - (8*1024);
	fr = 0;
	max_fr = physical_mem_size/(PAGE_SIZE); /* max pages */

	ut_log("	max_fr :%x (%d) image_end :%x(%d)\n",max_fr,max_fr,virt_image_end,virt_image_end-KERNEL_CODE_START);

	level3_table = curr_virt_end_addr;
	curr_virt_end_addr = curr_virt_end_addr + PAGE_SIZE;
	mk_pde(__va(0x101000+level4_index*8), (__pa(level3_table)) >> PAGE_SHIFT, 0, 1, 0);
	level3_index = 0;
	while (level3_index < 512) {/* each increment represents 1G */
		/* TODO: need to check the maximum physical memory and need to initilize accordingly, currently 1G is initilised */
		if (fr > max_fr) {
			break;
		}
		level2_index = 0; /* each increment represents 2M */
		level2_table = curr_virt_end_addr;
		mk_pde((level3_table), (__pa(level2_table)) >> PAGE_SHIFT, 0, 1, 0);

		curr_virt_end_addr = curr_virt_end_addr + PAGE_SIZE;
		while (level2_index <512) {/* covers 2M for each iteration */
			if (0) {/* use 2M size of pages */
				mk_pde((level2_table), fr, 1, 1, 0);
			} else { /* use 4k  size pages */
				mk_pde((level2_table), (__pa(curr_virt_end_addr)) >> PAGE_SHIFT, 0, 1, 0);
				for (j = 0; j < 512; j++) {
					if ((fr+j) <= (max_fr+1)){
						mk_pte(curr_virt_end_addr + j * 8, fr + j, 1, 0, 1);
					}else{
						goto last;
					}
				}
				curr_virt_end_addr = curr_virt_end_addr + PAGE_SIZE; /* this is physical space used for the page table */
			}
			level2_table = level2_table +8;
			level2_index++;
			fr = fr + 512; /* 2M = 512*4K frames */
		}/* end of while */

		level3_table = level3_table +8;
		level3_index++;
	}
last:
	ut_log("	paging init end :addr:%x ->  Lindex ( %x : %x : %x :%x )\n",curr_virt_end_addr,L4_INDEX(curr_virt_end_addr),L3_INDEX(curr_virt_end_addr),L2_INDEX(curr_virt_end_addr),L1_INDEX(curr_virt_end_addr));

	curr_virt_end_addr = curr_virt_end_addr + PAGE_SIZE;
	flush_tlb(0x101000);
	ut_log("	END address :%x fr:%x j:%x level3:%x(%d) level2:%x(%d)\n",curr_virt_end_addr,fr,j,level2_index,level2_index,level3_index,level3_index);

	g_kernel_address_space_starts = *virt_addr_start;
	virt_addr = virt_addr + (curr_virt_end_addr-KERNEL_CODE_START);

/* remove unnecessary hardcoded pagetable created at boot up time */
	image_size = virt_image_end-KERNEL_CODE_START;
	j= image_size /(512*4*1024); /* 2M pages */
	j = j+1;
	level2_table = __va(0x103000) +(j*8);
	for (i=j; i<21; i++){
		unsigned long *p;
		p=level2_table;
		ut_log("	%d: clearing the l2 entry :%x \n",i,p);
		*p=0;
		level2_table = level2_table + 8;
	}

	flush_tlb(0x101000);
	return (addr_t)virt_addr;
}

 void flush_tlb_entry(unsigned long  vaddr)
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

	DEBUG(" fault_addr :%x  errorcode:%x ip:%x ",faulting_address,ctx->errcode,ctx->istack_frame->rip);
	DEBUG("addr:%x ->  Lindex ( %x : %x : %x :%x )\n",faulting_address,L4_INDEX(faulting_address),L3_INDEX(faulting_address),L2_INDEX(faulting_address),L1_INDEX(faulting_address));

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
		handle_mm_fault(faulting_address, ctx->istack_frame->rip, 0, ctx);
		return;
	}
	if (rw) {
		DEBUG("Read-only \n");
		handle_mm_fault(faulting_address, ctx->istack_frame->rip, 1, ctx);
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
		BUG();
		SYS_sc_exit(901);
		return;
	}
	ut_log(" errorcode; %x resrved: %x rw:%x present:%x addr:%x  ip:%x\n",ctx->errcode,reserved,rw,present,faulting_address,ctx->istack_frame->rip);

	BUG();
}

#define MAX_KERNEL_ENTRIES 10
static int kernel_pages_level=3;
static unsigned long kernel_level3_entries[MAX_KERNEL_ENTRIES]; /* 1 entry represents 1G Memory */
static int kernel_level3_entries_count=0;
static void copy_kernel_pagetable_entries(){
	int i,j,k,m;
	unsigned long p4,p3,phy_entry; /* physical address */
	unsigned long *v4,*v3; /* virtual address */
	int found;

	k=0;
	p4 = g_kernel_mm->pgd;
	for (i=0; i<512; i++)
	{
		v4=__va(p4+i*8);
		p3=(*v4 & (~0xfff));
		if (p3==0) continue;
		for (j = 0; j < 512; j++) {
			found = 0;
			v3 = __va(p3+j*8);
			for (m = 0; m < k; m++) {
				if (kernel_level3_entries[m] == (*v3 & (~0xfff))) {
					found = 1;
					break;
				}
			}
			if (found == 0) {
				kernel_level3_entries[k] = (*v3 & (~0xfff));
				if (kernel_level3_entries[k] != 0)
					k++;
				else
					continue;
				if (k >= MAX_KERNEL_ENTRIES) {
					BUG();
				}
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
static inline unsigned long mm_pagealloc(struct mm_struct *mm){
	unsigned long ret;

	mm->stat_page_allocs++;
	ret = alloc_page(MEM_CLEAR);
	DEBUG(" pagetable alloc :%x  pa:%x allocs:%x \n",ret,__pa(ret),mm->stat_page_allocs);
	if (ret == 0){
		ut_printf("pagetable alloc :%x  pa:%x allocs:%x \n",ret,__pa(ret),mm->stat_page_allocs);
		Jcmd_mem(0,0);
	}
	return ret;
}
static int copy_pagetable(struct mm_struct *dest_mm, int level,unsigned long src_ptable_addr,unsigned long dest_ptable_addr)
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
					nv = (unsigned long *) mm_pagealloc(dest_mm);
					ut_memcpy(nv,__va(phy_addr),PAGE_SIZE);
					*dest=__pa(nv) | (*src&0xfff);
					DEBUG("Copying the page src:%x dest:%x \n",*dest,*src);
				}
			}else
			{
#if 1  /* To clear first 1G , so it can used for userspace ,  first 1G in user space and kernel space are different ,
   first 1G is need in the kernel space only for gdb, otherwise it can be cleared.
 */
				if ((level == kernel_pages_level) && i==0 && (*src & (~0xfff))==0x103000){
					*dest =0;
				}else
#endif
				if ((level == kernel_pages_level) && check_kernel_ptable_entry((*src & (~0xfff)))) /* for kernel pages , directly link the page table itself instead of copy recursively */
				{
					*dest = *src;
				} else {
					nv = (unsigned long *) mm_pagealloc(dest_mm);
					if (nv == 0)
						BUG();
					DEBUG(" calling i:%d level:%d src:%x  child src:%x physrc:%x \n", i, level, src_ptable_addr, __va(*src), *src);
					copy_pagetable(dest_mm,level - 1,(unsigned long) __va((*src)&(~0xfff)),
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
int pagetable_walk(int level,unsigned long ptable_addr, int print){
	unsigned long p,phy_entry; /* physical address */
	unsigned long *v; /* virtual address */
	unsigned int i,max_i,j;
	int count=0;
	int arg_print=print;

	p=ptable_addr;
	if (p == 0) return 0;

	if (level == 4){
		ut_printf("Page Table Walk : %x state:%x\n",ptable_addr,g_current_task->state);
	}
	max_i=512;
	for (i=0; i<max_i; i++){
		v=__va(p+i*8);
		phy_entry=(*v & (~0xfff));
		if (phy_entry == 0) continue;
		count++;

		if ((level == kernel_pages_level) && check_kernel_ptable_entry(phy_entry)){
			for (j=level; j<5; j++)
				if (print) ut_printf("   ");
			ut_printf(" %d (%d):%x  **** \n",level,i,phy_entry);
#if 0
			if (phy_entry== 0x103000){
				//ut_printf(" SKIP \n");
				continue;
			}
#endif
			//continue;
		}else{
			if (level >2 && print){
				for (j=level; j<5; j++)
					if (print) ut_printf("   ");
				ut_printf(" %d (%d):%x\n",level,i,phy_entry);
			}
		}

		if (level > 2) {
			pagetable_walk(level-1,phy_entry,arg_print);
		}
	}

	if (print){
		for (j=level; j<5; j++)
			if (print) ut_printf("   ");
		ut_printf(" count:%d \n",count);
	}
	return count;
}

/*
Return value : 1 - if entire table is cleared and deleted  else return 0
*/
static int clear_pagetable(struct mm_struct *mm, int level,unsigned long ptable_addr,unsigned long addr, unsigned long len,unsigned long start_addr)
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
	if (max_i>=512) max_i=512;
	if (addr == 0 && len ==0) 
	{
		i=0;
		max_i=512;
	}

	v=__va(p+i*8);
	start_addr=start_addr+i*max_entry;
	DEBUG("            i:%d max_i:%d tableaddr: %x \n",i,max_i,p);
	while( i<max_i )
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
					*v=0;
				}else
				{
					DEBUG("Freeing ANONYMOUS  from page table level:%d  vaddr:%x paddr:%x  \n",level,page,*v);
					if (level ==1) /* Freeing the Anonymous page */
					{
						mm->stat_page_free++;
						free_page((unsigned long)__va(page));
					    *v=0;
					}else
					{ /* TODO */
						ut_printf(" ERROR : in clear pagetable\n");
					}
				}
			}
			else /* Non leaf level entries */
			{
				DEBUG("  i=%d Call clear table : addr :%x len:%x start_addr:%x \n",i,addr,len,start_addr);
				if (clear_pagetable(mm,level-1,((*v)&(~0xfff)),addr,len,start_addr)==1) *v=0;
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
		free_page((unsigned long)__va(p));
		mm->stat_page_free++;
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
	if (clear_pagetable(mm,4,mm->pgd,addr,length,0) == 1)
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
	v=(unsigned char *)mm_pagealloc(dest_mm);
	if (v == 0) BUG();
	dest_mm->stat_page_allocs++;
	dest_mm->pgd=__pa(v);
	if (kernel_level3_entries_count==0 && src_mm==g_kernel_mm){
		copy_kernel_pagetable_entries();
	}
	copy_pagetable(dest_mm, 4,(unsigned long)__va(src_mm->pgd),(unsigned long)v);
#if 0
	pagetable_walk(4,src_mm->pgd,1);
	pagetable_walk(4,g_kernel_mm->pgd,1);
#endif
	return 1;
}

int ar_check_valid_address(unsigned long addr, int len) {
	addr_t *v; /* virtual address */
	addr_t *pl4, *pl3, *pl2, *pl1, *p; /* physical address */
	struct vm_area_struct *vma;

	if (g_boot_completed == 0)
		return 1; /* ignore checking while booting */

	return 1; /* TODO:HARDCODED */
	struct mm_struct *mm = g_current_task->mm;

	/* 1. check if it is  user level space */
	vma = vm_findVma(mm, (addr ), 4);
	if (vma != 0) {
		if ((addr+len) <= vma->vm_end){
			return JSUCCESS;
		}
	}

	/* If it is kernel thread then we have reached to fail point */
	if (mm == g_kernel_mm){
		BUG();
		return JFAIL;
	}

	/* 2. check for the kernel address */
	vma = vm_findVma(g_kernel_mm, (addr & PAGE_MASK), 4); /* check kernel addr */
	if (vma != 0){
		if ((addr+len) <= vma->vm_end){
			return JSUCCESS;
		}
	}
	Jcmd_maps(0,0);
	BUG();
	return JFAIL;
}
int g_stat_pagefault=0;
int g_stat_pagefaults_write=0;
extern unsigned long g_phy_mem_size;

int check_kernel_address(unsigned long addr){
	if (addr >=KADDRSPACE_START && addr <(KADDRSPACE_START + g_phy_mem_size)){
		return 1;
	}else{
		return 0;
	}
}
#define MAX_STAT_FAULTS 200
struct stat_fault{
	unsigned long  addr,fault_ip;
} stat_fault;
struct stat_fault stat_faults[MAX_STAT_FAULTS+1];

static int handle_mm_fault(addr_t addr,unsigned long faulting_ip, int write_fault, struct fault_ctx *ctx)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	addr_t *pl4,*pl3,*pl2,*pl1,*p; /* physical address */
	addr_t *v;	/* virtual address */
	unsigned char user=0;

	mm=g_kernel_mm;
	g_stat_pagefault++;
	stat_faults[g_stat_pagefault % MAX_STAT_FAULTS].addr = addr;
	stat_faults[g_stat_pagefault % MAX_STAT_FAULTS].fault_ip = faulting_ip;
	if ((check_kernel_address(addr)) && (write_fault == 1)) { /* look for the kernel slab data pages */
		vma=vm_findVma(g_kernel_mm,(addr & PAGE_MASK),8);
		assert(vma!=0);
		vma->stat_page_wrt_faults++;
		struct page *page = virt_to_page(addr & PAGE_MASK);
		if (PageSlab(page) ){
			jslab_pagefault(addr, faulting_ip, ctx);
			return 1;
		}else{
			ar_modifypte((unsigned long)addr,g_kernel_mm,(unsigned char)1); /* make the page write */
			vm_vma_stat(vma,addr,faulting_ip,write_fault,0);
			g_stat_pagefaults_write++;
			return 1;
		}
	}
	vma=vm_findVma(g_kernel_mm,(addr & PAGE_MASK),8);
	if (vma == 0 && g_current_task->mm != mm) {
		/* this is case where user thread running in kernel */
		user = 1;
		mm = g_current_task->mm;
		vma = vm_findVma(mm, (addr & PAGE_MASK), 8); /* length changed to just 8 bytes at maximum , instead of entire page*/
	}

	if (addr>=KERNEL_CODE_START && addr<=(KERNEL_CODE_START+0x200000)){
		while(1);
	}
	if (mm==0 || mm->pgd == 0){
		BUG();
	}

	if (vma == 0) 
	{
		if ( user == 1)
		{
			int stack_var;
		//	BUG();
			ut_printf("ERROR: user program Segmentaion Fault addr:%x  ip:%x :%s\n",addr,faulting_ip,g_current_task->name);
		BUG();
			Jcmd_maps(0,0);
			ut_log("ERROR: Segmentation fault page fault addr:%x ip:%x  \n",addr,faulting_ip);
			//Jcmd_lsmod(0,0);

			//ut_showTrace(&stack_var);
			SYS_sc_exit(902);
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
		v=(unsigned long *)mm_pagealloc(mm); /* get page of 4k size for page table */
		assert(v!=0);
		ut_memset((unsigned char *)v,0,4096);
		pl3=(unsigned long *)__pa(v);
		p=(pl4+(L4_INDEX(addr))); /* insert into l4 */
		v=(unsigned long *)__va(p);
		if (user)
			*v=((addr_t) pl3 |((addr_t) 0x7));
		else
			*v=((addr_t) pl3 |((addr_t) 0x7));
		DEBUG(" Inserted into L4 :%x  p13:%x  pl4:%x addr:%x index:%x \n",p,pl3,pl4,addr,L4_INDEX(addr));
	}
	p=(pl3+(L3_INDEX(addr)));
	v=__va(p);
	pl2=(unsigned long *)((*v) & (~0xfff));
	DEBUG(" Pl3 :%x pl4 :%x p12:%x \n",pl3,pl4,pl2);	

	if (pl2==0)
	{
		v=(unsigned long *)mm_pagealloc(mm); /* get page of 4k size for page table */
		if (v ==0) BUG();
		ut_memset((unsigned char *)v,0,4096);
		pl2=(unsigned long *)__pa(v);
		p=(pl3+(L3_INDEX(addr)));
		v=__va(p);
		if (user)
			*v=((addr_t) pl2 |((addr_t) 0x7));
		else
			*v=((addr_t) pl2 |((addr_t) 0x7));
		DEBUG(" INSERTED into L3 :%x  p12:%x  pl3:%x \n",p,pl2,pl3);
	}
	p=(pl2+(L2_INDEX(addr)));
	v=__va(p);
	pl1=(unsigned long *)((*v) & (~0xfff));
	DEBUG(" Pl2 :%x pl1 :%x\n",pl2,pl1);	

	if (pl1==0)
	{
		v=(unsigned long *)mm_pagealloc(mm); /* get page of 4k size for page table */
		assert (v !=0);
		ut_memset((unsigned char *)v,0,4096);
		pl1=(unsigned long *)__pa(v);
		p=(unsigned long *)(pl2+(L2_INDEX(addr)));
		v=__va(p);
		if (user)
			*v=((addr_t) pl1 |((addr_t) 0x7));
		else
			*v=((addr_t) pl1 |((addr_t) 0x7));
		DEBUG(" Inserted into L2 :%x :%x \n",p,pl1);
	}


	/* By Now we have pointer to all 4 tables , Now check the required page*/
    int writeFlag = vma->vm_prot&PROT_WRITE;
	if (vma->vm_flags & MAP_ANONYMOUS)
	{
		v=(unsigned long *)mm_pagealloc(mm); /* get page of 4k size for actual page */
		if ( v==0 ) { /* No Memory: kill the current process */
			ut_printf("Killing the current process because of shortage of memory \n");
			SYS_sc_exit(903);
			return 1;
		}
		p=(unsigned long *)__pa(v);
		vm_vma_stat(vma,addr,faulting_ip,write_fault,v);
		DEBUG(" Adding to LEAF: clean Anonymous page name:%s: page vaddr: %x addr:%x  faulip:%x pte:%x \n",g_current_task->name,v,addr,faulting_ip,(pl1+(L1_INDEX(addr))));
	}else if (vma->vm_flags & MAP_FIXED)
	{
		if ( vma->vm_inode != NULL)
		{
			asm volatile("sti");
			p=(unsigned long *)fs_getVmaPage(vma,vma->vm_private_data+(addr-vma->vm_start));
			assert(p!=0);
			if (write_fault && (writeFlag!= 0)) {
				addr_t *free_page;
				free_page=(unsigned long *)mm_pagealloc(mm);
#if 1
				int file_len = (vma->vm_end - vma->vm_start) - ((addr&PAGE_MASK) - (vma->vm_start));
				if (file_len > PAGE_SIZE) file_len = PAGE_SIZE;
				if (file_len > 0) {
					ut_memcpy((unsigned char *) free_page, (unsigned char *) __va(p), file_len);
				}
#else
				ut_memcpy((unsigned char *) free_page, (unsigned char *) __va(p), PAGE_SIZE);
#endif
				p=(unsigned long *)__pa(free_page);
				writeFlag = 1 ;
			}else{
				writeFlag = 0 ; /* this should be a COW data pages */
			}
			vm_vma_stat(vma,addr,faulting_ip,write_fault,0);

			asm volatile("cli");
			DEBUG(" Adding to LEAF: pagecache  paddr: %x vaddr: %x\n",p,addr);
		}else
		{
			p=(unsigned long *)(vma->vm_private_data + (addr-vma->vm_start)) ;
			if (user == 0){
				static int count=0;
#if 1
				count++;
				ut_log("Kernel Adding to LEAF: private page paddr: %x vaddr: %x \n",p,addr);
				ut_log("addr:%x ->  Lindex ( %x : %x : %x :%x )\n",addr,L4_INDEX(addr),L3_INDEX(addr),L2_INDEX(addr),L1_INDEX(addr));
				ut_log("%d: addr:%x ->  Lindexloc ( %x : %x : %x :%x )\n",count,addr,L4_INDEX(addr)*8,L3_INDEX(addr)*8,L2_INDEX(addr)*8,L1_INDEX(addr)*8);
				if (count > 2){
				 // BRK;
				}
#endif
			}
			DEBUG(" page fault of anonymous page p:%x  private_data:%x vm_start::x \n",p,vma->vm_private_data,vma->vm_start);
		}
	}else
	{
		ut_printf("ERROR: unknown vm_flag: %x \n",vma->vm_flags);
		BUG();
	}
	assert(p!=0);
	pl1=(pl1+(L1_INDEX(addr)));
	if (addr > KADDRSPACE_START && user==0){ /* then it is kernel address */
		mk_pte(__va(pl1),((addr_t)p>>12),1,0,1);/* global=on, user=off rw=on*/
		flush_tlb(0x101000);
	}else{
		mk_pte(__va(pl1),((addr_t)p>>12), 0, 1, writeFlag); /* global=off, user=on rw=flag from vma*/
	}

	ar_flushTlbGlobal();
	flush_tlb_entry(addr);
	if (addr >= 0x810000 &&  addr <= 0x811000){
		unsigned long *test_ptr=0x810ec0;
	}
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
unsigned long  ar_modifypte(unsigned long addr, struct mm_struct *mm, unsigned char rw)
{
	addr_t *pl4,*pl3,*pl2,*pl1,*v;
	pte_t *pte;
	pde_t *pde;
	int i;
	int ret=JFAIL;

	if (mm==0 || mm->pgd == 0 || addr==0) BUG();

	if (1)
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
			goto last;
#if 0
			if (pde->dirty==1){
			   ut_printf(" Big page: start:%x  end:%x  dirty:%x \n",addr,(addr+PAGE_SIZE*512),pde->dirty);
			}
			addr=addr+PAGE_SIZE*512;
			pde->dirty = 0;
#endif
		}

		pte=__va(pl1+(L1_INDEX(addr)));
		pte->rw = rw;
		ar_flushTlbGlobal();
	}

last:

	return addr;
}
