#define DEBUG_ENABLE
#define DLINFO_ITEMS 13

#include <elf.h>
#include "mm.h"
#include "interface.h"

//static int load_elf_binary(struct linux_binprm * bprm, struct pt_regs * regs);
//static int load_elf_library(unsigned long  *);
//static unsigned long elf_map (struct file *, unsigned long, struct elf_phdr *, int, int);

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
#if 0
static struct linux_binfmt elf_format = {
	NULL, THIS_MODULE, load_elf_binary, load_elf_library, elf_core_dump, ELF_EXEC_PAGESIZE
};

#define BAD_ADDR(x)	((unsigned long)(x) >= TASK_SIZE)

static int set_brk(unsigned long start, unsigned long end)
{
	start = ELF_PAGEALIGN(start);
	end = ELF_PAGEALIGN(end);
	if (end > start) {
		unsigned long addr;
		down_write(&current->mm->mmap_sem);
		addr = do_brk(start, end - start);
		up_write(&current->mm->mmap_sem);
		if (BAD_ADDR(addr))
			return addr;
	}
	current->mm->start_brk = current->mm->brk = end;
	return 0;
}


/* We need to explicitly zero any fractional pages
   after the data section (i.e. bss).  This would
   contain the junk from the file that should not
   be in memory */


static void padzero(unsigned long elf_bss)
{
	unsigned long nbyte;

	nbyte = ELF_PAGEOFFSET(elf_bss);
	if (nbyte) {
		nbyte = ELF_MIN_ALIGN - nbyte;
		clear_user((void *) elf_bss, nbyte);
	}
}

