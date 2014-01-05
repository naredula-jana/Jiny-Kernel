/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/mmap.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
//#define DEBUG_ENABLE
#include "common.h"
#include "mm.h"
#include "task.h"
#include "interface.h"
/*
 *
 *    vma -> mm  -> pagetable
 *    vma -> inode -> page
 *    page->inode ->{vma->mm ->page table}+
 *
 *    During pagefault(page injection):   start from pagefault
 *      pagetable->mm->vma->inode->page_read
 *      vma = vm_findVma(addr ,..)
 *      page = pc_getVmaPage(vma, file offset)
 *
 *    During page evacuation: starts from pagecache and remove entry from pagetable
 *      reclaim_freepages  -> vma_page_remove
 *		vma_page_remove(page) : page->inode->vma->mm->pagetable
 *
 *
 */
/* TODO : locking the entire vmap layer */
int stat_vma_removes=0;
void ipi_pagetable_interrupt(){
//	ut_log(" ipi interrupts clearing pagetables :cpuid :%d\n",getcpuid());
	ar_flushTlbGlobal();
	return;
}
int vma_page_remove(struct page *page){
	struct inode *inode;
	struct list_head *p;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	int found=0;

	inode=page->inode;
	if (inode ==0) return found;
	if (inode->flags & INODE_EXECUTING) return 1;
	list_for_each(p, &(inode->vma_list)) {
		vma=list_entry(p, struct vm_area_struct, inode_vma_link);
		mm=vma->vm_mm;
		if (mm == 0){
			BUG();
		}
		found=1;
		return 1;
#if 0
		stat_vma_removes++;
		vaddr =  vma->vm_start + page->offset;
		ar_pageTableCleanup(mm,vaddr,PAGE_SIZE);

		for (i=0; i<MAX_CPUS; i++)
			apic_send_ipi_vector(i,IPI_CLEARPAGETABLE);
		//ut_log(" PageTable Entry cleanup vaddr:%x file:%s\n",vaddr,inode->filename);
#endif
	}
	return found;
}
static int vma_unlink(struct mm_struct *mm, struct vm_area_struct *vma) {
	struct vm_area_struct *tmp;
	int ret = -1;

	tmp = mm->mmap;
	if (tmp == vma) {
		mm->mmap = vma->vm_next;
		vma->vm_next = 0;
		ret = 1;
		goto last;
	}
	while (tmp) {
		if (tmp->vm_next == vma) {
			tmp->vm_next = vma->vm_next;
			vma->vm_next = 0;
			ret = 1;
			goto last;
		}
		tmp = tmp->vm_next;
	}

last:
	if (ret == 1){ /* success case */
		if (vma->vm_inode != 0){
			list_del( &(vma->inode_vma_link));
			fs_putInode(vma->vm_inode);
		}
	}
	return ret;
}
static int vma_link(struct mm_struct *mm, struct vm_area_struct *vma) {
	struct vm_area_struct *tmp, *tmp_next;
	int ret = -1;

	vma->vm_next = 0;
	tmp = mm->mmap;
	if (tmp == 0) {
		mm->mmap = vma;
		ret=1;
		goto last;
	}
	while (tmp) {
		if (vma->vm_start >= tmp->vm_end) {
			tmp_next = tmp->vm_next;
			if (tmp_next != 0) {
				if (vma->vm_end < tmp_next->vm_start) {
					tmp->vm_next = vma;
					vma->vm_next = tmp_next;
					ret = 2;
					goto last;
				}
			} else {
				tmp->vm_next = vma;
				ret =1;
				goto last;
			}
		} else if (vma->vm_end < tmp->vm_start) {
			if (tmp == mm->mmap) {
				vma->vm_next = tmp;
				mm->mmap = vma;
				ret = 1 ;
				goto last;
			} else {
				ret = -1;
				goto last;
			}
		} else {
			ret = -2;
			goto last;
		}
		tmp = tmp->vm_next;
	}
	ret= -3;
last:
	if (ret >= 0){ /* success case */
		if (vma->vm_inode != 0){
			struct inode *inode;
			atomic_inc(&vma->vm_inode->count);
			inode = vma->vm_inode;
#if 1
			if (mm != g_kernel_mm){
				list_add( &(vma->inode_vma_link), &(inode->vma_list.next));
			}
#endif
		}
	}
	return ret;
}


struct vm_area_struct *vm_findVma(struct mm_struct *mm, unsigned long addr, unsigned long len) {
	struct vm_area_struct *vma;
	if (mm == 0) return 0;
	vma = mm->mmap;
	if (vma == 0)
		return 0;
	while (vma) {
		DEBUG(" [ %x - %x ] - %x - %x\n",vma->vm_start,vma->vm_end,addr,(addr+len));
		if ((vma->vm_start <= addr) && ((addr + len) < vma->vm_end)) {
			return vma;
		}
		vma = vma->vm_next;
	}
	return 0;
}


