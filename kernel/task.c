/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/task.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
//#define DEBUG_ENABLE 1
#include "common.h"
#include "descriptor_tables.h"


struct mm_struct *g_kernel_mm = 0;
task_queue_t g_task_queue;
spinlock_t g_global_lock = SPIN_LOCK_UNLOCKED("global");
unsigned long g_jiffies = 0; /* increments for every 10ms =100HZ = 100 cycles per second  */

static unsigned long free_pid_no = 1;  // TODO need to make unique when  wrap around
static task_queue_t run_queue;
static wait_queue_t timer_queue;


extern long *g_idle_stack;

static int free_mm(struct mm_struct *mm);
static unsigned long push_to_userland();
static unsigned long _schedule(unsigned long flags);
static void schedule_kernelSecondHalf();
static void schedule_userSecondHalf();
static int release_resources(struct task_struct *task, int attach_to_parent);

static struct task_struct *alloc_task_struct(void) {
	return (struct task_struct *) mm_getFreePages(0, 2); /*WARNING: do not change the size it is related TASK_SIZE, 4*4k=16k page size */
}

static void free_task_struct(struct task_struct *p) {
	mm_putFreePages((unsigned long) p, 2);
	return;
}
/************************************************
  All the below function should be called with holding lock
  ************************************************/
static inline int _add_to_runqueue(struct task_struct * p) /* Add at the first */
{
	if (p->magic_numbers[0] != MAGIC_LONG || p->magic_numbers[1] != MAGIC_LONG) /* safety check */
	{
		ut_printf(" Task Stack Got CORRUPTED task:%x :%x :%x \n", p, p->magic_numbers[0], p->magic_numbers[1]);
		BUG();
	}
	if (p->run_queue.next == 0 && p->cpu == 0xffff) { /* Avoid adding self adding in to runqueue */
		list_add_tail(&p->run_queue, &run_queue.head);
		return 1;
	}

	return 0;
}

int sc_add_to_runqueue(struct task_struct *task){
	return _add_to_runqueue(task);
}
static inline struct task_struct *_del_from_runqueue(struct task_struct *p) {
	if (p == 0) {
		struct list_head *node;

		node = run_queue.head.next;
		if (run_queue.head.next == &run_queue.head) {
			return 0;
		}

		p = list_entry(node,struct task_struct, run_queue);
		if (p == 0)
			BUG();
	}
	list_del(&p->run_queue);
	return p;
}



void Jcmd_ipi(unsigned char *arg1, unsigned char *arg2) {
	int i, j;
	int ret;

	if (arg1 == 0)
		return;
	i = ut_atoi(arg1);
	ut_printf(" sending one IPI's from:%d  to cpu :%d\n", getcpuid(), i);
	ret = apic_send_ipi_vector(i, IPI_CLEARPAGETABLE);
	ut_printf(" return value of ipi :%d \n", ret);
}
void Jcmd_prio(unsigned char *arg1,unsigned char *arg2){
	int cpu,priority;
	int ret;

	if (arg1==0 || arg2==0){
		ut_printf(" set cpu priority usage: prio <cpu> <priority>\n");
		return;
	}
	cpu = ut_atoi(arg1);
	priority = ut_atoi(arg2);
	if (cpu<MAX_CPUS && priority<255){
		g_cpu_state[cpu].cpu_priority = priority;
	}else{
		ut_log("Error : Ivalid cpu :%d or priority:%d \n",cpu,priority);
	}

}