static elf_addr_t * 
create_elf_tables(char *p, int argc, int envc,
		  struct elfhdr * exec,
		  unsigned long load_addr,
		  unsigned long load_bias,
		  unsigned long interp_load_addr, int ibcs)
{
	elf_caddr_t *argv;
	elf_caddr_t *envp;
	elf_addr_t *sp, *csp;
	char *k_platform, *u_platform;
	long hwcap;
	size_t platform_len = 0;
	size_t len;

	/*
	 * Get hold of platform and hardware capabilities masks for
	 * the machine we are running on.  In some cases (Sparc), 
	 * this info is impossible to get, in others (i386) it is
	 * merely difficult.
	 */

	hwcap = ELF_HWCAP;
	k_platform = ELF_PLATFORM;

	if (k_platform) {
		platform_len = strlen(k_platform) + 1;
		u_platform = p - platform_len;
		__copy_to_user(u_platform, k_platform, platform_len);
	} else
		u_platform = p;

#if defined(__i386__) && defined(CONFIG_SMP)
	/*
	 * In some cases (e.g. Hyper-Threading), we want to avoid L1 evictions
	 * by the processes running on the same package. One thing we can do
	 * is to shuffle the initial stack for them.
	 *
	 * The conditionals here are unneeded, but kept in to make the
	 * code behaviour the same as pre change unless we have hyperthreaded
	 * processors. This keeps Mr Marcelo Person happier but should be
	 * removed for 2.5
	 */
	 
	if(smp_num_siblings > 1)
		u_platform = u_platform - ((current->pid % 64) << 7);
#endif	

	/*
	 * Force 16 byte _final_ alignment here for generality.
	 */
	sp = (elf_addr_t *)(~15UL & (unsigned long)(u_platform));
	csp = sp;
	csp -= (1+DLINFO_ITEMS)*2 + (k_platform ? 2 : 0);
#ifdef DLINFO_ARCH_ITEMS
	csp -= DLINFO_ARCH_ITEMS*2;
#endif
	csp -= envc+1;
	csp -= argc+1;
	csp -= (!ibcs ? 3 : 1);	/* argc itself */
	if ((unsigned long)csp & 15UL)
		sp -= ((unsigned long)csp & 15UL) / sizeof(*sp);

	/*
	 * Put the ELF interpreter info on the stack
	 */
#define NEW_AUX_ENT(nr, id, val) \
	  __put_user ((id), sp+(nr*2)); \
	  __put_user ((val), sp+(nr*2+1)); \

	sp -= 2;
	NEW_AUX_ENT(0, AT_NULL, 0);
	if (k_platform) {
		sp -= 2;
		NEW_AUX_ENT(0, AT_PLATFORM, (elf_addr_t)(unsigned long) u_platform);
	}
	sp -= DLINFO_ITEMS*2;
	NEW_AUX_ENT( 0, AT_HWCAP, hwcap);
	NEW_AUX_ENT( 1, AT_PAGESZ, ELF_EXEC_PAGESIZE);
	NEW_AUX_ENT( 2, AT_CLKTCK, CLOCKS_PER_SEC);
	NEW_AUX_ENT( 3, AT_PHDR, load_addr + exec->e_phoff);
	NEW_AUX_ENT( 4, AT_PHENT, sizeof (struct elf_phdr));
	NEW_AUX_ENT( 5, AT_PHNUM, exec->e_phnum);
	NEW_AUX_ENT( 6, AT_BASE, interp_load_addr);
	NEW_AUX_ENT( 7, AT_FLAGS, 0);
	NEW_AUX_ENT( 8, AT_ENTRY, load_bias + exec->e_entry);
	NEW_AUX_ENT( 9, AT_UID, (elf_addr_t) current->uid);
	NEW_AUX_ENT(10, AT_EUID, (elf_addr_t) current->euid);
	NEW_AUX_ENT(11, AT_GID, (elf_addr_t) current->gid);
	NEW_AUX_ENT(12, AT_EGID, (elf_addr_t) current->egid);
#ifdef ARCH_DLINFO
	/* 
	 * ARCH_DLINFO must come last so platform specific code can enforce
	 * special alignment requirements on the AUXV if necessary (eg. PPC).
	 */
	ARCH_DLINFO;
#endif
#undef NEW_AUX_ENT

	sp -= envc+1;
	envp = (elf_caddr_t *) sp;
	sp -= argc+1;
	argv = (elf_caddr_t *) sp;
	if (!ibcs) {
		__put_user((elf_addr_t)(unsigned long) envp,--sp);
		__put_user((elf_addr_t)(unsigned long) argv,--sp);
	}

	__put_user((elf_addr_t)argc,--sp);
	current->mm->arg_start = current->mm->arg_end = (unsigned long) p;
	while (argc-->0) {
		__put_user((elf_caddr_t)(unsigned long)p,argv++);
		len = strnlen_user(p, PAGE_SIZE*MAX_ARG_PAGES);
		if (!len || len > PAGE_SIZE*MAX_ARG_PAGES)
			return NULL;
		p += len;
	}
	__put_user(NULL, argv);
	current->mm->arg_end = current->mm->env_start = (unsigned long) p;
	while (envc-->0) {
		__put_user((elf_caddr_t)(unsigned long)p,envp++);
		len = strnlen_user(p, PAGE_SIZE*MAX_ARG_PAGES);
		if (!len || len > PAGE_SIZE*MAX_ARG_PAGES)
			return NULL;
		p += len;
	}
	__put_user(NULL, envp);
	current->mm->env_end = (unsigned long) p;
	return sp;
}

#ifndef elf_map

static inline unsigned long
elf_map (struct file *filep, unsigned long addr, struct elf_phdr *eppnt, int prot, int type)
{
	unsigned long map_addr;

	down_write(&current->mm->mmap_sem);
	map_addr = do_mmap(filep, ELF_PAGESTART(addr),
			   eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr), prot, type,
			   eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr));
	up_write(&current->mm->mmap_sem);
	return(map_addr);
}

#endif /* !elf_map */

/* This is much more generalized than the library routine read function,
   so we keep this separate.  Technically the library read function
   is only provided so that we can read a.out libraries that have
   an ELF header */

