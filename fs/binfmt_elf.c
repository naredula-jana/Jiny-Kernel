#define DEBUG_ENABLE
#define DLINFO_ITEMS 13

#include <elf.h>
#include "mm.h"
#include "interface.h"

#ifndef elf_addr_t
#define elf_addr_t unsigned long
#define elf_caddr_t char *
#endif

/*
 * If we don't support core dumping, then supply a NULL so we
 * don't even try.
 */
#ifdef USE_ELF_CORE_DUMP
//static int elf_core_dump(long signr, struct pt_regs * regs, struct file * file);
#else
#define elf_core_dump	NULL
#endif

#if ELF_EXEC_PAGESIZE > PAGE_SIZE
# define ELF_MIN_ALIGN	ELF_EXEC_PAGESIZE
#else
# define ELF_MIN_ALIGN	PAGE_SIZE
#endif

#define ELF_PAGESTART(_v) ((_v) & ~(unsigned long)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v) (((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))

unsigned long fs_loadElfLibrary(struct file  *file,unsigned long tmp_stack, unsigned long stack_len)
{
	struct elf_phdr *elf_phdata;
	struct elf_phdr *eppnt;
	unsigned long elf_bss, bss, len;
	int retval, error, i, j;
	struct elfhdr elf_ex;

	error = 0;
	SYS_fs_lseek(file,0,0);
	retval = SYS_fs_read(file,  (unsigned char *) &elf_ex, sizeof(elf_ex));
	if (retval != sizeof(elf_ex))
	{
		error= -1;
		goto out;
	}

	if (ut_memcmp((unsigned char *)elf_ex.e_ident,(unsigned char *) ELFMAG, SELFMAG) != 0)
	{
		error= -2;
		goto out;
	}

	/* First of all, some simple consistency checks */
	//if (elf_ex.e_type != ET_EXEC || elf_ex.e_phnum > 2 ||
	if (elf_ex.e_type != ET_EXEC || 
			!elf_check_arch(&elf_ex) )
	{
		DEBUG("error:(not executable type or mismatch in architecture %x  %x %x \n",elf_ex.e_type,elf_ex.e_phnum,elf_check_arch(&elf_ex));
		error= -3;
		goto out;
	}

	/* Now read in all of the header information */

	j = sizeof(struct elf_phdr) * elf_ex.e_phnum;
	/* j < ELF_MIN_ALIGN because elf_ex.e_phnum <= 2 */

	elf_phdata = mm_malloc(j, 0);
	if (!elf_phdata)
	{
		error = -4;
		goto out;
	}

	eppnt = elf_phdata;
	DEBUG("start address : %x offset :%x \n",ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_offset);
	SYS_fs_lseek(file,(unsigned long)elf_ex.e_phoff,0);
	retval = SYS_fs_read(file, (unsigned char *)eppnt, j);
	if (retval != j)
	{
		error = -5;
		goto out_free_ph;
	}

	for (j = 0, i = 0; i<elf_ex.e_phnum; i++)
		if ((eppnt + i)->p_type == PT_LOAD) j++;
	if (j == 0)
	{
		error = -6;
		goto out_free_ph;
	}
	for (i = 0; i<elf_ex.e_phnum; i++,eppnt++) /* mmap all loadable program headers */
	{
		if (eppnt->p_type != PT_LOAD ) continue;
		DEBUG("%d: LOAD section: vaddr:%x filesz:%x offset:%x  \n",i,ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_filesz,eppnt->p_offset);
		/* Now use mmap to map the library into memory. */
		error=1;
		if (eppnt->p_filesz > 0)
		{	
			error = SYS_vm_mmap(file,
				ELF_PAGESTART(eppnt->p_vaddr),
				eppnt->p_filesz+ELF_PAGEOFFSET(eppnt->p_vaddr) ,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				0,
				(eppnt->p_offset -
				 ELF_PAGEOFFSET(eppnt->p_vaddr)));
		}
		//if (error != ELF_PAGESTART(eppnt->p_vaddr))
		if (error != 1)
		{
			error = -6;
			goto out_free_ph;
		}

		elf_bss = eppnt->p_vaddr + eppnt->p_filesz;
		//	padzero(elf_bss);

		len = ELF_PAGESTART(eppnt->p_filesz + eppnt->p_vaddr + ELF_MIN_ALIGN - 1);
		bss = eppnt->p_memsz + eppnt->p_vaddr;
		DEBUG(" bss :%x len:%x memsz:%x elf_bss:%x \n",bss,len,eppnt->p_memsz,elf_bss);
		if (bss > len) {
			vm_brk(len, bss - len);
		}
		error = 0;
	}

out_free_ph:
	mm_free(elf_phdata);
out:
	if (error != 0)
	{
		DEBUG(" ERROR in elf loader :%d\n",-error);
	}else
	{
		SYS_vm_mmap(0,USERSTACK_ADDR,USERSTACK_LEN,PROT_READ | PROT_WRITE ,MAP_ANONYMOUS,0);	
		if (stack_len > 0)
		{
			ut_memcpy(USERSTACK_ADDR+USERSTACK_LEN-stack_len,tmp_stack,stack_len);
		}
	}
	DEBUG(" Program start address(autod) : %x \n",elf_ex.e_entry);
	if ( error == 0)
		return elf_ex.e_entry;
	else return 0;
}