unsigned long vm_dup_vmaps(struct mm_struct *src_mm,struct mm_struct *dest_mm){ /* duplicate vmaps to the new thread */
	struct vm_area_struct *vma,*new_vma;

	vma = src_mm->mmap;
	while (vma) {
		new_vma = mm_slab_cache_alloc(vm_area_cachep, 0);
		if (new_vma == 0){
			BUG();
			return 0;
		}
		ut_memcpy((uint8_t *)new_vma,(uint8_t *)vma,sizeof(struct vm_area_struct));
		new_vma->vm_mm = dest_mm;
		new_vma->vm_next = 0;

		if (vma_link(dest_mm,new_vma)< 0){
			BUG();
		}
		vma = vma->vm_next;
	}

    return 1;
}
unsigned long vm_mmap(struct file *file, unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags, unsigned long pgoff, const char *name) {
	struct mm_struct *mm = g_current_task->mm;
	struct vm_area_struct *vma;
	int ret;

	//ut_log(" mmap : name:%s -- %s file:%x addr:%x len:%x pgoff:%x flags:%x  protection:%x\n",name,file->filename,file,addr,len,pgoff,flags,prot);
	vma = vm_findVma(mm, addr, len);
	if (vma){
		ut_log("VMA ERROR:  Already Found :%x \n",addr);
		return 0;
	}

	vma = mm_slab_cache_alloc(vm_area_cachep, 0);
	if (vma == 0){
		ut_log("VMA ERROR: alb alloc failed :%x \n",addr);
		return 0;
	}

	vma->vm_mm = mm;
	vma->name = name;
	vma->vm_flags = flags;
	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_prot = prot;
	vma->vm_inode = 0;
	vma->vm_private_data = pgoff;
	if (file != 0) {
		if (file->inode != 0) {
			vma->vm_flags = vma->vm_flags | MAP_FIXED;
			vma->vm_inode = file->inode;
			//if (len > (file->inode->file_size)) /*TODO: need to do stat and find the file length*/
			//vma->vm_end=addr+file->inode->file_size;

		}
	} else if (flags & MAP_ANONYMOUS) {
		if (addr == 0 && mm!=g_kernel_mm) {
			if (mm->anonymous_addr == 0)
				mm->anonymous_addr = USERANONYMOUS_ADDR;
			vma->vm_start = mm->anonymous_addr;
			vma->vm_end = vma->vm_start + len;
			mm->anonymous_addr = mm->anonymous_addr + len;
		}

	}
	ret=vma_link(mm, vma);
	if (ret < 0){
        ut_log(" mmap : name:%s file:%x addr:%x len:%x pgoff:%x flags:%x  protection:%x\n",name,file,addr,len,pgoff,flags,prot);
		ut_log(" Failed ret :%x \n",ret);
		//ut_getBackTrace(0,0,0);
		mm_slab_cache_free(vm_area_cachep, vma);
		return 0;
	}else{
		return vma->vm_start;
	}
}

void * SYS_vm_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags, unsigned long fd, unsigned long pgoff) {
	struct file *file;
	void *ret;

	SYSCALL_DEBUG("mmap fd:%x addr:%x len:%x prot:%x flags:%x pgpff:%x \n",fd,addr,len,prot,flags,pgoff);
	file = fd_to_file(fd);
	ret = vm_mmap(file, addr, len, prot, flags, pgoff,"from_syscall");
	SYSCALL_DEBUG("mmap ret :%x \n",ret);
	return ret;
}
/*
 *  this is really a simplified "do_mmap".  it only handles
 *  anonymous maps.  eventually we may be able to do some
 *  brk-specific accounting here.
 */
unsigned long vm_setupBrk(unsigned long addr, unsigned long len) {
	len = PAGE_ALIGN(len);
	if (!len)
		return addr;

	g_current_task->mm->brk_addr = addr;
	g_current_task->mm->brk_len = len;
	return vm_mmap(0, addr, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, 0,"anonymous_brk");
}
/************************************************************************************************************/


int SYS_vm_mprotect(const void *addr, int len, int prot) { /* TODO */
	SYSCALL_DEBUG("protect TODO :%x \n",addr);
	return 1;
}
unsigned long SYS_vm_brk(unsigned long addr) {
	struct vm_area_struct *vma;

	SYSCALL_DEBUG("brk:%x \n",addr);
	if (addr == 0)
		return g_current_task->mm->brk_addr + g_current_task->mm->brk_len;
	if (g_current_task->mm->brk_addr > (addr - g_current_task->mm->brk_len))
		return 0;
	vma = vm_findVma(g_current_task->mm, g_current_task->mm->brk_addr, g_current_task->mm->brk_len - 1);
	if (vma == 0){
		ut_printf(" BUG:  SYS_vm_brk addr:%x\n",addr);
		Jcmd_maps(0, 0);
		BUG();
	}
	if (addr > vma->vm_end) {
		/* TODO: check for collision for the next vm, less chance since the heap and stack are far apart */
		vma->vm_end = addr;
		g_current_task->mm->brk_len = addr - g_current_task->mm->brk_addr;
	}

	return addr;
}

