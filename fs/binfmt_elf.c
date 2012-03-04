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
/* Symbolic values for the entries in the auxiliary table
   put on the initial stack */
#define AT_NULL   0     /* end of vector */
#define AT_IGNORE 1     /* entry should be ignored */
#define AT_EXECFD 2     /* file descriptor of program */
#define AT_PHDR   3     /* program headers for program */
#define AT_PHENT  4     /* size of program header entry */
#define AT_PHNUM  5     /* number of program headers */
#define AT_PAGESZ 6     /* system page size */
#define AT_BASE   7     /* base address of interpreter */
#define AT_FLAGS  8     /* flags */
#define AT_ENTRY  9     /* entry point of program */
#define AT_NOTELF 10    /* program is not ELF */
#define AT_UID    11    /* real uid */
#define AT_EUID   12    /* effective uid */
#define AT_GID    13    /* real gid */
#define AT_EGID   14    /* effective gid */
#define AT_PLATFORM 15  /* string identifying CPU for optimizations */
#define AT_HWCAP  16    /* arch dependent hints at CPU capabilities */
#define AT_CLKTCK 17    /* frequency at which times() increments */
/* AT_* values 18 through 22 are reserved */
#define AT_SECURE 23   /* secure mode boolean */
#define AT_BASE_PLATFORM 24     /* string identifying real platform, may
                                 * differ from AT_PLATFORM. */
#define AT_RANDOM 25    /* address of 16 random bytes */

#define AT_EXECFN  31   /* filename of program */

#define AUX_ENT(id, val) \
        do { \
                aux_vec[aux_index++] = id; \
                aux_vec[aux_index++] = val; \
        } while (0)

#define userstack_addr(kaddr)  (USERSTACK_ADDR+USERSTACK_LEN+kaddr-((kaddr/PAGE_SIZE)*PAGE_SIZE)-PAGE_SIZE)
unsigned long fs_loadElfLibrary(struct file  *file,unsigned long tmp_stack, unsigned long stack_len,unsigned long aux_addr)
{
	struct elf_phdr *elf_phdata;
	struct elf_phdr *eppnt;
	unsigned long elf_bss, bss, len;
	int retval, error, i, j;
	struct elfhdr elf_ex;
	unsigned long *aux_vec,aux_index,load_addr;

	error = 0;
	fs_lseek(file,0,0);
	retval = fs_read(file,  (unsigned char *) &elf_ex, sizeof(elf_ex));
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
	fs_lseek(file,(unsigned long)elf_ex.e_phoff,0);
	retval = fs_read(file, (unsigned char *)eppnt, j);
	if (retval != j)
	{
		error = -5;
		goto out_free_ph;
	}
	DEBUG("START address : %x offset :%x \n",ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_offset);
	for (j = 0, i = 0; i<elf_ex.e_phnum; i++)
		if ((eppnt + i)->p_type == PT_LOAD) j++;
	if (j == 0)
	{
		error = -6;
		goto out_free_ph;
	}
	load_addr=ELF_PAGESTART(eppnt->p_vaddr);
	for (i = 0; i<elf_ex.e_phnum; i++,eppnt++) /* mmap all loadable program headers */
	{
		if (eppnt->p_type != PT_LOAD ) continue;
		DEBUG("%d: LOAD section: vaddr:%x filesz:%x offset:%x  \n",i,ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_filesz,eppnt->p_offset);
		/* Now use mmap to map the library into memory. */
		error=1;
		if (eppnt->p_filesz > 0)
		{	
			error = vm_mmap(file,
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
		vm_mmap(0,USERSTACK_ADDR,USERSTACK_LEN,PROT_READ | PROT_WRITE ,MAP_ANONYMOUS,0);	
		if (stack_len > 0)
		{
			aux_vec=aux_addr;
			if (aux_vec != 0)
			{
				int aux_last;

				aux_index=0;
				aux_last=(MAX_AUX_VEC_ENTRIES-2)*2;

				//AUX_ENT(AT_HWCAP, ELF_HWCAP);
				AUX_ENT(AT_HWCAP, 0); /* TODO */
        			AUX_ENT(AT_PAGESZ, PAGE_SIZE);
        			AUX_ENT(AT_CLKTCK, 100);
        			AUX_ENT(AT_PHDR, load_addr + elf_ex.e_phoff);
        			AUX_ENT(AT_PHENT, sizeof(struct elf_phdr));
        			AUX_ENT(AT_PHNUM, elf_ex.e_phnum);
        			AUX_ENT(AT_BASE, 0);
        			AUX_ENT(AT_FLAGS, 0);
        			AUX_ENT(AT_ENTRY, elf_ex.e_entry);
        			AUX_ENT(AT_UID, 0);
        			AUX_ENT(AT_EUID, 0);
        			AUX_ENT(AT_GID, 0);
        			AUX_ENT(AT_EGID, 0);
     //   AUX_ENT(AT_SECURE, security_bprm_secureexec(bprm));

				aux_vec[aux_last]=  0x1234567887654321;
				aux_vec[aux_last+1]=0x1122334455667788;
        			AUX_ENT(AT_RANDOM, userstack_addr((unsigned long)&aux_vec[aux_last]));
      //  AUX_ENT(AT_EXECFN, bprm->exec);	
			}
			ut_memcpy(USERSTACK_ADDR+USERSTACK_LEN-stack_len,tmp_stack,stack_len);
			
		}
	}
	DEBUG(" Program start address(autod) : %x \n",elf_ex.e_entry);
	if ( error == 0)
		return elf_ex.e_entry;
	else return 0;
}