static unsigned long load_elf_interp(struct elfhdr * interp_elf_ex,
				     struct file * interpreter,
				     unsigned long *interp_load_addr)
{
	struct elf_phdr *elf_phdata;
	struct elf_phdr *eppnt;
	unsigned long load_addr = 0;
	int load_addr_set = 0;
	unsigned long last_bss = 0, elf_bss = 0;
	unsigned long error = ~0UL;
	int retval, i, size;

	/* First of all, some simple consistency checks */
	if (interp_elf_ex->e_type != ET_EXEC &&
	    interp_elf_ex->e_type != ET_DYN)
		goto out;
	if (!elf_check_arch(interp_elf_ex))
		goto out;
	if (!interpreter->f_op || !interpreter->f_op->mmap)
		goto out;

	/*
	 * If the size of this structure has changed, then punt, since
	 * we will be doing the wrong thing.
	 */
	if (interp_elf_ex->e_phentsize != sizeof(struct elf_phdr))
		goto out;

	if (interp_elf_ex->e_phnum < 1 ||
	    interp_elf_ex->e_phnum > 65536U / sizeof(struct elf_phdr))
		goto out;

	/* Now read in all of the header information */

	size = sizeof(struct elf_phdr) * interp_elf_ex->e_phnum;
	if (size > ELF_MIN_ALIGN)
		goto out;
	elf_phdata = (struct elf_phdr *) kmalloc(size, GFP_KERNEL);
	if (!elf_phdata)
		goto out;

	retval = kernel_read(interpreter,interp_elf_ex->e_phoff,(char *)elf_phdata,size);
	error = -EIO;
	if (retval != size) {
		if (retval < 0)
			error = retval;	
		goto out_close;
	}

	eppnt = elf_phdata;
	for (i=0; i<interp_elf_ex->e_phnum; i++, eppnt++) {
	  if (eppnt->p_type == PT_LOAD) {
	    int elf_type = MAP_PRIVATE | MAP_DENYWRITE;
	    int elf_prot = 0;
	    unsigned long vaddr = 0;
	    unsigned long k, map_addr;

	    if (eppnt->p_flags & PF_R) elf_prot =  PROT_READ;
	    if (eppnt->p_flags & PF_W) elf_prot |= PROT_WRITE;
	    if (eppnt->p_flags & PF_X) elf_prot |= PROT_EXEC;
	    vaddr = eppnt->p_vaddr;
	    if (interp_elf_ex->e_type == ET_EXEC || load_addr_set)
	    	elf_type |= MAP_FIXED;

	    map_addr = elf_map(interpreter, load_addr + vaddr, eppnt, elf_prot, elf_type);
	    if (BAD_ADDR(map_addr))
	    	goto out_close;

	    if (!load_addr_set && interp_elf_ex->e_type == ET_DYN) {
		load_addr = map_addr - ELF_PAGESTART(vaddr);
		load_addr_set = 1;
	    }

	    /*
	     * Check to see if the section's size will overflow the
	     * allowed task size. Note that p_filesz must always be
	     * <= p_memsize so it is only necessary to check p_memsz.
	     */
	    k = load_addr + eppnt->p_vaddr;
	    if (BAD_ADDR(k) || eppnt->p_filesz > eppnt->p_memsz ||
		eppnt->p_memsz > TASK_SIZE || TASK_SIZE - eppnt->p_memsz < k) {
	        error = -ENOMEM;
		goto out_close;
	    }

	    /*
	     * Find the end of the file mapping for this phdr, and keep
	     * track of the largest address we see for this.
	     */
	    k = load_addr + eppnt->p_vaddr + eppnt->p_filesz;
	    if (k > elf_bss)
		elf_bss = k;

	    /*
	     * Do the same thing for the memory mapping - between
	     * elf_bss and last_bss is the bss section.
	     */
	    k = load_addr + eppnt->p_memsz + eppnt->p_vaddr;
	    if (k > last_bss)
		last_bss = k;
	  }
	}

	/* Now use mmap to map the library into memory. */

	/*
	 * Now fill out the bss section.  First pad the last page up
	 * to the page boundary, and then perform a mmap to make sure
	 * that there are zero-mapped pages up to and including the 
	 * last bss page.
	 */
	padzero(elf_bss);
	elf_bss = ELF_PAGESTART(elf_bss + ELF_MIN_ALIGN - 1);	/* What we have mapped so far */

	/* Map the last of the bss segment */
	if (last_bss > elf_bss) {
		down_write(&current->mm->mmap_sem);
		error = do_brk(elf_bss, last_bss - elf_bss);
		up_write(&current->mm->mmap_sem);
		if (BAD_ADDR(error))
			goto out_close;
	}

	*interp_load_addr = load_addr;
	/*
	 * XXX: is everything deallocated properly if this happens
	 * to be ~0UL (that is, we succeeded, but the header is broken
	 * and thus the caller will think that we failed)? We'd better
	 * switch to out-of-band error reporting.
	 */
	error = ((unsigned long) interp_elf_ex->e_entry) + load_addr;

out_close:
	kfree(elf_phdata);
out:
	return error;
}