int sc_sleep(long ticks) /* each tick is 100HZ or 10ms */
/* TODO : return number ticks elapsed  instead of 1*/
/* TODO : when multiple user level thread sleep it is not returning in correct time , it may be because the idle thread halting*/
{
	return ipc_waiton_waitqueue(&timer_queue, ticks);
}
int Jcmd_ps(char *arg1, char *arg2) {
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;
	int i,ret,len;
	unsigned char *buf;

	ut_printf("pid state generation ticks sleep_ticks mm mm_count name cpu\n");
	buf=mm_getFreePages(0,0);
	len = PAGE_SIZE;

	spin_lock_irqsave(&g_global_lock, flags);
	list_for_each(pos, &g_task_queue.head) {
		task = list_entry(pos, struct task_struct, task_queue);
		if (is_kernelThread(task))
			len = len - ut_snprintf(buf+PAGE_SIZE-len,len,"[%2x]", task->pid);
		else
			len = len - ut_snprintf(buf+PAGE_SIZE-len,len,"(%2x)", task->pid);

		len = len - ut_snprintf(buf+PAGE_SIZE-len,len,"%3d %3d %5x  %5x %4d %7s sleeptick(%3d:%d) cpu:%3d :%s: count:%d\n",task->state, task->cpu_contexts, task->ticks, task->mm, task->mm->count.counter, task->name, task->sleep_ticks,
				task->stats.ticks_consumed,task->cpu,task->mm->fs.cwd,task->count.counter);
	}
	len = len -ut_snprintf(buf+PAGE_SIZE-len,len," CPU Processors:  cpuid contexts <state 0=idle, intr-disabled > name pid\n");
	for (i=0; i<getmaxcpus(); i++){
		len = len -ut_snprintf(buf+PAGE_SIZE-len,len,"%2d:%4d <%d-%d> %7s(%d)\n",i,g_cpu_state[i].stat_total_contexts,g_cpu_state[i].active,g_cpu_state[i].intr_disabled,g_cpu_state[i].current_task->name,g_cpu_state[i].current_task->pid);
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

	ut_printf("%s",buf);

	mm_putFreePages(buf,0);
	return 1;
}

static unsigned long setup_userstack(unsigned char **argv, unsigned char **env, unsigned long *stack_len, unsigned long *t_argc, unsigned long *t_argv, unsigned long *p_aux) {
	int i, len, total_args = 0;
	int total_envs =0;
	unsigned char *p, *stack;
	unsigned long real_stack, addr;
	unsigned char *target_argv[12];
	unsigned char *target_env[12];

	if (argv == 0 && env == 0) {
		ut_printf(" ERROR in setuo_userstack argv:0\n");
		return 0;
	}
	stack = (unsigned char *) mm_getFreePages(MEM_CLEAR, 0);
	p = stack + PAGE_SIZE;
	len = 0;
	real_stack = USERSTACK_ADDR + USERSTACK_LEN;

	for (i = 0; argv[i] != 0 && i < 10; i++) {
		total_args++;
		len = ut_strlen(argv[i]);
		if ((p - len - 1) > stack) {
			p = p - len - 1;
			real_stack = real_stack - len - 1;
			DEBUG(" argument :%d address:%x \n",i,real_stack);
			ut_strcpy(p, argv[i]);
			target_argv[i] = (unsigned char *)real_stack;
		} else {
			goto error;
		}
	}
	target_argv[i] = 0;

	for (i = 0; env[i] != 0 && i < 10; i++) {
		total_envs++;
		len = ut_strlen(env[i]);
		if ((p - len - 1) > stack) {
			p = p - len - 1;
			real_stack = real_stack - len - 1;
			DEBUG(" envs :%d address:%x \n",i,real_stack);
			ut_strcpy(p, env[i]);
			target_env[i] = (unsigned char *)real_stack;
		} else {
			goto error;
		}
	}
	target_env[i] = 0;

	addr = (unsigned long)p;
	addr = (unsigned long)((addr / 8) * 8);
	p = (unsigned char *)addr;

	p = p - (MAX_AUX_VEC_ENTRIES * 16);

	real_stack = USERSTACK_ADDR + USERSTACK_LEN + p - (stack + PAGE_SIZE);
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
		goto error;
	}

	*stack_len = PAGE_SIZE - (p - stack);
	*t_argc = total_args;
	*t_argv = real_stack ;
	return (unsigned long)stack;

error: mm_putFreePages((unsigned long)stack, 0);
	return 0;
}
void SYS_sc_execve(unsigned char *file, unsigned char **argv, unsigned char **env) {
	struct mm_struct *mm,*old_mm;
	unsigned long flags;
	int i;
	unsigned long main_func;
	unsigned long t_argc, t_argv, stack_len, tmp_stack, tmp_aux;

	SYSCALL_DEBUG("execve file:%s argv:%x env:%x \n",file,argv,env);
	/* create the argc and env in a temporray stack before we destory the old stack */
	t_argc = 0;
	t_argv = 0;
	tmp_stack = setup_userstack(argv, env, &stack_len, &t_argc, &t_argv, &tmp_aux);
	if (tmp_stack == 0) {
		return;
	}

	/* delete old vm and create a new one */
	mm = mm_slab_cache_alloc(mm_cachep, 0);
	if (mm == 0)
		BUG();
	ut_memset((unsigned char *)mm,0,sizeof(struct mm_struct)); /* clear the mm struct */
	atomic_set(&mm->count,1);
	mm->pgd = 0;
	mm->mmap = 0;
	/* these are for user level threads */
	for (i=0; i<g_current_task->mm->fs.total;i++){
		mm->fs.filep[i]=fs_dup(g_current_task->mm->fs.filep[i],0);
	}
	mm->fs.total = g_current_task->mm->fs.total;
	mm->fs.input_device = DEVICE_SERIAL;
	mm->fs.output_device = 	DEVICE_SERIAL;
	ut_strcpy(mm->fs.cwd,g_current_task->mm->fs.cwd);
	/* every process page table should have soft links to kernel page table */
	if (ar_dup_pageTable(g_kernel_mm, mm)!=1){
		BUG();
	}

	mm->exec_fp = (struct file *)fs_open(file, 0, 0);
	if (mm->exec_fp != 0 && mm->exec_fp->inode!= 0)
		mm->exec_fp->inode->flags = mm->exec_fp->inode->flags | INODE_EXECUTING ;
	ut_strncpy(g_current_task->name, file, MAX_TASK_NAME);


	release_resources(g_current_task, 0);

	old_mm=g_current_task->mm;
	g_current_task->mm = mm;

	 /* from this point onwards new address space comes into picture, no memory belonging to previous address space like file etc should  be used */
	flush_tlb(mm->pgd);
	free_mm(old_mm);

	/* check for symbolic link */
	if (mm->exec_fp != 0 && (mm->exec_fp->inode->file_type != REGULAR_FILE)) {
		struct fileStat  file_stat;
		unsigned char newfilename[MAX_FILENAME];
		fs_stat(mm->exec_fp,&file_stat);
		if (mm->exec_fp->inode->file_type == SYM_LINK_FILE){
			if (fs_read(mm->exec_fp, newfilename, MAX_FILENAME)!=0){
				fs_close(mm->exec_fp);
				mm->exec_fp = (struct file *)fs_open(newfilename, 0, 0);
			}
		}
	}

	/* populate vm with vmaps */
	if (mm->exec_fp == 0) {
		mm_putFreePages(tmp_stack, 0);
		ut_printf("Error execve : Failed to open the file \n");
		SYS_sc_exit(701);
		return;
	}
	main_func = fs_loadElfLibrary(mm->exec_fp, tmp_stack + (PAGE_SIZE - stack_len), stack_len, tmp_aux);
	if (main_func == 0) {
		mm_putFreePages(tmp_stack, 0);
		ut_printf("Error execve : ELF load Failed \n");
		SYS_sc_exit(703);
		return ;
	}
	mm_putFreePages(tmp_stack, 0);

	//Jcmd_vmaps_stat(0, 0);

	g_current_task->thread.userland.ip = main_func;
	g_current_task->thread.userland.sp = t_argv;
	g_current_task->thread.userland.argc = t_argc;
	g_current_task->thread.userland.argv = t_argv;

	g_current_task->thread.userland.user_stack = USERSTACK_ADDR + USERSTACK_LEN;
	g_current_task->thread.userland.user_ds = 0;
	g_current_task->thread.userland.user_es = 0;
	g_current_task->thread.userland.user_gs = 0;
	g_current_task->thread.userland.user_fs = 0;

	g_current_task->thread.userland.user_fs_base = 0;
	ar_updateCpuState(g_current_task);

	push_to_userland();
}


