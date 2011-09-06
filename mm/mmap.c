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
#define DEBUG_ENABLE
#include "common.h"
#include "mm.h"
#include "task.h"
#include "interface.h"
static int vma_unlink(struct mm_struct *mm,struct vm_area_struct *vma)
{
        struct vm_area_struct *tmp;

        tmp=mm->mmap;
	if (tmp == vma)
	{
		mm->mmap=vma->vm_next;
		vma->vm_next=0;
		return 1;
	}
	while (tmp) {
	     if (tmp->vm_next == vma) 
		{
			tmp->vm_next=vma->vm_next;
			vma->vm_next=0;
			return 1;
		}
              tmp = tmp->vm_next;
	}
	return -1;
}
static int vma_link(struct mm_struct *mm,struct vm_area_struct *vma)
{
        struct vm_area_struct *tmp,*tmp_next;

	vma->vm_next=0;
	tmp=mm->mmap;
	if (tmp ==0) 
	{
		mm->mmap=vma;
		return 1;
	}
        while (tmp) {
             if (vma->vm_start > tmp->vm_end) 
             {
		 tmp_next=tmp->vm_next;
		 if (tmp_next != 0)
		 {
			if (vma->vm_end < tmp_next->vm_start)
			{
			tmp->vm_next=vma;
			vma->vm_next=tmp_next;
			return 2;
			}
		 } else
		 {
			tmp->vm_next=vma;
                 	return 1;
		 }
             }else if (vma->vm_end < tmp->vm_start)
	    {
		if (tmp == mm->mmap) 
		{
			vma->vm_next=tmp;
			mm->mmap=vma;
			return 1;
		}else
		{
			return -1;
		}
	     } else
	     {
		return -2;
	    }
             tmp = tmp->vm_next;
        }
        return -3;
}

/************************** API function *****************/

int vm_printMmaps(char *arg1,char *arg2)
{
        struct mm_struct *mm;
        struct vm_area_struct *vma;

        mm=g_current_task->mm;
        if (mm == 0) return 0;
        vma=mm->mmap;

	while (vma) {
		struct inode *inode;

		inode=vma->vm_inode;
		if (inode == NULL)
			ut_printf(" [ %x - %x ] - (+%x)\n",vma->vm_start,vma->vm_end,vma->vm_private_data);
		else
			ut_printf(" [ %x - %x ] - %s(+%x)\n",vma->vm_start,vma->vm_end,inode->filename,vma->vm_private_data);
		vma = vma->vm_next;
	}
	return 1;
}
struct vm_area_struct *vm_findVma(struct mm_struct *mm,unsigned long addr, unsigned long len)
{
	struct vm_area_struct *vma;
	vma=mm->mmap;
	if (vma ==0) return 0;
	while (vma) {
		DEBUG(" [ %x - %x ] - %x - %x\n",vma->vm_start,vma->vm_end,addr,(addr+len));
		if ((vma->vm_start <= addr) && ((addr+len) <  vma->vm_end))
		{
			return vma;
		}
		vma = vma->vm_next;
	}
	return 0;
}

long SYS_vm_mmap(unsigned long fd, unsigned long addr, unsigned long len,
		unsigned long prot, unsigned long flags, unsigned long pgoff)
{
	struct file *file;

	SYSCALL_DEBUG("mmap fs:%d addr:%x len:%x prot:%x flags:%x pgpff:%x \n",fd,addr,len,prot,flags,pgoff);
	file=fd_to_file(fd);
	if (file ==0 ) return 0;
	return vm_mmap(file,  addr, len, prot, flags, pgoff) ;
}

long vm_mmap(struct file *file, unsigned long addr, unsigned long len,
		unsigned long prot, unsigned long flags, unsigned long pgoff)
{
	struct mm_struct *mm = g_current_task->mm;
	struct vm_area_struct *vma;

	DEBUG(" mmap : addr:%x len:%x pgoff:%x flags \n",addr,len,pgoff,flags);
	vma=vm_findVma(mm,addr,len);
	if (vma) return -1;

	vma = kmem_cache_alloc(vm_area_cachep, 0);
	if (vma==0) return -2;

	vma->vm_flags = flags;		
	vma->vm_start=addr;
	vma->vm_end=addr+len;
	vma->vm_prot=prot;
	vma->vm_inode=0;
	vma->vm_private_data = pgoff ;
	if (file != 0)
	{
		if (file->inode != 0)
		{
			vma->vm_flags= vma->vm_flags | MAP_FIXED ;	
			vma->vm_inode=file->inode;
			if (len > (file->inode->length))
				vma->vm_end=addr+file->inode->length;

		}
	}
	vma_link(mm, vma);
	return 1;
}

/*
 *  this is really a simplified "do_mmap".  it only handles
 *  anonymous maps.  eventually we may be able to do some
 *  brk-specific accounting here.
 */
unsigned long vm_brk(unsigned long addr, unsigned long len)
{
	len = PAGE_ALIGN(len);
	if (!len)
		return addr;

	g_current_task->mm->brk_addr=addr;
	g_current_task->mm->brk_len=len;
	return vm_mmap(0,addr,len,PROT_READ | PROT_WRITE ,MAP_ANONYMOUS,0);
}
int SYS_vm_mprotect(const void *addr, int len, int prot)
{ /* TODO */
	SYSCALL_DEBUG("protect TODO :%x \n",addr);
}
unsigned long SYS_vm_brk(unsigned long addr)
{
	struct vm_area_struct *vma;

	SYSCALL_DEBUG("brk:%x \n",addr);
	if (addr ==0) return g_current_task->mm->brk_addr+g_current_task->mm->brk_len;
	if (g_current_task->mm->brk_addr > (addr-g_current_task->mm->brk_len)) return 0;
	vma=vm_findVma(g_current_task->mm,g_current_task->mm->brk_addr,g_current_task->mm->brk_len-1);
	if (vma == 0) BUG();
	vma->vm_end=addr;
	vm_printMmaps(0,0);
	return addr;	
}

int SYS_vm_munmap( unsigned long addr, unsigned long len)
{
	SYSCALL_DEBUG("munmap:%x \n",addr);
	return vm_munmap(g_current_task->mm, addr,len);
}
int vm_munmap(struct mm_struct *mm, unsigned long addr, unsigned long len)
{
	struct vm_area_struct *vma;
	unsigned long start_addr,end_addr;
	int ret=0;

	if ((len = PAGE_ALIGN(len)) == 0)
		return -1;
	DEBUG("VMA unlinking addr:%x len:%x \n",addr,len);
restart:
	vma=mm->mmap;
	if (vma ==0) return 0;
	while (vma) {
		DEBUG("VMA Unlink [ %x - %x ] - %x - %x\n",vma->vm_start,vma->vm_end,addr,(addr+len));
		if ((addr<=vma->vm_start) && ((addr+len) >=  vma->vm_end))
		{
			start_addr=vma->vm_start;
			end_addr=vma->vm_end;
			ret=vma_unlink(mm,vma);	
			kmem_cache_free(vm_area_cachep, vma);
			ar_pageTableCleanup(mm,start_addr, end_addr-start_addr);
			ret++;
			DEBUG("VMA unlink : clearing the tables :start:%x leb:%x \n",start_addr,end_addr-start_addr);
			goto restart;
		}
		vma = vma->vm_next;
	}
	DEBUG("VMA unlink : clearing the tables \n");
	ar_pageTableCleanup(mm,addr, len);
	return ret;
}

