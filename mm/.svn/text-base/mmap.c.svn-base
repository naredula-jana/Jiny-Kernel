#include "common.h"
#include "mm.h"
#include "task.h"
void vm_printMmaps()
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	mm=g_current_task->mm;
	if (mm == 0) return;
        vma=mm->mmap;

        while (vma) {
	     ut_printf(" [ %x - %x ]\n",vma->vm_start,vma->vm_end);
             vma = vma->vm_next;
        }
        return ;
}
struct vm_area_struct *vm_findVma(struct mm_struct *mm,unsigned long addr, unsigned long len)
{
	struct vm_area_struct *vma;
	vma=mm->mmap;
	if (vma ==0) return 0;
        while (vma) {
             if ((vma->vm_start <= addr) && ((addr+len) <  vma->vm_end))
             {
                    return vma;
             }
             vma = vma->vm_next;
        }
	return 0;
}
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
int make_pages_present(unsigned long start, unsigned long end)
{
	return 0;
}
unsigned long vm_mmap(struct file *file, unsigned long addr, unsigned long len,
        unsigned long prot, unsigned long flags, unsigned long pgoff)
{
        struct mm_struct *mm = g_current_task->mm;
        struct vm_area_struct *vma;

	vma=vm_findVma(mm,addr,len);
	if (vma) return 0;
	if (flags & MAP_FIXED)
	{
        	vma = kmem_cache_alloc(vm_area_cachep, 0);
		if (vma==0) return 0;
		vma->vm_flags = MAP_FIXED;		
		vma->vm_start=addr;
		vma->vm_end=addr+len;
		vma->vm_prot=prot;
		vma->vm_private_data = pgoff ;
        	vma_link(mm, vma);
		return 1;
	}
	return 0;
}
/*
 *  this is really a simplified "do_mmap".  it only handles
 *  anonymous maps.  eventually we may be able to do some
 *  brk-specific accounting here.
 */
unsigned long vm_brk(unsigned long addr, unsigned long len)
{
        struct mm_struct * mm = g_current_task->mm;
        struct vm_area_struct * vma;
        unsigned long flags;

        len = PAGE_ALIGN(len);
        if (!len)
                return addr;

        /*
         * Clear old maps.  this also does some error checking for us
         */
        vma=vm_findVma(mm,addr,len);
	if (vma) return 0;

        //flags = VM_DATA_DEFAULT_FLAGS | mm->def_flags;
	flags=MAP_ANONYMOUS;

        /*
         * create a vma struct for an anonymous mapping
         */
        vma = kmem_cache_alloc(vm_area_cachep, 0);
        if (!vma)
                return 0;
        vma->vm_mm = mm;
        vma->vm_start = addr;
        vma->vm_end = addr + len;
        vma->vm_flags = flags;
    //    vma->vm_page_prot = protection_map[flags & 0x0f];
        vma->vm_pgoff = 0;
        vma->vm_file = NULL;
        vma->vm_private_data = NULL;

        vma_link(mm, vma);

out:
        mm->total_vm += len >> PAGE_SHIFT;
        make_pages_present(addr, addr + len);
        return addr;
}

int mm_freePgtables(struct mm_struct *mm, unsigned long start_addr,unsigned long end_addr)
{
/*        start_index = pgd_index(first);
        end_index = pgd_index(last);
        if (end_index > start_index) {
                clear_page_tables(mm, start_index, end_index - start_index);
                flush_tlb_pgtables(mm, first & PGDIR_MASK, last & PGDIR_MASK);
        } */
}

int vm_munmap(struct mm_struct *mm, unsigned long addr, int len)
{
        struct vm_area_struct *vma;
	unsigned long start_addr,end_addr;
	int ret;

        if ((len = PAGE_ALIGN(len)) == 0)
            return -1;

        vma = vm_findVma(mm, addr, len);
        if (!vma)
             return 0;
        /* we have  addr < mpnt->vm_end  */
	start_addr=vma->vm_start;
	end_addr=vma->vm_end;

  	ret=vma_unlink(mm,vma);	
	if (ret <=0) return ret;
        /* Release the extra vma struct if it wasn't used */
        if (vma)
                kmem_cache_free(vm_area_cachep, vma);

        mm_freePgtables(mm,start_addr,end_addr);

        return 0;
}

