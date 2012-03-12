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
#define DEBUG_ENABLE 1
#include "task.h"
#include "interface.h"
#include "descriptor_tables.h"
#define MAGIC_CHAR 0xab
#define MAGIC_LONG 0xabababababababab

struct task_struct *g_current_task,*g_idle_task;
struct mm_struct *g_kernel_mm=0;

typedef struct queue{
	struct list_head head;
	atomic_t count;
}queue_t;
static queue_t run_queue;
static queue_t task_queue;
static spinlock_t sched_lock  = SPIN_LOCK_UNLOCKED;
struct wait_struct g_timerqueue;
unsigned long g_pid=0;
unsigned long g_jiffies = 0; /* increments for every 10ms =100HZ = 100 cycles per second  */
unsigned long  g_nr_waiting=0;
static struct task_struct *g_task_dead=0;  /* TODO : remove me later */

#define is_kernelThread(task) (task->mm == g_kernel_mm)

extern long *stack;
void init_timer();
static int free_mm(struct mm_struct *mm);
static unsigned long push_to_userland();

static struct task_struct *alloc_task_struct(void)
{
	return (struct task_struct *) mm_getFreePages(0,2); /*WARNING: do not change the size it is related TASK_SIZE, 4*4k=16k page size */
}

static void free_task_struct(struct task_struct *p)
{
	mm_putFreePages((unsigned long) p, 2);
	return ;
}
static inline void add_to_runqueue(struct task_struct * p) /* Add at the first */
{
	list_add_tail(&p->run_link,&run_queue.head);
	atomic_inc(&run_queue.count);
}

static inline struct task_struct *get_from_runqueue()
{
	struct task_struct * p;
	struct list_head *node;

	if (run_queue.head.next == &run_queue.head)
	{
		return 0;
	}
	node=run_queue.head.next;
	p=list_entry(node,struct task_struct, run_link);
	if (p==0) BUG();
	return p;
}
static inline struct task_struct *del_from_runqueue(struct task_struct * p)
{
	if (p==0)
	{
		struct list_head *node;

		node=run_queue.head.next;
		if (run_queue.head.next == &run_queue.head)
		{
			return 0;
		}

		p=list_entry(node,struct task_struct, run_link);
		if (p==0) BUG();
	}
	list_del(&p->run_link);
	atomic_dec(&run_queue.count);
	return p;
}

static inline void move_last_runqueue(struct task_struct * p)
{
	/* remove from list */
	del_from_runqueue(p);

	/* add back to list */
	add_to_runqueue(p);
}


/******************************************  WAIT QUEUE *******************/
void inline init_waitqueue(struct wait_struct *waitqueue)
{
	waitqueue->queue=NULL;
	waitqueue->lock=SPIN_LOCK_UNLOCKED;
	return;
}
static inline void add_to_waitqueue(struct wait_struct *waitqueue,struct task_struct * p,int dur) 
{  
	struct task_struct *tmp,*ptmp;
	int cum_dur,prev_cum_dur;
	unsigned long flags;

	spin_lock_irqsave(&waitqueue->lock, flags);

	cum_dur=0;
	prev_cum_dur=0;	
	if (waitqueue->queue == NULL) 
	{
		waitqueue->queue=p;
		p->sleep_ticks=dur;
		p->next_wait=NULL;
		p->prev_wait=NULL;
	}else
	{
		tmp=waitqueue->queue;
		while (tmp->next_wait != NULL)
		{
			if (dur < cum_dur) break;
			prev_cum_dur=cum_dur;
			cum_dur=cum_dur+tmp->sleep_ticks;
			tmp=tmp->next_wait;
		}
		p->next_wait=tmp;
		/* prev_cum_dur < dur < cum_dir  */
		if (tmp->prev_wait == NULL) /* Inserting at first node */
		{
			if (waitqueue->queue != tmp) BUG();
			waitqueue->queue=p;
			p->sleep_ticks=dur;
			tmp->prev_wait=p;
			p->prev_wait=NULL;
		}else
		{
			ptmp=tmp->prev_wait;
			ptmp->next_wait=p;
			p->prev_wait=ptmp;
			p->sleep_ticks=dur-prev_cum_dur;
			tmp->prev_wait=p;
		}
	}
	g_nr_waiting++;
	spin_unlock_irqrestore(&waitqueue->lock, flags);
}
static inline int del_from_waitqueue(struct wait_struct *waitqueue,struct task_struct * p)
{
	struct task_struct *next = p->next_wait;
	struct task_struct *prev = p->prev_wait;
	unsigned long flags;

	if (p->state != TASK_INTERRUPTIBLE) return 0;
	spin_lock_irqsave(&waitqueue->lock, flags);
	g_nr_waiting--; /* TODO : */
	if (next != NULL)
	{
		next->prev_wait = prev;
		next->sleep_ticks=next->sleep_ticks + p->sleep_ticks;
	}
	if (prev != NULL)
	{
		prev->next_wait = next;
	}

	if (waitqueue->queue == p)
	{
		waitqueue->queue=next;
	}
	if (waitqueue->queue == NULL)
	{
	}
	p->next_wait = NULL;
	p->prev_wait = NULL;
	p->sleep_ticks=0;

	spin_unlock_irqrestore(&waitqueue->lock, flags);
	return 1;
}

