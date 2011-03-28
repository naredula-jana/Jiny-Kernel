/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/task.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "task.h"
#include "interface.h"
#define MAGIC_CHAR 0xab
#define MAGIC_LONG 0xabababababababab

struct task_struct *g_current_task,*g_init_task;
struct mm_struct *g_kernel_mm=0;
spinlock_t g_runqueue_lock  = SPIN_LOCK_UNLOCKED;
struct wait_struct g_timerqueue;
int g_pid=0;
addr_t g_jiffies = 0; /* increments for every 10ms =100HZ = 100 cycles per second  */
addr_t g_nr_running = 0;
addr_t g_nr_waiting=0;
static addr_t g_task_dead=0;  /* TODO : remove me later */

extern long *stack;

void init_timer();
static struct task_struct *alloc_task_struct(void)
{
	return (struct task_struct *) mm_getFreePages(0,2); /* 4*4k=16k page size */
}

static void free_task_struct(struct task_struct *p)
{
	mm_putFreePages((unsigned long) p, 2);
	return ;
}
static inline void add_to_runqueue(struct task_struct * p) /* Add at the first */
{
	struct task_struct *next = g_init_task->next_run;

	p->prev_run = g_init_task;
	g_init_task->next_run = p;
	p->next_run = next;
	next->prev_run = p;
	g_nr_running++;
}

static inline void del_from_runqueue(struct task_struct * p)
{
	struct task_struct *next = p->next_run;
	struct task_struct *prev = p->prev_run;

	g_nr_running--;
	next->prev_run = prev;
	prev->next_run = next;
	p->next_run = NULL;
	p->prev_run = NULL;
}

static inline void move_last_runqueue(struct task_struct * p)
{
	struct task_struct *next = p->next_run;
	struct task_struct *prev = p->prev_run;

	/* remove from list */
	next->prev_run = prev;
	prev->next_run = next;
	/* add back to list */
	p->next_run = g_init_task;
	prev = g_init_task->prev_run;
	g_init_task->prev_run = p;
	p->prev_run = prev;
	prev->next_run = p;
}
static inline void move_first_runqueue(struct task_struct * p)
{
	struct task_struct *next = p->next_run;
	struct task_struct *prev = p->prev_run;

	/* remove from list */
	next->prev_run = prev;
	prev->next_run = next;
	/* add back to list */
	p->prev_run = g_init_task;
	next = g_init_task->next_run;
	g_init_task->next_run = p;
	p->next_run = next;
	next->prev_run = p;
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
		if (prev <0x100)   /* TODO : remove me later , for debugging purpose only */
		{
			DEBUG(" prev:%x p:%x waitqueue:%x \n",prev,p,waitqueue);
			BUG();
		}
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
			spin_lock_irqsave(&g_runqueue_lock, flags);
			if (del_from_waitqueue(waitqueue,p) == 1)
			{
				p->state = TASK_RUNNING;
				if (!p->next_run)
					add_to_runqueue(p);
				ret++;
			}
			spin_unlock_irqrestore(&g_runqueue_lock, flags);
		}	
	}else
	{
		spin_lock_irqsave(&g_runqueue_lock, flags);
		if (del_from_waitqueue(waitqueue,p) == 1)
		{
			p->state = TASK_RUNNING;
			if (!p->next_run)
				add_to_runqueue(p);
			ret++;
		}
		spin_unlock_irqrestore(&g_runqueue_lock, flags);
	}
	return ret;
}


int sc_wait(struct wait_struct *waitqueue,int ticks)
{
	g_current_task->state=TASK_INTERRUPTIBLE;
	add_to_waitqueue(waitqueue,g_current_task,ticks);
	sc_schedule();
	return 1;
}
int sc_sleep(int ticks) /* each tick is 100HZ or 10ms */
{
	g_current_task->state=TASK_INTERRUPTIBLE;
	add_to_waitqueue(&g_timerqueue,g_current_task,ticks);
	sc_schedule();
	return 1;
}