#define DEFAULT_RFLAGS_VALUE (0x10202)
#define GLIBC_PASSING 1
extern void enter_userspace();
static unsigned long push_to_userland() {
	struct user_regs *p;
	int cpuid=getcpuid();
	local_apic_send_eoi(); // Re-enable the APIC interrupts

	DEBUG(" from PUSH113_TO_USERLAND :%d\n",cpuid);
	/* From here onwards DO NOT  call any function that consumes stack */
	asm("cli");
	asm("movq %%rsp,%0" : "=m" (p));
	p = p - 1;
	asm("subq $0xa0,%rsp");
	p->gpres.rbp = 0;
#ifndef GLIBC_PASSING
	p->gpres.rsi=g_current_task->thread.userland.sp; /* second argument to main i.e argv */
	p->gpres.rdi=g_current_task->thread.userland.argc; /* first argument argc */
#else
	p->gpres.rsi = 0;
	p->gpres.rdi = 0;
#endif
	p->gpres.rdx = 0;
	p->gpres.rbx = p->gpres.rcx = 0;
	p->gpres.rax = p->gpres.r10 = 0;
	p->gpres.r11 = p->gpres.r12 = 0;
	p->gpres.r13 = p->gpres.r14 = 0;
	p->gpres.r15 = p->gpres.r9 = 0;
	p->gpres.r8 = 0;
	p->isf.rip = g_current_task->thread.userland.ip;
	p->isf.rflags = DEFAULT_RFLAGS_VALUE;
	p->isf.rsp = g_current_task->thread.userland.sp;
	p->isf.cs = GDT_SEL(UCODE_DESCR) | SEG_DPL_USER;
	p->isf.ss = GDT_SEL(UDATA_DESCR) | SEG_DPL_USER;


	g_cpu_state[cpuid].md_state.user_fs = 0;
	g_cpu_state[cpuid].md_state.user_gs = 0;
	g_cpu_state[cpuid].md_state.user_fs_base = 0;
	g_current_task->callstack_top =0;
	enter_userspace();
	return 0;
}
static unsigned long clone_push_to_userland() {
	struct user_regs *p;

	DEBUG("CLONE from PUSH113_TO_USERLAND :%d\n",getcpuid());
	/* From here onwards DO NOT  call any function that consumes stack */
	asm("cli");
	asm("movq %%rsp,%0" : "=m" (p));
	p = p - 1;
	asm("subq $0xa0,%rsp");

	ut_memcpy(p,&(g_current_task->thread.user_regs),sizeof(struct user_regs));

	p->gpres.rax =0 ; /* return of the clone call */
	p->isf.rflags = DEFAULT_RFLAGS_VALUE;
	if (g_current_task->thread.userland.sp != 0)
	    p->isf.rsp = g_current_task->thread.userland.sp;
	p->isf.cs = GDT_SEL(UCODE_DESCR) | SEG_DPL_USER;
	p->isf.ss = GDT_SEL(UDATA_DESCR) | SEG_DPL_USER;
	g_current_task->callstack_top =0;
	enter_userspace();
	return 0;
}
static int free_mm(struct mm_struct *mm) {
	int ret;
	DEBUG("freeing the mm :%x counter:%x \n",mm,mm->count.counter);

	if (mm->count.counter == 0)
		BUG();
	atomic_dec(&mm->count);
	if (mm->count.counter > 0)
		return 0;

	unsigned long vm_space_size = ~0;
	vm_munmap(mm, 0, vm_space_size);
	DEBUG(" tid:%x Freeing the final PAGE TABLE :%x\n",g_current_task->pid,vm_space_size);
	ret=ar_pageTableCleanup(mm, 0, vm_space_size);
	if (ret != 1 ){
		ut_printf("ERROR : clear the pagetables :%d: \n",ret);
	}

	mm_slab_cache_free(mm_cachep, mm);
	return 1;
}
#define CLONE_VM 0x100
unsigned long sc_createKernelThread(int(*fn)(void *), unsigned char *args, unsigned char *thread_name) {
	unsigned long pid;
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;

	pid = SYS_sc_clone(CLONE_VM,0,0, fn, args);

	if (thread_name == 0)
		return pid;
	spin_lock_irqsave(&g_global_lock, flags);
	list_for_each(pos, &g_task_queue.head) {
		task = list_entry(pos, struct task_struct, task_queue);
		if (task->pid == pid) {
			ut_strncpy(task->name, thread_name, MAX_TASK_NAME);
			goto last;
		}
	}
last:
	spin_unlock_irqrestore(&g_global_lock, flags);

	return pid;
}
static void init_task_struct(struct task_struct *p,struct mm_struct *mm){
	atomic_set(&p->count,1);
	p->mm = mm; /* TODO increase the corresponding count */
	p->locks_sleepable = 0;
	p->locks_nonsleepable = 0;
	p->trace_on = 0;
	p->callstack_top = 0;
	p->ticks = 0;
	p->cpu_contexts = 0;
	INIT_LIST_HEAD(&(p->dead_tasks.head));
	p->pending_signal = 0;
	p->exit_code = 0;
	p->cpu = getcpuid();
	p->thread.userland.user_fs = 0;
	p->thread.userland.user_fs_base = 0;

	p->cpu =0xffff;
	p->ppid = g_current_task->pid;
	p->trace_stack_length = 10; /* some initial stack length, we do not know at what point tracing starts */
	p->run_queue.next = 0;
	p->run_queue.prev = 0;
	p->task_queue.next = 0;
	p->task_queue.prev = 0;
	p->wait_queue.next = p->wait_queue.prev = NULL;
	p->stats.ticks_consumed = 0;
	/* link to queue */
	free_pid_no++;
	if (free_pid_no==0) free_pid_no++;
	p->pid = free_pid_no;
}
unsigned long SYS_sc_clone( int clone_flags, void *child_stack, void *pid, void *ctid,  void *args) {
	struct task_struct *p;
	struct mm_struct *mm;
	unsigned long flags;
	void *fn=ctid;
	unsigned long ret_pid=0;

	SYSCALL_DEBUG("clone ctid:%x child_stack:%x flags:%x args:%x \n",fn,child_stack,clone_flags,args);
	/* Initialize the stack  */
	p = alloc_task_struct();
	if (p == 0)
		BUG();
	ut_memset((unsigned char *) p, MAGIC_CHAR, TASK_SIZE);


	/* Initialize mm */
	if (clone_flags & CLONE_VM) /* parent and child run in the same vm */
	{
		mm = g_current_task->mm;
		atomic_inc(&mm->count);
		DEBUG("clone  CLONE_VM the mm :%x counter:%x \n",mm,mm->count.counter);
	} else {
		int i;
		mm = mm_slab_cache_alloc(mm_cachep, 0);
		if (mm == 0)
			BUG();
		ut_memset(mm,0,sizeof(struct mm_struct));
		atomic_set(&mm->count,1);
		DEBUG("clone  the mm :%x counter:%x \n",mm,mm->count.counter);
		mm->pgd = 0;
		mm->mmap = 0;
		for (i=0; i<g_current_task->mm->fs.total;i++){
			mm->fs.filep[i]=fs_dup(g_current_task->mm->fs.filep[i],0);
		}
		mm->fs.total = g_current_task->mm->fs.total;
		mm->fs.input_device = g_current_task->mm->fs.input_device;
		mm->fs.output_device = g_current_task->mm->fs.output_device;
		ut_strcpy(mm->fs.cwd,g_current_task->mm->fs.cwd);

		DEBUG("BEFORE duplicating pagetables and vmaps \n");
		ar_dup_pageTable(g_current_task->mm, mm);
		vm_dup_vmaps(g_current_task->mm, mm);
		DEBUG("AFTER duplicating pagetables and vmaps\n");

		mm->exec_fp = 0; // TODO : need to be cloned
	}
	/* initialize task struct */
	init_task_struct(p,mm);
	if (g_current_task->mm != g_kernel_mm) { /* user level thread */
		ut_memcpy(&(p->thread.userland), &(g_current_task->thread.userland), sizeof(struct user_thread));
		ut_memcpy(&(p->thread.user_regs),g_cpu_state[getcpuid()].md_state.kernel_stack-sizeof(struct user_regs),sizeof(struct user_regs));

		DEBUG(" userland  ip :%x \n",p->thread.userland.ip);
		p->thread.userland.sp = (unsigned long)child_stack;
		p->thread.userland.user_stack = (unsigned long)child_stack;

		DEBUG(" child ip:%x stack:%x \n",p->thread.userland.ip,p->thread.userland.sp);
		DEBUG("userspace rip:%x rsp:%x \n",p->thread.user_regs.isf.rip,p->thread.user_regs.isf.rsp);
		//p->thread.userland.argc = 0;/* TODO */
		//p->thread.userland.argv = 0; /* TODO */
		save_flags(p->flags);
		p->thread.ip = (void *) schedule_userSecondHalf;
	} else { /* kernel level thread */
		p->thread.ip = (void *) schedule_kernelSecondHalf;
		save_flags(p->flags);
		p->thread.argv = args;
		p->thread.real_ip =fn;
	}
	p->thread.sp = (void *)((addr_t) p + (addr_t) TASK_SIZE - (addr_t)160); /* 160 bytes are left at the bottom of the stack */
	p->state = TASK_RUNNING;
	ut_strncpy(p->name, g_current_task->name, MAX_TASK_NAME);

	ret_pid=p->pid;
	spin_lock_irqsave(&g_global_lock, flags);
	list_add_tail(&p->task_queue, &g_task_queue.head);
	if (_add_to_runqueue(p)==0) {
		BUG();
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

	SYSCALL_DEBUG("clone return pid :%d \n",ret_pid);
	return ret_pid;
}
unsigned long SYS_sc_fork() {
	SYSCALL_DEBUG("fork \n");
	return SYS_sc_clone(CLONE_VM, 0, 0,  0, 0);
}
static int release_resources(struct task_struct *child_task, int attach_to_parent){
	int i;
	struct mm_struct *mm;
	struct list_head *pos;
	struct task_struct *task;
	unsigned long flags;

	/* 1. release files */
	mm = child_task->mm;
 	if (mm->count.counter > 1) return 0;
	for (i = 0; i < mm->fs.total; i++) {
		DEBUG("FREEing the files :%d \n",i);
		fs_close(mm->fs.filep[i]);
	}
	if (mm->exec_fp != 0)
		fs_close(mm->exec_fp);

	mm->fs.total = 0;
	mm->exec_fp=0;

	/* 2. relase locks */
	if (child_task->locks_sleepable > 0){ // TODO : need to check the mutex held
		ut_log("ERROR:  lock held by the process: %s \n",task->name);
		ipc_release_resources(child_task);
	}

	if (attach_to_parent == 0) return 1;

	spin_lock_irqsave(&g_global_lock, flags);
	child_task->state = TASK_DEAD;
	list_for_each(pos, &g_task_queue.head) {
		task = list_entry(pos, struct task_struct, task_queue);
		if (task->pid == child_task->ppid && task->state!=TASK_DEAD) {
			atomic_inc(&child_task->count);
			list_del(&child_task->wait_queue);
			list_add_tail(&child_task->wait_queue, &(task->dead_tasks.head)); /* wait queue is overloaded, queue yourself to your parent */

			while(1){
				if (list_empty(&(child_task->dead_tasks.head.next))) {
					break;
				} else {
					list_move(&(child_task->dead_tasks.head), &(task->dead_tasks.head));
				}
			}

			goto last;
		}
	}
last:
    spin_unlock_irqrestore(&g_global_lock, flags);
    DEBUG("dead child attached to parent\n");
	return 1;
}
int SYS_sc_exit(int status) {
	unsigned long flags;
	SYSCALL_DEBUG("sys exit : status:%d \n",status);
	ut_log(" pid:%d existed cause:%d \n",g_current_task->pid,status);
	ar_updateCpuState(g_current_task);

	release_resources(g_current_task, 1);

	spin_lock_irqsave(&g_global_lock, flags);
	g_current_task->state = TASK_DEAD; /* this should be last statement before schedule */
	g_current_task->exit_code = status;
	spin_unlock_irqrestore(&g_global_lock, flags);

	sc_schedule();
	return 0;
}
int SYS_sc_kill(unsigned long pid, unsigned long signal) {
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;
	int ret = SYSCALL_FAIL;

	SYSCALL_DEBUG("kill pid:%d signal:%d \n",pid,signal);

	spin_lock_irqsave(&g_global_lock, flags);
	list_for_each(pos, &g_task_queue.head) {
		task = list_entry(pos, struct task_struct, task_queue);
		if (task->pid == pid) {
			task->pending_signal = 1;
			ret = SYSCALL_SUCCESS;
			break;
		}
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
	return ret;
}
/******************* schedule related functions **************************/

static struct task_struct *__switch_to(struct task_struct *prev_p, struct task_struct *next_p) {
	return prev_p;
}
#ifdef ARCH_X86_64
#define __STR(x) #x
#define STR(x) __STR(x)

#define __PUSH(x) "pushq %%" __STR(x) "\n\t"
#define __POP(x)  "popq  %%" __STR(x) "\n\t"

#define SAVE_CONTEXT \
	__PUSH(rsi) __PUSH(rdi) \
__PUSH(r12) __PUSH(r13) __PUSH(r14) __PUSH(r15)  \
__PUSH(rdx) __PUSH(rcx) __PUSH(r8) __PUSH(r9) __PUSH(r10) __PUSH(r11)  \
__PUSH(rbx) __PUSH(rbp) 
#define RESTORE_CONTEXT \
	__POP(rbp) __POP(rbx) \
__POP(r11) __POP(r10) __POP(r9) __POP(r8) __POP(rcx) __POP(rdx) \
__POP(r15) __POP(r14) __POP(r13) __POP(r12) \
__POP(rdi) __POP(rsi)
#define switch_to(prev, next, last)              \
	do {                                                                    \
		/*                                                              \
		 * Context-switching clobbers all registers, so we clobber      \
		 * them explicitly, via unused output variables.                \
		 * (EAX and EBP is not listed because EBP is saved/restored     \
		 * explicitly for wchan access and EAX is the return value of   \
		 * __switch_to())                                               \
		 */                                                             \
		unsigned long rbx, rcx, rdx, rsi, rdi;                          \
		\
		asm volatile("pushfq\n\t"               /* save    flags */     \
				SAVE_CONTEXT \
				"pushq %%rbp\n\t"          /* save    EBP   */     \
				"movq %%rsp,%[prev_sp]\n\t"        /* save    ESP   */ \
				"movq %[next_sp],%%rsp\n\t"        /* restore ESP   */ \
				"movq $1f,%[prev_ip]\n\t"  /* save    EIP   */     \
				"pushq %[next_ip]\n\t"     /* restore EIP   */     \
				"jmp __switch_to\n"        /* regparm call  */     \
				"1:\t"                                             \
				"popq %%rbp\n\t"           /* restore EBP   */     \
				RESTORE_CONTEXT \
				"popfq\n"                  /* restore flags */     \
				\
				/* output parameters */                            \
				: [prev_sp] "=m" (prev->thread.sp),                \
				[prev_ip] "=m" (prev->thread.ip),                \
				"=a" (last),                                     \
				\
				/* clobbered output registers: */                \
				"=b" (rbx), "=c" (rcx), "=d" (rdx),              \
				"=S" (rsi), "=D" (rdi)                           \
		\
		\
		/* input parameters: */                          \
		: [next_sp]  "m" (next->thread.sp),                \
		[next_ip]  "m" (next->thread.ip),                \
		\
		/* regparm parameters for __switch_to(): */      \
		[prev]     "a" (prev),                           \
		[next]     "d" (next)                            \
		\
		\
		: /* reloaded segment registers */                 \
		"memory");                                      \
	} while (0)                      
#else
#endif
void ipi_interrupt(){ /* Do nothing, this is just wake up the core when it is executing HLT instruction  */

}
extern kmem_cache_t *g_slab_filep;
extern void ipi_pagetable_interrupt();
extern void *g_print_lock;
int init_tasking(unsigned long unused) {
	int i;
	unsigned long task_addr;

	g_global_lock.recursion_allowed = 0; /* this lock ownership will be transfer while task scheduling */
	g_current_task->pid = 1 ;/* hardcoded pid to make g_global _lock work */
	vm_area_cachep = kmem_cache_create("vm_area_struct",sizeof(struct vm_area_struct), 0,0, NULL, NULL);
	mm_cachep = kmem_cache_create("mm_struct",sizeof(struct mm_struct), 0,0,NULL,NULL);

	g_kernel_mm = mm_slab_cache_alloc(mm_cachep, 0);
	if (g_kernel_mm == 0)
		return -1;

	init_ipc();

	g_inode_lock = mutexCreate("mutex_vfs");
	g_print_lock = mutexCreate("mutex_print");

	INIT_LIST_HEAD(&(run_queue.head));
	INIT_LIST_HEAD(&(g_task_queue.head));
	ipc_register_waitqueue(&timer_queue,"timer");

	atomic_set(&g_kernel_mm->count,1);
	g_kernel_mm->mmap = 0x0;
	g_kernel_mm->pgd =  g_kernel_page_dir;
	g_kernel_mm->fs.total = 3;
	for (i=0; i<g_kernel_mm->fs.total; i++){
		g_kernel_mm->fs.filep[i]=mm_slab_cache_alloc(g_slab_filep, 0);
		if (i==0){
			g_kernel_mm->fs.filep[i]->type = IN_FILE;
		}else{
			g_kernel_mm->fs.filep[i]->type = OUT_FILE;
		}
	}
	g_kernel_mm->fs.input_device = DEVICE_KEYBOARD;
	g_kernel_mm->fs.output_device = DEVICE_DISPLAY_VGI;
	ut_strcpy(g_kernel_mm->fs.cwd,"/");

	free_pid_no = 1; /* pid should never be 0 */
    task_addr=(unsigned long )((unsigned long )(&g_idle_stack)+TASK_SIZE) & (~((unsigned long )(TASK_SIZE-1)));
    ut_log("	Task Addr start :%x  stack:%x current:%x\n",task_addr,&task_addr,g_current_task);
	for (i = 0; i < MAX_CPUS; i++) {
		g_cpu_state[i].md_state.cpu_id = i;
		g_cpu_state[i].idle_task = (unsigned char *)(task_addr)+i*TASK_SIZE;
		ut_memset((unsigned char *) g_cpu_state[i].idle_task, MAGIC_CHAR, TASK_SIZE-PAGE_SIZE/2);
		init_task_struct(g_cpu_state[i].idle_task,g_kernel_mm);

		g_cpu_state[i].idle_task->state = TASK_RUNNING;
		g_cpu_state[i].idle_task->cpu = i;
		g_cpu_state[i].current_task = g_cpu_state[i].idle_task;
		g_cpu_state[i].dead_task = 0;
		g_cpu_state[i].stat_total_contexts = 0;
		g_cpu_state[i].active = 1; /* by default when the system starts all the cpu are in active state */
		g_cpu_state[i].intr_disabled = 0; /* interrupts are active */
		g_cpu_state[i].cpu_priority = 0;
		ut_strncpy(g_cpu_state[i].idle_task->name, (unsigned char *)"idle", MAX_TASK_NAME);
	}
    g_current_task->cpu=0;

	ar_archSetUserFS(0);
//	init_timer(); //currently apic timer is in use
	ar_registerInterrupt(IPI_INTERRUPT, &ipi_interrupt, "IPI_GENERIC", NULL);
	ar_registerInterrupt(IPI_CLEARPAGETABLE, &ipi_pagetable_interrupt, "IPI_PTABLE", NULL);
	return 0;
}
/* This function should not block, if it block then the idle thread may block */
void sc_delete_task(struct task_struct *task) {
	unsigned long intr_flags;

	if (task ==0) return;
	atomic_dec(&task->count);
	if (task->count.counter != 0){
		DEBUG("Not deleted Task :%d\n",task->pid);
		return;
	}

	DEBUG("DELETING TASK :%x\n",task->pid);
	//ut_log("DELETING TASK deleting task id:%d name:%s  curretask:%s\n",task->pid,task->name,g_current_task->name);

	spin_lock_irqsave(&g_global_lock, intr_flags);
	list_del(&task->wait_queue);
	list_del(&task->run_queue);
	list_del(&task->task_queue);
	while (1) {
		struct list_head *node;
		struct task_struct *child_task = 0;

		node = task->dead_tasks.head.next;
		if (list_empty(&(task->dead_tasks.head.next))) {
			break;
		} else {
			child_task = list_entry(node,struct task_struct, wait_queue);
			if (child_task == 0)
				BUG();
			list_del(&child_task->wait_queue);
		}

		spin_unlock_irqrestore(&g_global_lock, intr_flags);
		sc_delete_task(child_task);
		spin_lock_irqsave(&g_global_lock, intr_flags);

		if (!list_empty(&(task->dead_tasks.head.next))) {
			BUG();
		}
	}

	spin_unlock_irqrestore(&g_global_lock, intr_flags);

	free_mm(task->mm);
	free_task_struct(task);
}

#ifdef SMP
/* getcpuid func is defined in smp code */
#else
int getcpuid(){
	return 0;
}
int getmaxcpus(){
	return 1;
}
#endif

static struct task_struct * _get_dead_task(){
	struct task_struct *task=0;

	int cpuid=getcpuid();
	if (g_cpu_state[cpuid].dead_task!=0) {
		task=g_cpu_state[cpuid].dead_task;
		g_cpu_state[cpuid].dead_task=0;
	}else{
		task=0;
	}
	return task;
}
static void schedule_userSecondHalf(){ /* user thread second Half: _schedule function function land here. */
	struct task_struct *task = _get_dead_task();
	spin_unlock_irqrestore(&g_global_lock, g_current_task->flags);
	if (task != 0)
		sc_delete_task(task);

	ar_updateCpuState(g_current_task);
	clone_push_to_userland();
}
static void schedule_kernelSecondHalf(){ /* kernel thread second half:_schedule function task can lands here. */
	struct task_struct *task = _get_dead_task();
	spin_unlock_irqrestore(&g_global_lock, g_current_task->flags);
	if (task != 0)
		sc_delete_task(task);
	g_current_task->thread.real_ip(0);
}

#define MAX_DEAD_TASKS 20
static struct task_struct *dead_tasks[MAX_DEAD_TASKS];
void sc_schedule() { /* _schedule function task can land here. */
	unsigned long intr_flags;
	int i;

	int cpuid=getcpuid();

	if (!g_current_task) {
		BUG();
		return;
	}

	if (g_current_task->cpu!=cpuid || g_current_task->magic_numbers[0] != MAGIC_LONG || g_current_task->magic_numbers[1] != MAGIC_LONG) /* safety check */
	{
		DEBUG(" Task Stack got CORRUPTED task:%x :%x :%x \n",g_current_task,g_current_task->magic_numbers[0],g_current_task->magic_numbers[1]);
		BUG();
	}
#if 1
	if (g_current_task->pending_signal != 0 && g_current_task->state != TASK_KILLING){ /* TODO: need handle the signal properly, need to merge this code with sc_check_signal, sc_check_signal may not be called if there is no syscall  */
		g_current_task->state = TASK_KILLING;
		g_current_task->exit_code = 9;
		release_resources(g_current_task, 1);
		g_current_task->state = TASK_DEAD;
	}
#endif

	spin_lock_irqsave(&g_global_lock, intr_flags);
	intr_flags=_schedule(intr_flags);
	struct task_struct *task = _get_dead_task();
	spin_unlock_irqrestore(&g_global_lock, intr_flags);

	if (task != 0){
		if (task->state == TASK_DEAD){
			sc_delete_task(task);
		}else{
			BUG();
		}
	}
}
void sc_before_syscall() {
	g_current_task->callstack_top = 0;
}
void sc_after_syscall() {

	/* Handle any pending signal */
	if (g_current_task->pending_signal == 0) {
		return;
	}
	if (is_kernelThread(g_current_task)) {
		ut_printf(" WARNING: kernel thread cannot be killed \n");
	} else {
		g_current_task->pending_signal = 0;
		SYS_sc_exit(9);
		return;
	}
	g_current_task->pending_signal = 0;
	g_current_task->callstack_top = 0;
    sc_schedule();
}
void Jcmd_cpu_active(unsigned char *arg1,unsigned char *arg2){
	int cpu,state;

	if (arg1==0 || arg2==0){
		ut_printf(" cpu_active <cpu> <active=0/1>");
		return ;
	}
	cpu=ut_atoi(arg1);
	state=ut_atoi(arg2);
	if (cpu>getmaxcpus() || state>1 || cpu<1 || state<0){
		ut_printf(" Invalid values cpu:%d state:%d valid cpus:1-%d state:0,1 \n",cpu,state,getmaxcpus());
		return ;
	}
	if (state == 1 && g_cpu_state[cpu].active==0){
		g_cpu_state[cpu].active = state;
	}else{
	    g_cpu_state[cpu].active = state;
	}
    ut_printf(" CPU %d changed state to %d \n",cpu,state);
	return ;
}
static unsigned long  _schedule(unsigned long flags) {
	struct task_struct *prev, *next;
	int cpuid=getcpuid();

	g_current_task->ticks++;
	prev = g_current_task;

	/* Only Active cpu will pickup the task, others runs idle task with external interrupts disable */
	if ((cpuid != 0) && (g_cpu_state[cpuid].active == 0)) { /* non boot cpu can sit idle without picking any tasks */
		next = g_cpu_state[cpuid].idle_task;
		apic_disable_partially();
		g_cpu_state[cpuid].intr_disabled = 1;
	} else {
		next = _del_from_runqueue(0);
		if ((g_cpu_state[cpuid].intr_disabled == 1) && (cpuid != 0) && (g_cpu_state[cpuid].active == 1)){
			/* re-enable the apic */
			apic_reenable();
			g_cpu_state[cpuid].intr_disabled = 0;
		}
	}

	if (next == 0 && (prev->state!=TASK_RUNNING)){
		next = g_cpu_state[cpuid].idle_task;
	}

	if (next ==0 ) /* by this point , we will always have some next */
		next = prev;

	g_cpu_state[cpuid].current_task = next;
#ifdef SMP   // SAFE Check
	if (g_cpu_state[0].idle_task==g_cpu_state[1].current_task || g_cpu_state[0].current_task==g_cpu_state[1].idle_task){
		ut_printf("ERROR  cpuid :%d  %d\n",cpuid,getcpuid());
		while(1);
	}
	if (g_cpu_state[0].current_task==g_cpu_state[1].current_task){
		while(1);
	}
#endif


	/* if prev and next are same then return */
	if (prev == next) {
		return flags;
	}else{
		if (next != g_cpu_state[cpuid].idle_task){
			g_cpu_state[cpuid].stat_nonidle_contexts++;
			  apic_set_task_priority(g_cpu_state[cpuid].cpu_priority);
		}else{
			  apic_set_task_priority(g_cpu_state[cpuid].cpu_priority);
		}
	}
	/* if  prev and next are having same address space , then avoid tlb flush */
	next->counter = 5; /* 50 ms time slice */
	if (prev->mm->pgd != next->mm->pgd) /* TODO : need to make generic */
	{
		flush_tlb(next->mm->pgd);
	}
	if (prev->state == TASK_DEAD) {
		if (g_cpu_state[cpuid].dead_task != NULL){
			BUG(); //Hitting
		}
		g_cpu_state[cpuid].dead_task = prev;
	}else if (prev!=g_cpu_state[cpuid].idle_task && prev->state==TASK_RUNNING){ /* some other cpu  can pickup this task , running task and idle task should not be in a run equeue even though there state is running */
		if (prev->run_queue.next != 0){ /* Prev should not be on the runqueue */
			BUG();
		}
		list_add_tail(&prev->run_queue, &run_queue.head);
    }
	next->cpu = cpuid; /* get cpuid based on this */
	next->cpu_contexts++;
	g_cpu_state[cpuid].stat_total_contexts++;
	arch_spinlock_transfer(&g_global_lock,prev,next);
	/* update the cpu state  and tss state for system calls */
	ar_updateCpuState(next);
	ar_setupTssStack((unsigned long) next + TASK_SIZE);


	prev->flags = flags;
	prev->cpu=0xffff;
	/* finally switch the task */
	switch_to(prev, next, prev);

	/* from the next statement onwards should not use any stack variables, new threads launched will not able see next statements*/
	return g_current_task->flags;
}

void do_softirq() {
	if (g_current_task->counter <= 0) {
		sc_schedule();
	}
}
unsigned long g_jiffie_errors=0;
unsigned long g_jiffie_tick=0;
void timer_callback(registers_t regs) {
	int i;
	unsigned long flags;

	/* 1. increment timestamp */
	if (getcpuid()==0){
		unsigned long kvm_ticks=get_kvm_clock();
		if ((kvm_ticks!=0) && (g_jiffie_tick>(kvm_ticks+10)) ){
			g_jiffie_errors++;
		}else{
			g_jiffie_tick++;
			g_jiffies++;
		}
	}
	g_current_task->counter--;
	g_current_task->stats.ticks_consumed++;

	/* 2. Test of wait queues for any expiry. time queue is one of the wait queue  */
	ipc_check_waitqueues();

	do_softirq();
}