static unsigned long load_aout_interp(struct exec * interp_ex,
			     struct file * interpreter)
{
	unsigned long text_data, elf_entry = ~0UL;
	char * addr;
	loff_t offset;

	current->mm->end_code = interp_ex->a_text;
	text_data = interp_ex->a_text + interp_ex->a_data;
	current->mm->end_data = text_data;
	current->mm->brk = interp_ex->a_bss + text_data;

	switch (N_MAGIC(*interp_ex)) {
	case OMAGIC:
		offset = 32;
		addr = (char *) 0;
		break;
	case ZMAGIC:
	case QMAGIC:
		offset = N_TXTOFF(*interp_ex);
		addr = (char *) N_TXTADDR(*interp_ex);
		break;
	default:
		goto out;
	}

	down_write(&current->mm->mmap_sem);
	do_brk(0, text_data);
	up_write(&current->mm->mmap_sem);

	if (!interpreter->f_op || !interpreter->f_op->read)
		goto out;
	if (interpreter->f_op->read(interpreter, addr, text_data, &offset) < 0)
		goto out;
	flush_icache_range((unsigned long)addr,
	                   (unsigned long)addr + text_data);

	down_write(&current->mm->mmap_sem);
	do_brk(ELF_PAGESTART(text_data + ELF_MIN_ALIGN - 1),
		interp_ex->a_bss);
	up_write(&current->mm->mmap_sem);

	elf_entry = interp_ex->a_entry;

out:
	return elf_entry;
}

/*
 * These are the functions used to load ELF style executables and shared
 * libraries.  There is no binary dependent code anywhere else.
 */

#define INTERPRETER_NONE 0
#define INTERPRETER_AOUT 1
#define INTERPRETER_ELF 2