int sc_fork(unsigned long clone_flags, unsigned long usp, int (*fn)(void *))
{
	int nr;
	struct task_struct *p;
	unsigned long flags;

	p = alloc_task_struct();
	ut_memset(p,MAGIC_CHAR,STACK_SIZE);
	p->thread.ip=(void *)fn;
	p->thread.sp=(addr_t)p+(addr_t)STACK_SIZE;
	p->pid=g_pid;
	p->state=TASK_RUNNING;
	p->mm=g_kernel_mm;
	atomic_inc(&g_kernel_mm->mm_count);
	g_pid++;

	p->next_wait=p->prev_wait=NULL;

	spin_lock_irqsave(&g_runqueue_lock, flags);
	add_to_runqueue(p);
	spin_unlock_irqrestore(&g_runqueue_lock, flags);

	return 1;
}

int sc_exit()
{
	g_current_task->state=TASK_DEAD;
	sc_schedule();
	return 0;
}

int sc_createThread(int (*fn)(void *))
{
	return sc_fork(0, 0, fn);
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
	int i;
	g_kernel_mm=kmem_cache_alloc(mm_cachep, 0);
	if (g_kernel_mm ==0) return ;
	atomic_set(&g_kernel_mm->mm_count,1);
	g_kernel_mm->start_brk=0xc0000000;
	g_kernel_mm->mmap=0x0;
	g_kernel_mm->pgd=(unsigned char *)g_kernel_page_dir;

	g_current_task =(struct task_struct *) &stack;
	g_init_task =(struct task_struct *) &stack;
	g_init_task->magic_numbers[0]=g_init_task->magic_numbers[1]=MAGIC_LONG;
	g_current_task->next_run=g_init_task;
	g_current_task->prev_run=g_init_task;
	g_current_task->state=TASK_RUNNING;
	g_current_task->pid=g_pid;
	g_current_task->mm=g_kernel_mm;
	g_pid++;
	init_timer();
	init_waitqueue(&g_timerqueue);
}

//asmlinkage void schedule(void)
void sc_schedule()
{
	struct task_struct *prev, *next;
	unsigned long flags;

	if (!g_current_task)
		return;

	if (g_current_task->magic_numbers[0]!=MAGIC_LONG || g_current_task->magic_numbers[1]!=MAGIC_LONG) /* safety check */
	{
		DEBUG(" Task Stack got CORRUPTED task:%x :%x :%x \n",g_current_task,g_current_task->magic_numbers[0],g_current_task->magic_numbers[1]);
		BUG();
	}
	spin_lock_irqsave(&g_runqueue_lock, flags);
	prev=g_current_task;
	if (prev!= g_init_task) 
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
	next=g_init_task->next_run;
	g_current_task = next;
	if (g_task_dead) 
	{
		free_task_struct(g_task_dead);
		g_task_dead=0;
	}
	spin_unlock_irqrestore(&g_runqueue_lock, flags);

	if (prev->state == TASK_DEAD)
	{
		g_task_dead=prev;
	}

	if (prev==next) return;
	next->counter=5; /* 50 ms time slice */
	switch_to(prev,next,prev);
}
void do_softirq()
{
	asm volatile("sti");
	if (g_current_task->counter <= 0)
	{
		sc_schedule();
	}
}
extern struct wait_struct g_hfs_waitqueue;
static void timer_callback(registers_t regs)
{
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
			if (!p->next_run)
				add_to_runqueue(p);
		}
	}
	if (g_hfs_waitqueue.queue != NULL)
	{
		g_hfs_waitqueue.queue->sleep_ticks--;
                if (g_hfs_waitqueue.queue->sleep_ticks <= 0)
                {
                        struct task_struct *p;

                        p=g_hfs_waitqueue.queue;
                        del_from_waitqueue(&g_hfs_waitqueue,p);
                        p->state = TASK_RUNNING;
                        if (!p->next_run)
                                add_to_runqueue(p);
			else
			   ut_printf(" BUG identified \n");
                }
	}
	do_softirq();
}

void init_timer()
{
	addr_t frequency=100;
	// Firstly, register our timer callback.
	ar_registerInterrupt(32, &timer_callback);

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