int sc_wakeUp(struct wait_struct *waitqueue,struct task_struct * p)
{
	int ret=0;

	unsigned long flags;
	if (p == NULL)
	{
		while (waitqueue->queue != NULL)
		{
			p=waitqueue->queue;
			spin_lock_irqsave(&sched_lock, flags);
			if (del_from_waitqueue(waitqueue,p) == 1)
			{
				p->state = TASK_RUNNING;
				if (p->run_link.next==0)
					add_to_runqueue(p);
				ret++;
			}
			spin_unlock_irqrestore(&sched_lock, flags);
		}	
	}else
	{
		spin_lock_irqsave(&sched_lock, flags);
		if (del_from_waitqueue(waitqueue,p) == 1)
		{
			p->state = TASK_RUNNING;
			if (p->run_link.next==0)
				add_to_runqueue(p);
			ret++;
		}
		spin_unlock_irqrestore(&sched_lock, flags);
	}
	return ret;
}


int sc_wait(struct wait_struct *waitqueue,int ticks)
{
	g_current_task->state=TASK_INTERRUPTIBLE;
	add_to_waitqueue(waitqueue,g_current_task,ticks);
	sc_schedule();
	if (g_current_task->sleep_ticks <= 0 ) return 0;
	else
	  return g_current_task->sleep_ticks;
}
int sc_sleep(int ticks) /* each tick is 100HZ or 10ms */
{
	g_current_task->state=TASK_INTERRUPTIBLE;
	add_to_waitqueue(&g_timerqueue,g_current_task,ticks);
	sc_schedule();
	return 1;
}
int sc_threadlist( char *arg1,char *arg2)
{
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;

	spin_lock_irqsave(&sched_lock, flags);
	ut_printf("pid task ticks mm pgd mm_count \n");
	list_for_each(pos, &task_queue.head) {
		task=list_entry(pos, struct task_struct, task_link);
		if (is_kernelThread(task))
			ut_printf("[%d]",task->pid);
		else
			ut_printf("%d",task->pid);
		
		ut_printf(" %x %x %x %x %d %s\n",task,task->ticks,task->mm,task->mm->pgd,task->mm->count.counter,task->name);
	}
	spin_unlock_irqrestore(&sched_lock, flags);
	return 1;
}