static int load_elf_binary(struct linux_binprm * bprm, struct pt_regs * regs)
{
	struct file *interpreter = NULL; /* to shut gcc up */
 	unsigned long load_addr = 0, load_bias = 0;
	int load_addr_set = 0;
	char * elf_interpreter = NULL;
	unsigned int interpreter_type = INTERPRETER_NONE;
	unsigned char ibcs2_interpreter = 0;
	unsigned long error;
	struct elf_phdr * elf_ppnt, *elf_phdata;
	unsigned long elf_bss, k, elf_brk;
	int elf_exec_fileno;
	int retval, i;
	unsigned int size;
	unsigned long elf_entry, interp_load_addr = 0;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long reloc_func_desc = 0;
	struct elfhdr elf_ex;
	struct elfhdr interp_elf_ex;
  	struct exec interp_ex;
	char passed_fileno[6];
	struct files_struct *files;
	
	/* Get the exec-header */
	elf_ex = *((struct elfhdr *) bprm->buf);

	retval = -ENOEXEC;
	/* First of all, some simple consistency checks */
	if (ut_memcmp((unsigned char *)elf_ex.e_ident, (unsigned char *)ELFMAG, SELFMAG) != 0)
		goto out;

	if (elf_ex.e_type != ET_EXEC && elf_ex.e_type != ET_DYN)
		goto out;
	if (!elf_check_arch(&elf_ex))
		goto out;
	if (!bprm->file->f_op||!bprm->file->f_op->mmap)
		goto out;

	/* Now read in all of the header information */

	if (elf_ex.e_phentsize != sizeof(struct elf_phdr))
		goto out;
	if (elf_ex.e_phnum < 1 ||
	    elf_ex.e_phnum > 65536U / sizeof(struct elf_phdr))
		goto out;
	size = elf_ex.e_phnum * sizeof(struct elf_phdr);
	retval = -ENOMEM;
	elf_phdata = (struct elf_phdr *) kmalloc(size, GFP_KERNEL);
	if (!elf_phdata)
		goto out;

	retval = kernel_read(bprm->file, elf_ex.e_phoff, (char *) elf_phdata, size);
	if (retval != size) {
		if (retval >= 0)
			retval = -EIO;
		goto out_free_ph;
	}

	files = current->files;		/* Refcounted so ok */
	retval = unshare_files();
	if (retval < 0)
		goto out_free_ph;
	if (files == current->files) {
		put_files_struct(files);
		files = NULL;
	}

	/* exec will make our files private anyway, but for the a.out
	   loader stuff we need to do it earlier */
	   
	retval = get_unused_fd();
	if (retval < 0)
		goto out_free_fh;
	get_file(bprm->file);
	fd_install(elf_exec_fileno = retval, bprm->file);

	elf_ppnt = elf_phdata;
	elf_bss = 0;
	elf_brk = 0;

	start_code = ~0UL;
	end_code = 0;
	start_data = 0;
	end_data = 0;

	for (i = 0; i < elf_ex.e_phnum; i++) {
		if (elf_ppnt->p_type == PT_INTERP) {
			/* This is the program interpreter used for
			 * shared libraries - for now assume that this
			 * is an a.out format binary
			 */

			retval = -ENOEXEC;
			if (elf_ppnt->p_filesz > PATH_MAX || 
			    elf_ppnt->p_filesz < 2)
				goto out_free_file;

			retval = -ENOMEM;
			elf_interpreter = (char *) kmalloc(elf_ppnt->p_filesz,
							   GFP_KERNEL);
			if (!elf_interpreter)
				goto out_free_file;

			retval = kernel_read(bprm->file, elf_ppnt->p_offset,
					   elf_interpreter,
					   elf_ppnt->p_filesz);
			if (retval != elf_ppnt->p_filesz) {
				if (retval >= 0)
					retval = -EIO;
				goto out_free_interp;
			}
			/* make sure path is NULL terminated */
			retval = -ENOEXEC;
			if (elf_interpreter[elf_ppnt->p_filesz - 1] != '\0')
				goto out_free_interp;

			/* If the program interpreter is one of these two,
			 * then assume an iBCS2 image. Otherwise assume
			 * a native linux image.
			 */
			if (strcmp(elf_interpreter,"/usr/lib/libc.so.1") == 0 ||
			    strcmp(elf_interpreter,"/usr/lib/ld.so.1") == 0)
				ibcs2_interpreter = 1;
#if 0
			printk("Using ELF interpreter %s\n", elf_interpreter);
#endif

			SET_PERSONALITY(elf_ex, ibcs2_interpreter);

			interpreter = open_exec(elf_interpreter);
			retval = PTR_ERR(interpreter);
			if (IS_ERR(interpreter))
				goto out_free_interp;
			retval = kernel_read(interpreter, 0, bprm->buf, BINPRM_BUF_SIZE);
			if (retval != BINPRM_BUF_SIZE) {
				if (retval >= 0)
					retval = -EIO;
				goto out_free_dentry;
			}

			/* Get the exec headers */
			interp_ex = *((struct exec *) bprm->buf);
			interp_elf_ex = *((struct elfhdr *) bprm->buf);
			break;
		}
		elf_ppnt++;
	}

	/* Some simple consistency checks for the interpreter */
	if (elf_interpreter) {
		interpreter_type = INTERPRETER_ELF | INTERPRETER_AOUT;

		/* Now figure out which format our binary is */
		if ((N_MAGIC(interp_ex) != OMAGIC) &&
		    (N_MAGIC(interp_ex) != ZMAGIC) &&
		    (N_MAGIC(interp_ex) != QMAGIC))
			interpreter_type = INTERPRETER_ELF;

		if (memcmp(interp_elf_ex.e_ident, ELFMAG, SELFMAG) != 0)
			interpreter_type &= ~INTERPRETER_ELF;

		retval = -ELIBBAD;
		if (!interpreter_type)
			goto out_free_dentry;

		/* Make sure only one type was selected */
		if ((interpreter_type & INTERPRETER_ELF) &&
		     interpreter_type != INTERPRETER_ELF) {
	     		// FIXME - ratelimit this before re-enabling
			// printk(KERN_WARNING "ELF: Ambiguous type, using ELF\n");
			interpreter_type = INTERPRETER_ELF;
		}
		/* Verify the interpreter has a valid arch */
		if ((interpreter_type == INTERPRETER_ELF) &&
		    !elf_check_arch(&interp_elf_ex))
			goto out_free_dentry;
	} else {
		/* Executables without an interpreter also need a personality  */
		SET_PERSONALITY(elf_ex, ibcs2_interpreter);
	}

	if (BAD_ADDR(elf_ex.e_entry)) {
		retval = -ENOEXEC;
		goto out_free_dentry;
	}

	/* OK, we are done with that, now set up the arg stuff,
	   and then start this sucker up */

	if (!bprm->sh_bang) {
		char * passed_p;

		if (interpreter_type == INTERPRETER_AOUT) {
		  sprintf(passed_fileno, "%d", elf_exec_fileno);
		  passed_p = passed_fileno;

		  if (elf_interpreter) {
		    retval = copy_strings_kernel(1,&passed_p,bprm);
			if (retval)
				goto out_free_dentry; 
		    bprm->argc++;
		  }
		}
	}

	/* Flush all traces of the currently running executable */
	retval = flush_old_exec(bprm);
	if (retval)
		goto out_free_dentry;

	/* Discard our unneeded old files struct */
	if (files) {
		steal_locks(files);
		put_files_struct(files);
		files = NULL;
	}

	/* OK, This is the point of no return */
	current->mm->start_data = 0;
	current->mm->end_data = 0;
	current->mm->end_code = 0;
	current->mm->mmap = NULL;
	current->flags &= ~PF_FORKNOEXEC;
	elf_entry = (unsigned long) elf_ex.e_entry;

	/* Do this so that we can load the interpreter, if need be.  We will
	   change some of these later */
	current->mm->rss = 0;
	retval = setup_arg_pages(bprm);
	if (retval < 0) {
		send_sig(SIGKILL, current, 0);
		return retval;
	}
	
	current->mm->start_stack = bprm->p;

	/* Now we do a little grungy work by mmaping the ELF image into
	   the correct location in memory.  At this point, we assume that
	   the image should be loaded at fixed address, not at a variable
	   address. */

	for(i = 0, elf_ppnt = elf_phdata; i < elf_ex.e_phnum; i++, elf_ppnt++) {
		int elf_prot = 0, elf_flags;
		unsigned long vaddr;

		if (elf_ppnt->p_type != PT_LOAD)
			continue;

		if (unlikely (elf_brk > elf_bss)) {
			unsigned long nbyte;
	            
			/* There was a PT_LOAD segment with p_memsz > p_filesz
			   before this one. Map anonymous pages, if needed,
			   and clear the area.  */
			retval = set_brk (elf_bss + load_bias,
					  elf_brk + load_bias);
			if (retval) {
				send_sig(SIGKILL, current, 0);
				goto out_free_dentry;
			}
			nbyte = ELF_PAGEOFFSET(elf_bss);
			if (nbyte) {
				nbyte = ELF_MIN_ALIGN - nbyte;
				if (nbyte > elf_brk - elf_bss)
					nbyte = elf_brk - elf_bss;
				clear_user((void *) elf_bss + load_bias, nbyte);
			}
		}

		if (elf_ppnt->p_flags & PF_R) elf_prot |= PROT_READ;
		if (elf_ppnt->p_flags & PF_W) elf_prot |= PROT_WRITE;
		if (elf_ppnt->p_flags & PF_X) elf_prot |= PROT_EXEC;

		elf_flags = MAP_PRIVATE|MAP_DENYWRITE|MAP_EXECUTABLE;

		vaddr = elf_ppnt->p_vaddr;
		if (elf_ex.e_type == ET_EXEC || load_addr_set) {
			elf_flags |= MAP_FIXED;
		} else if (elf_ex.e_type == ET_DYN) {
			/* Try and get dynamic programs out of the way of the default mmap
			   base, as well as whatever program they might try to exec.  This
		           is because the brk will follow the loader, and is not movable.  */
			load_bias = ELF_PAGESTART(ELF_ET_DYN_BASE - vaddr);
		}

		error = elf_map(bprm->file, load_bias + vaddr, elf_ppnt, elf_prot, elf_flags);
		if (BAD_ADDR(error)) {
			send_sig(SIGKILL, current, 0);
			goto out_free_dentry;
		}

		if (!load_addr_set) {
			load_addr_set = 1;
			load_addr = (elf_ppnt->p_vaddr - elf_ppnt->p_offset);
			if (elf_ex.e_type == ET_DYN) {
				load_bias += error -
				             ELF_PAGESTART(load_bias + vaddr);
				load_addr += load_bias;
				reloc_func_desc = load_addr;
			}
		}
		k = elf_ppnt->p_vaddr;
		if (k < start_code) start_code = k;
		if (start_data < k) start_data = k;

		/*
		 * Check to see if the section's size will overflow the
		 * allowed task size. Note that p_filesz must always be
		 * <= p_memsz so it is only necessary to check p_memsz.
		 */
		if (BAD_ADDR(k) || elf_ppnt->p_filesz > elf_ppnt->p_memsz ||
		    elf_ppnt->p_memsz > TASK_SIZE ||
		    TASK_SIZE - elf_ppnt->p_memsz < k) {
			/* set_brk can never work.  Avoid overflows.  */
			send_sig(SIGKILL, current, 0);
			goto out_free_dentry;
		}

		k = elf_ppnt->p_vaddr + elf_ppnt->p_filesz;

		if (k > elf_bss)
			elf_bss = k;
		if ((elf_ppnt->p_flags & PF_X) && end_code <  k)
			end_code = k;
		if (end_data < k)
			end_data = k;
		k = elf_ppnt->p_vaddr + elf_ppnt->p_memsz;
		if (k > elf_brk)
			elf_brk = k;
	}

	elf_entry += load_bias;
	elf_bss += load_bias;
	elf_brk += load_bias;
	start_code += load_bias;
	end_code += load_bias;
	start_data += load_bias;
	end_data += load_bias;

	/* Calling set_brk effectively mmaps the pages that we need
	 * for the bss and break sections.  We must do this before
	 * mapping in the interpreter, to make sure it doesn't wind
	 * up getting placed where the bss needs to go.
	 */
	retval = set_brk(elf_bss, elf_brk);
	if (retval) {
		send_sig(SIGKILL, current, 0);
		goto out_free_dentry;
	}
	padzero(elf_bss);

	if (elf_interpreter) {
		if (interpreter_type == INTERPRETER_AOUT)
			elf_entry = load_aout_interp(&interp_ex,
						     interpreter);
		else
			elf_entry = load_elf_interp(&interp_elf_ex,
						    interpreter,
						    &interp_load_addr);
		if (BAD_ADDR(elf_entry)) {
	     		// FIXME - ratelimit this before re-enabling
			// printk(KERN_ERR "Unable to load interpreter %.128s\n",
			//        elf_interpreter);

			force_sig(SIGSEGV, current);
			retval = IS_ERR((void *)elf_entry) ?
					(int)elf_entry : -EINVAL;
			goto out_free_dentry;
		}
		reloc_func_desc = interp_load_addr;

		allow_write_access(interpreter);
		fput(interpreter);
		kfree(elf_interpreter);
	} else {
		if (BAD_ADDR(elf_entry)) {
			force_sig(SIGSEGV, current);
			retval = -EINVAL;
			goto out_free_dentry;
		}
	}

	kfree(elf_phdata);

	if (interpreter_type != INTERPRETER_AOUT)
		sys_close(elf_exec_fileno);

	set_binfmt(&elf_format);

	compute_creds(bprm);
	current->flags &= ~PF_FORKNOEXEC;
	bprm->p = (unsigned long)
	  create_elf_tables((char *)bprm->p,
			bprm->argc,
			bprm->envc,
			&elf_ex,
			load_addr, load_bias,
			interp_load_addr,
			(interpreter_type == INTERPRETER_AOUT ? 0 : 1));
	/* N.B. passed_fileno might not be initialized? */
	if (interpreter_type == INTERPRETER_AOUT)
		current->mm->arg_start += strlen(passed_fileno) + 1;
	current->mm->start_brk = current->mm->brk = elf_brk;
	current->mm->end_code = end_code;
	current->mm->start_code = start_code;
	current->mm->start_data = start_data;
	current->mm->end_data = end_data;
	current->mm->start_stack = bprm->p;

#if 0
	printk("(start_brk) %lx\n" , (long) current->mm->start_brk);
	printk("(end_code) %lx\n" , (long) current->mm->end_code);
	printk("(start_code) %lx\n" , (long) current->mm->start_code);
	printk("(start_data) %lx\n" , (long) current->mm->start_data);
	printk("(end_data) %lx\n" , (long) current->mm->end_data);
	printk("(start_stack) %lx\n" , (long) current->mm->start_stack);
	printk("(brk) %lx\n" , (long) current->mm->brk);
#endif

	if (current->personality & MMAP_PAGE_ZERO) {
		/* Why this, you ask???  Well SVr4 maps page 0 as read-only,
		   and some applications "depend" upon this behavior.
		   Since we do not have the power to recompile these, we
		   emulate the SVr4 behavior.  Sigh.  */
		/* N.B. Shouldn't the size here be PAGE_SIZE?? */
		down_write(&current->mm->mmap_sem);
		error = do_mmap(NULL, 0, 4096, PROT_READ | PROT_EXEC,
				MAP_FIXED | MAP_PRIVATE, 0);
		up_write(&current->mm->mmap_sem);
	}

#ifdef ELF_PLAT_INIT
	/*
	 * The ABI may specify that certain registers be set up in special
	 * ways (on i386 %edx is the address of a DT_FINI function, for
	 * example.  In addition, it may also specify (eg, PowerPC64 ELF)
	 * that the e_entry field is the address of the function descriptor
	 * for the startup routine, rather than the address of the startup
	 * routine itself.  This macro performs whatever initialization to
	 * the regs structure is required as well as any relocations to the
	 * function descriptor entries when executing dynamically linked apps.
	 */
	ELF_PLAT_INIT(regs, reloc_func_desc);
#endif

	start_thread(regs, elf_entry, bprm->p);
	if (current->ptrace & PT_PTRACED)
		send_sig(SIGTRAP, current, 0);
	retval = 0;
out:
	return retval;

	/* error cleanup */
out_free_dentry:
	allow_write_access(interpreter);
	if (interpreter)
		fput(interpreter);
out_free_interp:
	if (elf_interpreter)
		kfree(elf_interpreter);
out_free_file:
	sys_close(elf_exec_fileno);
out_free_fh:
	if (files) {
		put_files_struct(current->files);
		current->files = files;
	}
out_free_ph:
	kfree(elf_phdata);
	goto out;
}
#endif
/* This is really simpleminded and specialized - we are loading an
   a.out library that is given an ELF header. */

unsigned long fs_loadElfLibrary(struct file  *file)
{
	struct elf_phdr *elf_phdata;
	struct elf_phdr *eppnt;
	unsigned long elf_bss, bss, len;
	int retval, error, i, j;
	struct elfhdr elf_ex;

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
	DEBUG("start address : %x offset :%x \n",ELF_PAGESTART(eppnt->p_vaddr),eppnt->p_offset);
	fs_lseek(file,elf_ex.e_phoff,0);
	retval = fs_read(file, (char *)eppnt, j);
	if (retval != j)
	{
		error = -5;
		goto out_free_ph;
	}

	for (j = 0, i = 0; i<elf_ex.e_phnum; i++)
		if ((eppnt + i)->p_type == PT_LOAD) j++;
	//if (j != 1)
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
	}
	DEBUG(" Program start address(autod) : %x \n",elf_ex.e_entry);
	if ( error == 0)
		return elf_ex.e_entry;
	else return 0;
}
