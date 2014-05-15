//#define DEBUG_ENABLE
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

extern void * __vsyscall_page;
#define userstack_addr(kaddr)  (USERSTACK_ADDR+USERSTACK_LEN+kaddr-((kaddr/PAGE_SIZE)*PAGE_SIZE)-PAGE_SIZE)

#define MAX_USERSPACE_STACK_TEMPLEN 409600
static unsigned long setup_userstack(unsigned char **argv, unsigned char **env, unsigned long *stack_len, unsigned long *t_argc, unsigned long *t_argv, unsigned long *p_aux, unsigned char *elf_interp) {
	int i, len, total_args = 0;
	int total_envs =0;
	unsigned char *p, *stack;
	unsigned long real_stack, addr;
	unsigned char **target_argv;
	unsigned char **target_env;
	int max_stack_len=MAX_USERSPACE_STACK_TEMPLEN;
	int max_list_len=(PAGE_SIZE/sizeof(void *))-1;
	unsigned long ret=0;


	if (argv == 0 && env == 0) {
		ut_printf(" ERROR in setuo_userstack argv:0\n");
		return 0;
	}

	target_argv = (unsigned char **) alloc_page(MEM_CLEAR);
	target_env = (unsigned char **) alloc_page(MEM_CLEAR);
	if (target_argv==0 || target_env==0){
		BUG();
	}
	stack = (unsigned char *) vmalloc(MAX_USERSPACE_STACK_TEMPLEN,MEM_CLEAR);
	if (stack ==0){
		goto error;
	}
	ret = stack;

	p = stack + max_stack_len;
	len = 0;
	real_stack = USERSTACK_ADDR + USERSTACK_LEN;

	if (elf_interp != 0){
		//BUG();
		len = ut_strlen(elf_interp);
		p = p - len - 1;
		real_stack = real_stack - len - 1;
		ut_strcpy(p, elf_interp);
		target_argv[total_args] = (unsigned char *)real_stack;
		total_args++;
	}
	for (i = 0; argv[i] != 0 && i < max_list_len; i++) {
		len = ut_strlen(argv[i]);
		if ((p - len - 1) > stack) {
			p = p - len - 1;
			real_stack = real_stack - len - 1;
			DEBUG(" argument :%d address:%x \n",i,real_stack);
			ut_strcpy(p, argv[i]);
			target_argv[total_args] = (unsigned char *)real_stack;
			total_args++;
		} else {
			ret=0 ;
			goto error;
		}
	}
	target_argv[total_args] = 0;

	for (i = 0; env[i] != 0 && i < max_list_len; i++) {
		total_envs++;
		len = ut_strlen(env[i]);
		if ((p - len - 1) > stack) {
			p = p - len - 1;
			real_stack = real_stack - len - 1;
			DEBUG(" envs :%d address:%x \n",i,real_stack);
			ut_strcpy(p, env[i]);
			target_env[i] = (unsigned char *)real_stack;
		} else {
			ret=0 ;
			goto error;
		}
	}
	target_env[i] = 0;

	addr = (unsigned long)p;
	addr = (unsigned long)((addr / 8) * 8);
	p = (unsigned char *)addr;

	p = p - (MAX_AUX_VEC_ENTRIES * 16);

	real_stack = USERSTACK_ADDR + USERSTACK_LEN + p - (stack + max_stack_len);
	*p_aux = (unsigned long)p;
	len = (1+total_args + 1 + total_envs+1) * 8; /* total_args+args+0+envs+0 */
	if ((p - len - 1) > stack) {
		unsigned long *t;

		p = p - (total_envs+1)*8;
		ut_memcpy(p, (unsigned char *)target_env, (total_envs+1)*8);

		p = p - (1+total_args+1)*8;
		ut_memcpy(p+8, (unsigned char *)target_argv, (total_args+1)*8);
		t = (unsigned long *)p;
		*t = total_args; /* store argc at the top of stack */

		DEBUG(" arg0:%x arg1:%x arg2:%x len:%d \n",target_argv[0],target_argv[1],target_argv[2],len);
		DEBUG(" arg0:%x arg1:%x arg2:%x len:%d \n",target_env[0],target_env[1],target_env[2],len);

		real_stack = real_stack - len;
	} else {
		ret=0;
		goto error;
	}

	*stack_len = max_stack_len - (p - stack);
	*t_argc = total_args;
	*t_argv = real_stack ;

error:
	if (ret ==0 ){
		ut_log(" Error: user stack creation failed :%s:\n",g_current_task->name);
		vfree((unsigned long)stack);
	}
	mm_putFreePages((unsigned long)target_argv, 0);
	mm_putFreePages((unsigned long)target_env, 0);
	return ret;
}
unsigned long fs_elf_check_prepare(struct file *file,unsigned char **argv, unsigned char **env,unsigned long *t_argc, unsigned long *t_argv,unsigned long  *stack_len, unsigned long *aux_addr,unsigned char **elf_interpreter, unsigned long *tmp_stackp) {
	struct elf_phdr *elf_phdata=0;
	struct elf_phdr *eppnt;
	int retval, error, i, j;
	struct elfhdr elf_ex;
	Elf64_Addr p_entry;
	unsigned long tmp_stack_top=0;

	error = 0;
	fs_lseek(file, 0, 0);
	retval = fs_read(file, (unsigned char *) &elf_ex, sizeof(elf_ex));
	if (retval != sizeof(elf_ex)) {
		error = -1;
		return 0;
	}
	if (ut_memcmp((unsigned char *) elf_ex.e_ident, (unsigned char *) ELFMAG, SELFMAG) != 0) {
		error = -2;
		return 0;
	}

	if (elf_ex.e_type == ET_DYN)  elf_ex.e_type=ET_EXEC;
	if (elf_ex.e_type != ET_EXEC || !elf_check_arch(&elf_ex)) {
		DEBUG("error:(not executable type or mismatch in architecture %x  %x %x \n",elf_ex.e_type,elf_ex.e_phnum,elf_check_arch(&elf_ex));
		error = -3;
		return 0;
	}

	/* Now read in all of the header information */
	j = sizeof(struct elf_phdr) * elf_ex.e_phnum;
	/* j < ELF_MIN_ALIGN because elf_ex.e_phnum <= 2 */

	elf_phdata = mm_malloc(j, 0);
	if (!elf_phdata) {
		error = -4;
		return 0;
	}

	eppnt = elf_phdata;
	fs_lseek(file, (unsigned long) elf_ex.e_phoff, 0);
	retval = fs_read(file, (unsigned char *) eppnt, j);
	if (retval != j) {
		goto out;
	}

	p_entry = elf_ex.e_entry;
	*elf_interpreter=0;
	for (i = 0; i < elf_ex.e_phnum; i++, eppnt++) /* mmap all loadable program headers */
	{
		if (eppnt->p_type == PT_INTERP){
			*elf_interpreter = (char *) ut_calloc(eppnt->p_filesz+1);
			fs_lseek(file, (unsigned long) eppnt->p_offset, 0);
			retval = fs_read(file, (unsigned char *) *elf_interpreter, eppnt->p_filesz);
			//ut_printf(" interpreter :%s: \n",*elf_interpreter);
			break;
		}
	}

	tmp_stack_top = setup_userstack(argv, env, stack_len, t_argc, t_argv, aux_addr, *elf_interpreter);
	*tmp_stackp=tmp_stack_top;
	if (tmp_stack_top == 0) {
		goto out;
	}
	tmp_stack_top = tmp_stack_top + (MAX_USERSPACE_STACK_TEMPLEN - *stack_len);

out:
	if (elf_phdata) {
	 	mm_free(elf_phdata);
	}
	if (tmp_stack_top==0 && *elf_interpreter!=0){
		ut_free(*elf_interpreter);
	}
	return tmp_stack_top;
}