unsigned long setup_userstack(unsigned char **argv,unsigned char *env,unsigned long *stack_len,unsigned long *t_argc,unsigned long *t_argv,unsigned long *p_aux)
{
	int i,len,total_args=0;
	unsigned char *p,*stack;
	unsigned long real_stack,addr;
	unsigned char *target_argv[12];

	if (argv==0 && env==0)
	{
		ut_printf(" ERROR  argv:0\n");	
		 return 0;
	}
	stack=(unsigned char *)mm_getFreePages(MEM_CLEAR,0);
	p=stack+PAGE_SIZE;
	len=0;	
	real_stack=USERSTACK_ADDR+USERSTACK_LEN;
	for (i=0; argv[i]!=0&& i<10; i++)
	{
		total_args++;
		len=ut_strlen(argv[i]);	
		if ((p-len-1)>stack)
		{
			p=p-len-1;
			real_stack=real_stack-len-1;
			DEBUG(" argument :%d address:%x \n",i,real_stack);
			ut_strcpy(p,argv[i]);
			target_argv[i]=real_stack;
		}else
		{
			goto error;
		}
	}

	target_argv[i]=0;
	target_argv[i+1]=0;
	addr=p;
	addr=(addr/8)*8;
	p=addr;

	p=p-(MAX_AUX_VEC_ENTRIES*16);
	
	real_stack=USERSTACK_ADDR+USERSTACK_LEN+p-(stack+PAGE_SIZE);
	*p_aux=p;
	len=(total_args+1+1)*8; /* we are assuming emoty env */
	if ((p-len-1) > stack)
	{
		unsigned long *t;

		p=p-len;
		real_stack=real_stack-len;
		ut_memcpy(p,target_argv,len);
		DEBUG(" arg0:%x arg1:%x arg2:%x len:%d \n",target_argv[0],target_argv[1],target_argv[2],len);
		p=p-8;
		t=p;
		*t=total_args; /* store argc at the top of stack */
	}else
	{
		goto error;
	}
	*stack_len=PAGE_SIZE-(p-stack);
	*t_argc=total_args;
	*t_argv=real_stack-8;
	return stack;

error:
	mm_putFreePages(stack,0);
	return 0;	
}
unsigned long SYS_sc_execve(unsigned char *file,unsigned char **argv,unsigned char *env)
{
	struct file *fp;
	struct mm_struct *mm;
	unsigned long main_func;
	struct user_regs *p;
	unsigned long *tmp;
	unsigned long t_argc,t_argv,stack_len,tmp_stack,tmp_aux;

	SYSCALL_DEBUG("execve file:%s argv:%x env:%x \n",file,argv,env);
	/* create the argc and env in a temporray stack before we destory the old stack */
	t_argc=0;
	t_argv=0;
	tmp_stack=setup_userstack(argv,env,&stack_len,&t_argc,&t_argv,&tmp_aux);
	if (tmp_stack ==0) 
	{
		return 0;
	}
	/* delete old vm and create a new one */
	free_mm(g_current_task->mm);
	mm=kmem_cache_alloc(mm_cachep, 0);
	if (mm ==0 ) BUG();
	atomic_set(&mm->count,1);
	mm->pgd=0;
	mm->mmap=0;
	mm->fs.total=3;
	ar_pageTableCopy(g_kernel_mm,mm);	 /* every process page table should have soft links to kernel page table */
	g_current_task->mm=mm;
	flush_tlb(mm->pgd);

	/* populate vm with vmaps */
	fp=fs_open(file,0,0);
	if (fp == 0)
	{
		ut_printf("Error :execve Failed to open the file :%s\n",file);
		return 0;
	}
	main_func=fs_loadElfLibrary(fp,tmp_stack+(PAGE_SIZE-stack_len),stack_len,tmp_aux);
	if (main_func ==0) 
	{
		mm_putFreePages(tmp_stack,0);
		return 0;
	}
	mm_putFreePages(tmp_stack,0);
	ut_strncpy(g_current_task->name,file,MAX_TASK_NAME);
	vm_printMmaps(0,0);

	g_current_task->thread.userland.ip=main_func;
	g_current_task->thread.userland.sp=t_argv;
	g_current_task->thread.userland.argc=t_argc;
	g_current_task->thread.userland.argv=t_argv;

	g_current_task->thread.userland.user_stack = USERSTACK_ADDR+USERSTACK_LEN;
	g_current_task->thread.userland.user_ds=0;
	g_current_task->thread.userland.user_es=0;
	g_current_task->thread.userland.user_gs=0;
	g_current_task->thread.userland.user_fs=0;

	g_current_task->thread.userland.user_fs_base=0;
	ar_updateCpuState(g_current_task);
	push_to_userland();
}