/**********************************************************************************************/
int vm_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len) {
	struct vm_area_struct *vma;
	unsigned long start_addr, end_addr;
	int ret = 0;

	if ((len!=~0) && ((PAGE_ALIGN(len)) == 0))
		return -1;
	DEBUG("VMA unlinking addr:%x len:%x \n",addr,len);
	restart: vma = mm->mmap;
	if (vma == 0)
		return 0;
	while (vma) {
		DEBUG("VMA Unlink [ %x - %x ] - %x - %x\n",vma->vm_start,vma->vm_end,addr,(addr+len));
		if ((addr <= vma->vm_start) && ((addr + len) >= vma->vm_end)) {
			start_addr = vma->vm_start;
			end_addr = vma->vm_end;
			ret = vma_unlink(mm, vma);
			mm_slab_cache_free(vm_area_cachep, vma);
			ar_pageTableCleanup(mm, start_addr, end_addr - start_addr);
			ret++;

			DEBUG("VMA unlink : clearing the tables :start:%x leb:%x \n",start_addr,end_addr-start_addr);

			goto restart;
		}
		vma = vma->vm_next;
	}
	DEBUG("VMA unlink : clearing the tables \n");
	ar_pageTableCleanup(mm, addr, len);
	return ret;
}
int SYS_vm_munmap(unsigned long addr, unsigned long len) {
	SYSCALL_DEBUG("munmap:%x len:%x(%d)\n",addr,len,len);
	return vm_munmap(g_current_task->mm, addr, len);
}
void vm_vma_stat(struct vm_area_struct *vma, unsigned long vaddr,
		unsigned long faulting_ip, int write_fault,unsigned long optional) {
	if (vma == 0)
		return;
	int index = vma->stat_log_index % 10;
	vma->stat_log[index].vaddr = vaddr;
	vma->stat_log[index].fault_addr = faulting_ip;
	vma->stat_log[index].rw_flag = write_fault;
	vma->stat_log[index].optional = optional;

	if (write_fault){
		vma->stat_page_count++;
		vma->stat_page_wrt_faults++;
	}
	vma->stat_page_faults++;
	vma->stat_log_index++;
	return;
}
/************************** API function *****************/
extern task_queue_t g_task_queue;
int Jcmd_maps(char *arg1, char *arg2) {
	struct list_head *pos;
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long flags;
	unsigned char *buf;
	int len = PAGE_SIZE*100;
	int ret,pid,i,max_len;
	int found=0;

	max_len=len;
	buf = (unsigned char *) vmalloc(len,0);
	if (buf == 0) {
		ut_printf(" Unable to get vmalloc memory \n");
		return 0;
	}
	spin_lock_irqsave(&g_global_lock, flags);

	if (arg1 == 0) {
		found=1;
		task = g_current_task;
	} else {
		pid=ut_atoi(arg1);
		list_for_each(pos, &g_task_queue.head) {
			task = list_entry(pos, struct task_struct, task_queue);
			if (task->pid == pid) {
				found =1;
				break;
			}
		}
	}
	if (task == 0 || found==0) {
		goto last;
	}
	mm = task->mm;
	if (mm == 0)
		goto last;
	vma = mm->mmap;
	ret=0;
	buf[0]=0;
	while (vma) {
		int tret;
		struct inode *inode;

		inode = vma->vm_inode;
		if (inode == NULL) {
			len = len - ut_snprintf(buf+max_len-len,len,"%9s [ %p - %p ] - (+%p) flag:%x prot:%x stats:%d (%d/%d)\n",vma->name, vma->vm_start, vma->vm_end,
					vma->vm_private_data,vma->vm_flags,vma->vm_prot,vma->stat_page_count,vma->stat_page_faults,vma->stat_page_wrt_faults);
		} else {
			len = len - ut_snprintf(buf+max_len-len,len,"%9s [ %p - %p ] - %s(+%p) flag:%x prot:%x stats:%d (%d/%d)\n",vma->name, vma->vm_start, vma->vm_end,
					inode->filename, vma->vm_private_data,vma->vm_flags,vma->vm_prot, vma->stat_page_count,vma->stat_page_faults,vma->stat_page_wrt_faults);
		}
		for (i=0; i<10 && i<vma->stat_log_index; i++){
			len = len - ut_snprintf(buf+max_len-len,len,"	  vad:%x- faulip:%x - rw:%d option:%x \n",vma->stat_log[i].vaddr,vma->stat_log[i].fault_addr,vma->stat_log[i].rw_flag,vma->stat_log[i].optional);
		}

		if (len <0) goto last;
		vma = vma->vm_next;
	}
last:
	spin_unlock_irqrestore(&g_global_lock, flags);
    ut_printf(buf);
	vfree(buf);
	return 1;
}