//unsigned long fs_loadElfLibrary(struct file *file, unsigned long tmp_stack, unsigned long stack_len, unsigned long aux_addr) {
unsigned long fs_elf_load(struct file *file,unsigned long tmp_stack, unsigned long stack_len, unsigned long aux_addr) {
	struct elf_phdr *elf_phdata;
	struct elf_phdr *eppnt;
	unsigned long elf_bss, bss_start, bss, len;
	int retval, error, i, j;
	struct elfhdr elf_ex;
	Elf64_Addr p_entry;
	unsigned long *aux_vec, aux_index, load_addr;
	struct task_struct *task=g_current_task;

	error = 0;
	fs_lseek(file, 0, 0);
	retval = fs_read(file, (unsigned char *) &elf_ex, sizeof(elf_ex));
	if (retval != sizeof(elf_ex)) {
		error = -1;
		goto out;
	}

	if (ut_memcmp((unsigned char *) elf_ex.e_ident, (unsigned char *) ELFMAG, SELFMAG) != 0) {
		error = -2;
		goto out;
	}

	if (elf_ex.e_type == ET_DYN)  elf_ex.e_type=ET_EXEC;
	/* First of all, some simple consistency checks */
	//if (elf_ex.e_type != ET_EXEC || elf_ex.e_phnum > 2 ||
	if (elf_ex.e_type != ET_EXEC || !elf_check_arch(&elf_ex)) {
		DEBUG("error:(not executable type or mismatch in architecture %x  %x %x \n",elf_ex.e_type,elf_ex.e_phnum,elf_check_arch(&elf_ex));
		error = -3;
		goto out;
	}

	/* Now read in all of the header information */

	j = sizeof(struct elf_phdr) * elf_ex.e_phnum;
	/* j < ELF_MIN_ALIGN because elf_ex.e_phnum <= 2 */

	elf_phdata = mm_malloc(j, 0);
	if (!elf_phdata) {
		error = -4;
		goto out;
	}

	eppnt = elf_phdata;
	fs_lseek(file, (unsigned long) elf_ex.e_phoff, 0);
	retval = fs_read(file, (unsigned char *) eppnt, j);
	if (retval != j) {
		error = -5;
		goto out;
	}
	DEBUG("START address : %x offset :%x \n",ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_offset);
	for (j = 0, i = 0; i < elf_ex.e_phnum; i++){
		if ((eppnt + i)->p_type == PT_LOAD)
			j++;
	}
	if (j == 0) {
		error = -6;
		goto out;
	}
	load_addr = ELF_PAGESTART(eppnt->p_vaddr);
	p_entry = elf_ex.e_entry;
	task->mm->start_code = 0;
	task->mm->end_code =0;
	for (i = 0; i < elf_ex.e_phnum; i++, eppnt++) /* mmap all loadable program headers */
	{
		if (eppnt->p_type != PT_LOAD)
			continue;
		//ut_log("%d: LOAD section: vaddr:%x filesz:%x offset:%x flags:%x  \n",i,ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_filesz,eppnt->p_offset,eppnt->p_flags);
		/* Now use mmap to map the library into memory. */
		error = 1;
		if (eppnt->p_filesz > 0) {
			unsigned long addr;
			unsigned long start_addr = ELF_PAGESTART(eppnt->p_vaddr);
			unsigned long end_addr= eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr);
			addr = vm_mmap(file, start_addr, end_addr, eppnt->p_flags, 0, (eppnt->p_offset
					- ELF_PAGEOFFSET(eppnt->p_vaddr)),"text");
			if (addr == 0)
				error = 0;
			if (task->mm->start_code ==0  || task->mm->start_code > start_addr ) task->mm->start_code = start_addr;
			if (task->mm->end_code < end_addr ) task->mm->end_code = end_addr;
		}
		//if (error != ELF_PAGESTART(eppnt->p_vaddr))
		if (error != 1) {
			error = -6;
			goto out;
		}

		elf_bss = eppnt->p_vaddr + eppnt->p_filesz;
		//	padzero(elf_bss);

		/* TODO :  bss start address in not at the PAGE_ALIGN or ELF_MIN_ALIGN , need to club this partial page with the data */
	//	len = ELF_PAGESTART(eppnt->p_filesz + eppnt->p_vaddr + ELF_MIN_ALIGN - 1);
		bss_start = eppnt->p_filesz + eppnt->p_vaddr;
		bss = eppnt->p_memsz + eppnt->p_vaddr;
		//ut_log(" bss start :%x end:%x memsz:%x elf_bss:%x \n",bss_start, bss,eppnt->p_memsz,elf_bss);
		if (bss > bss_start) {
			vm_setupBrk(bss_start, bss - bss_start);
		}
		error = 0;
	}

 out:
 	if (elf_phdata) {
 		mm_free(elf_phdata);
 	}
	if (error != 0) {
		ut_log(" ERROR in elf loader filename :%s :%d\n",file->filename,-error);
	} else {
		task->mm->stack_bottom = USERSTACK_ADDR+USERSTACK_LEN;
		vm_mmap(0, USERSTACK_ADDR, USERSTACK_LEN, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, 0,"userstack");
		if (stack_len > 0) {
			aux_vec = (unsigned long *)aux_addr;
			if (aux_vec != 0) {
				int aux_last;

				aux_index = 0;
				aux_last = (MAX_AUX_VEC_ENTRIES - 2) * 2;

				AUX_ENT(AT_HWCAP, 0xbfebfbff); /* TODO: need to modify*/
				AUX_ENT(AT_PAGESZ, PAGE_SIZE);
				AUX_ENT(AT_CLKTCK, 100);
				AUX_ENT(AT_PHDR, load_addr + elf_ex.e_phoff);
				AUX_ENT(AT_PHENT, sizeof(struct elf_phdr));
				AUX_ENT(AT_PHNUM, elf_ex.e_phnum);
				AUX_ENT(AT_BASE, 0);
				AUX_ENT(AT_FLAGS, 0);
				AUX_ENT(AT_ENTRY, p_entry);

				AUX_ENT(AT_UID, 0x1f4); /* TODO : remove  UID hard coded to 0x1f4 for the next four entries  */
				AUX_ENT(AT_EUID, 0x1f4);
				AUX_ENT(AT_GID, 0x1f4);
				AUX_ENT(AT_EGID, 0x1f4);

				AUX_ENT(AT_SECURE, 0x0);

				aux_vec[aux_last] = 0x1234567887654321;
				aux_vec[aux_last + 1] = 0x1122334455667788;
				AUX_ENT(AT_RANDOM, userstack_addr((unsigned long)&aux_vec[aux_last]));

				aux_vec[aux_last + 2] = 0x34365f363878; /* This is string "x86_64" */
				AUX_ENT(AT_PLATFORM, userstack_addr((unsigned long)&aux_vec[aux_last+2]));

				//  AUX_ENT(AT_EXECFN, bprm->exec);
			}
			//ut_log(" before copy :%x :%x len:%x\n",USERSTACK_ADDR + USERSTACK_LEN - stack_len,tmp_stack,stack_len);
			ut_memcpy((unsigned char *)USERSTACK_ADDR + USERSTACK_LEN - stack_len, (unsigned char *)tmp_stack, stack_len);


			vm_mmap(0, USER_SYSCALL_PAGE, 0x1000, PROT_READ | PROT_EXEC |PROT_WRITE, MAP_ANONYMOUS, 0,"fst_syscal");
			//ut_memset((unsigned char *)SYSCALL_PAGE,(unsigned char )0xcc,0x1000);
			ut_memcpy((unsigned char *)USER_SYSCALL_PAGE,(unsigned char *)&__vsyscall_page,0x1000);
			if (g_conf_syscall_debug==1){
				pagetable_walk(4,g_current_task->mm->pgd,1);
			}
		}
	}
	DEBUG(" Program start address(autod) : %x \n",elf_ex.e_entry);

	if (error == 0)
		return p_entry;
	else
		return 0;
}