struct user_regs {
	struct gpregs gpres;
	struct intr_stack_frame isf;
};
#define DEFAULT_RFLAGS_VALUE (0x10202)
#define GLIBC_PASSING 1
extern void enter_userspace();
static unsigned long push_to_userland()
{
	struct user_regs *p;
	DEBUG(" from PUSH113_TO_USERLAND \n");
	/* From here onwards DO NOT  call any function that consumes stack */
	asm("cli");
	asm("movq %%rsp,%0" : "=m" (p));
	p=p-1;
	asm("subq $0xa0,%rsp");
	p->gpres.rbp=0;
#ifndef GLIBC_PASSING
	p->gpres.rsi=g_current_task->thread.userland.sp; /* second argument to main i.e argv */
	p->gpres.rdi=g_current_task->thread.userland.argc; /* first argument argc */
#else
	p->gpres.rsi=0;
	p->gpres.rdi=0;
#endif
	p->gpres.rdx=0;
	p->gpres.rbx=p->gpres.rcx=0;
	p->gpres.rax=p->gpres.r10=0;
	p->gpres.r11=p->gpres.r12=0;
	p->gpres.r13=p->gpres.r14=0;
	p->gpres.r15=p->gpres.r9=0;
	p->gpres.r8=0;
	p->isf.rip=g_current_task->thread.userland.ip;
	p->isf.rflags=DEFAULT_RFLAGS_VALUE;
	p->isf.rsp=g_current_task->thread.userland.sp;
	p->isf.cs=GDT_SEL(UCODE_DESCR) | SEG_DPL_USER;
	p->isf.ss=GDT_SEL(UDATA_DESCR) | SEG_DPL_USER;

	g_cpu_state[0].user_fs=0;
	g_cpu_state[0].user_gs=0;
	g_cpu_state[0].user_fs_base=0;

	enter_userspace();	
}
static int free_mm(struct mm_struct *mm)
{
	int i,total_fds;

	DEBUG("freeing the mm :%x counter:%x \n",mm,mm->count.counter);

	if (mm->count.counter==0) BUG();
	atomic_dec(&mm->count);
	if (mm->count.counter > 0) return 0;

	vm_munmap(mm,0,0xffffffff);
	ar_pageTableCleanup(mm,0, 0xfffffffff);
	for (i=3; i<mm->fs.total;i++)
	{
		DEBUG("FREEing the files :%d \n",i);
		fs_close(mm->fs.filep[i]);		
	}
	mm->fs.total=0;
	kmem_cache_free(mm_cachep,mm);
	return 1;
}
#define CLONE_VM 1
unsigned long sc_createKernelThread(int (*fn)(void *),unsigned char  *args,unsigned char *thread_name)
{
	unsigned long pid;
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;

	pid = SYS_sc_clone(fn,0,CLONE_VM,args);

	if (thread_name == 0)return pid;
	spin_lock_irqsave(&sched_lock, flags);
	list_for_each(pos, &task_queue.head) {
		task=list_entry(pos, struct task_struct, task_link);
		if (task->pid ==pid)
		{
			ut_strncpy(task->name,thread_name,MAX_TASK_NAME);
			break;
		}
	}
	spin_unlock_irqrestore(&sched_lock, flags);

	return pid;
}
unsigned long SYS_sc_clone(int (*fn)(void *), void *child_stack,int clone_flags,void *args)
{
	struct task_struct *p;
	struct mm_struct *mm;
	unsigned long flags;


//	SYSCALL_DEBUG("clone fn:%x child_stack:%x flags:%x args:%x \n",fn,child_stack,clone_flags,args);
	/* Initialize the stack  */
	p = alloc_task_struct();
	if ( p == 0) BUG();
	ut_memset((unsigned char *)p,MAGIC_CHAR,TASK_SIZE);
	/* Initialize mm */
	if (clone_flags | CLONE_VM) /* parent and child run in the same vm */
	{
		mm=g_current_task->mm;
		atomic_inc(&mm->count);	
		DEBUG("clone  CLONE_VM the mm :%x counter:%x \n",mm,mm->count.counter);
	}else
	{
		mm=kmem_cache_alloc(mm_cachep, 0);
		if (mm ==0 ) BUG();
		atomic_set(&mm->count,1);
		DEBUG("clone  the mm :%x counter:%x \n",mm,mm->count.counter);
		mm->pgd=0;
		mm->fs.total=3;
		ar_pageTableCopy(g_current_task->mm,mm);	
		mm->mmap=0;
	}
	/* initialize task struct */
	p->mm=mm;
	p->ticks=0;
	p->pending_signal=0;
	if (g_current_task->mm != g_kernel_mm)
	{ /* user level thread */
		p->thread.userland.ip=fn;
		p->thread.userland.sp=child_stack;
		p->thread.userland.argc=0;/* TODO */
		p->thread.userland.argv=0; /* TODO */
		p->thread.ip=(void *)push_to_userland;
	}else
	{ /* kernel level thread */
		p->thread.ip=(void *)fn;
		p->thread.argv=args;
	}
	p->thread.sp=(addr_t)p+(addr_t)TASK_SIZE;
	p->state=TASK_RUNNING;
	ut_strncpy(p->name,g_current_task->name,MAX_TASK_NAME);

	/* link to queue */
	g_pid++;
	p->pid=g_pid;
	p->ppid=g_current_task->pid;
	p->run_link.next=0;
	p->run_link.prev=0;
	p->task_link.next=0;
	p->task_link.prev=0;
	p->next_wait=p->prev_wait=NULL;

	spin_lock_irqsave(&sched_lock, flags);
	list_add_tail(&p->task_link,&task_queue.head);
	add_to_runqueue(p);
	spin_unlock_irqrestore(&sched_lock, flags);
	return p->pid;
}
unsigned long  SYS_sc_fork()
{
	SYSCALL_DEBUG("fork \n");
	return SYS_sc_clone(0,0,CLONE_VM,0);
}
int SYS_sc_exit(int status)
{
	unsigned long flags;
	SYSCALL_DEBUG("sys exit : status:%d \n",status);

	spin_lock_irqsave(&sched_lock, flags);
	list_del(&g_current_task->task_link);
	g_current_task->state=TASK_DEAD;
	spin_unlock_irqrestore(&sched_lock, flags);

	sc_schedule();
	return 0;
}
int SYS_sc_kill(unsigned long pid,unsigned long signal)
{
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;

	SYSCALL_DEBUG("kill pid:%d signal:%d \n",pid,signal);
	spin_lock_irqsave(&sched_lock, flags);
	list_for_each(pos, &task_queue.head) {
		task=list_entry(pos, struct task_struct, task_link);
		if (task->pid ==pid) 
		{
			task->pending_signal=1;
			break;
		}
	}
	spin_unlock_irqrestore(&sched_lock, flags);
	return 1;
}
/******************* schedule related functions **************************/


static struct task_struct *__switch_to(struct task_struct *prev_p, struct task_struct *next_p)
{
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


void init_tasking()
{
	g_kernel_mm=kmem_cache_alloc(mm_cachep, 0);
	if (g_kernel_mm ==0) return ;

	INIT_LIST_HEAD(&(run_queue.head));
	INIT_LIST_HEAD(&(task_queue.head));
	run_queue.count.counter=0;
	task_queue.count.counter=0;

	atomic_set(&g_kernel_mm->count,1);
	g_kernel_mm->mmap=0x0;
	g_kernel_mm->pgd=(unsigned char *)g_kernel_page_dir;
	g_kernel_mm->fs.total=3;

	g_current_task =(struct task_struct *) &stack;
	g_idle_task =(struct task_struct *) &stack;
	g_idle_task->ticks=0;
	g_idle_task->magic_numbers[0]=g_idle_task->magic_numbers[1]=MAGIC_LONG;
	g_current_task->state=TASK_RUNNING;
	g_current_task->pid=g_pid;
	g_current_task->mm=g_kernel_mm;
	list_add_tail(&g_current_task->task_link,&task_queue.head);
	ut_strncpy(g_current_task->name,"idle",MAX_TASK_NAME);
	g_pid++;
	init_timer();
	init_waitqueue(&g_timerqueue);
}
static unsigned long intr_flags;
void sc_schedule()
{
	struct task_struct *prev, *next;

	if (!g_current_task)
	{
		BUG();
		return;
	}

	if (g_current_task->magic_numbers[0]!=MAGIC_LONG || g_current_task->magic_numbers[1]!=MAGIC_LONG) /* safety check */
	{
		DEBUG(" Task Stack got CORRUPTED task:%x :%x :%x \n",g_current_task,g_current_task->magic_numbers[0],g_current_task->magic_numbers[1]);
		BUG();
	}

	g_current_task->ticks++;
	spin_lock_irqsave(&sched_lock, intr_flags);
	prev=g_current_task;

/* Handle any pending signal */
	if (prev->pending_signal != 0)
	{
		if (is_kernelThread(prev))
		{
			ut_printf(" WARNING: kernel thread cannot be killed \n");
		}else
		{
	        	list_del(&prev->task_link);
       			prev->state=TASK_DEAD;
		}
		prev->pending_signal=0;
	}
/* remove from runqueue based on state and get  the next task */
	if (prev!= g_idle_task) 
	{
		move_last_runqueue(prev);
		switch (prev->state)
		{
			case TASK_INTERRUPTIBLE:

			default:
				del_from_runqueue(prev);
			case TASK_RUNNING: break;
		}
	}
	next=get_from_runqueue(0);
	if (next == 0) next=g_idle_task;
	g_current_task = next;

/* handle the  dead task */
	if (g_task_dead) 
	{
		free_mm((struct task_struct *)g_task_dead->mm);
		free_task_struct(g_task_dead);
		g_task_dead=0;
	}
	if (prev->state == TASK_DEAD )
	{
		g_task_dead=prev;
	}

/* if prev and next are same then return */
	if (prev==next)
	{
		spin_unlock_irqrestore(&sched_lock, intr_flags);
		return;
	}
/* if  prev and next are having same address space , then avoid tlb flush */
	next->counter=5; /* 50 ms time slice */
	if (prev->mm->pgd != next->mm->pgd) /* TODO : need to make generic */
	{
		flush_tlb(next->mm->pgd);
	}	
/* update the cpu state  and tss state for system calls */
	ar_updateCpuState(next);
	ar_setupTssStack((unsigned long)next+TASK_SIZE);
/* finally switch the task */
	switch_to(prev,next,prev);
	spin_unlock_irqrestore(&sched_lock, intr_flags);
}

void do_softirq()
{
//	asm volatile("sti");
	if (g_current_task->counter <= 0)
	{
		sc_schedule();
	}
}


#define MAX_WAIT_QUEUES 50
static struct wait_struct *wait_queues[MAX_WAIT_QUEUES]; /* TODO : this need to be locked */
int stat_wq_count=0;
int sc_register_waitqueue(struct wait_struct *waitqueue)
{
	int i;

	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queues[i] == 0) {
			waitqueue->queue=NULL;
			waitqueue->lock=SPIN_LOCK_UNLOCKED;
			wait_queues[i]=waitqueue;
			stat_wq_count++;
			return 0;
		}
	}
	return -1;
}
int sc_unregister_waitqueue(struct wait_struct *waitqueue)
{
	int i;

	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queues[i] == waitqueue) {
			wait_queues[i]=0;
			stat_wq_count--;
			return 0;
		}
	}
	return -1;
}
static void timer_callback(registers_t regs)
{
	int i;

	g_jiffies++;
	g_current_task->counter--;
	if (g_timerqueue.queue != NULL)
	{
		g_timerqueue.queue->sleep_ticks--;
		if (g_timerqueue.queue->sleep_ticks <= 0)
		{
			struct task_struct *p;

			p=g_timerqueue.queue;
			del_from_waitqueue(&g_timerqueue,p);			
			p->state = TASK_RUNNING;
			if (p->run_link.next==0)
				add_to_runqueue(p);
		}
	}
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queues[i] == 0) continue;
		if (wait_queues[i]->queue != NULL) {
			wait_queues[i]->queue->sleep_ticks--;
			if (wait_queues[i]->queue->sleep_ticks <= 0) {
				struct task_struct *p;

				p = wait_queues[i]->queue;
				del_from_waitqueue(wait_queues[i], p);
				p->state = TASK_RUNNING;
				if (p->run_link.next == 0)
					add_to_runqueue(p);
				else
					ut_printf(" BUG identified \n");
			}
		}
	}
	do_softirq();
}

void init_timer()
{
	addr_t frequency=100;
	// Firstly, register our timer callback.
	ar_registerInterrupt(32, &timer_callback,"timer_callback");

	// The value we send to the PIT is the value to divide it's input clock
	// (1193180 Hz) by, to get our required frequency. Important to note is
	// that the divisor must be small enough to fit into 16-bits.
	addr_t divisor = 1193180 / frequency;

	// Send the command byte.
	outb(0x43, 0x36);

	// Divisor has to be sent byte-wise, so split here into upper/lower bytes.
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

	// Send the frequency divisor.
	outb(0x40, l);
	outb(0x40, h);
}

