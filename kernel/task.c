#include "task.h"

struct task_struct *g_current_task,*g_init_task;
struct task_struct *g_wait_queue=NULL;
struct mm_struct *g_kernel_mm=0;
spinlock_t g_runqueue_lock  = SPIN_LOCK_UNLOCKED;
spinlock_t g_waitqueue_lock  = SPIN_LOCK_UNLOCKED;
int g_pid=0;
addr_t g_jiffies = 0; /* increments for every 10ms =100HZ = 100 cycles per second  */
addr_t g_nr_running;
addr_t g_nr_waiting=0;
addr_t g_tasks[200];
static int timer_counter=0;

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

static inline void add_to_waitqueue(struct task_struct * p,int dur) 
{  
	struct task_struct *tmp,*ptmp;
	int cum_dur,prev_cum_dur;
	unsigned long flags;

	spin_lock_irqsave(&g_waitqueue_lock, flags);

	cum_dur=0;
	prev_cum_dur=0;	
	if (g_wait_queue == NULL) 
	{
		g_wait_queue=p;
		p->sleep_ticks=dur;
		p->next_wait=NULL;
		p->prev_wait=NULL;
	}else
	{
		tmp=g_wait_queue;
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
			if (g_wait_queue != tmp) BUG();
			g_wait_queue=p;
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
	spin_unlock_irqrestore(&g_waitqueue_lock, flags);
}
static inline void del_from_waitqueue(struct task_struct * p)
{
	struct task_struct *next = p->next_wait;
	struct task_struct *prev = p->prev_wait;
	unsigned long flags;

	spin_lock_irqsave(&g_waitqueue_lock, flags);
	g_nr_waiting--;
	if (next != NULL)
	{
		next->prev_wait = prev;
		next->sleep_ticks=next->sleep_ticks + p->sleep_ticks;
	}
	if (prev != NULL)
	{
		prev->next_wait = next;
	}

	if (g_wait_queue == p)
	{
		g_wait_queue=next;
	}
	if (g_wait_queue == NULL)
	{
		timer_counter=0;
	}
	p->next_wait = NULL;
	p->prev_wait = NULL;
	p->sleep_ticks=0;

	spin_unlock_irqrestore(&g_waitqueue_lock, flags);
}
void sc_wakeUpProcess(struct task_struct * p)
{
	unsigned long flags;

	spin_lock_irqsave(&g_runqueue_lock, flags);
	del_from_waitqueue(p);
	p->state = TASK_RUNNING;
	if (!p->next_run)
		add_to_runqueue(p);
	spin_unlock_irqrestore(&g_runqueue_lock, flags);
}

int sc_sleep(int millisec)
{
	g_current_task->state=TASK_INTERRUPTIBLE;
	add_to_waitqueue(g_current_task,millisec);
	sc_schedule();
}

int sc_fork(unsigned long clone_flags, unsigned long usp, int (*fn)(void *))
{
	int nr;
	struct task_struct *p;
	unsigned long flags;

	p = alloc_task_struct();
	ut_memset(p,0xab,STACK_SIZE);
	p->thread.ip=fn;
	p->thread.sp=(addr_t)p+(addr_t)STACK_SIZE;
	p->pid=g_pid;
	p->state=TASK_RUNNING;
	p->mm=g_kernel_mm;
	atomic_inc(&g_kernel_mm->mm_count);
	if (g_pid <200) g_tasks[g_pid]=p; /* TODO : remove this later */
	g_pid++;

	p->next_wait=p->prev_wait=NULL;

	spin_lock_irqsave(&g_runqueue_lock, flags);
	add_to_runqueue(p);
	spin_unlock_irqrestore(&g_runqueue_lock, flags);
}

int sc_exit()
{
	g_current_task->state=TASK_DEAD;
	sc_schedule();
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
	g_kernel_mm->pgd=g_kernel_page_dir;

	g_current_task = &stack;
	g_init_task = &stack;
	g_current_task->next_run=g_init_task;
	g_current_task->prev_run=g_init_task;
	g_current_task->state=TASK_RUNNING;
	g_current_task->pid=g_pid;
	g_current_task->mm=g_kernel_mm;
	g_pid++;
	for (i=0; i<200;i++)
		g_tasks[0]=0;
	init_timer();
}

extern int keyboard_int;
extern void keybh();
//asmlinkage void schedule(void)
void sc_schedule()
{
	struct task_struct *prev, *next;
	unsigned long flags;

	if (!g_current_task)
		return;
	if (keyboard_int ==1) /* TODO remove when bottom half comes in */
	{
		//	key_bh();
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
	next->counter=10;
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
static void timer_callback(registers_t regs)
{
	g_jiffies++;
	g_current_task->counter--;
	if (g_wait_queue != NULL)
	{
		timer_counter++;
		if (timer_counter > g_wait_queue->sleep_ticks)
		{
			struct task_struct *p;

			p=g_wait_queue;
			timer_counter=0;
			del_from_waitqueue(g_wait_queue);			
		        p->state = TASK_RUNNING;
        		if (!p->next_run)
                		add_to_runqueue(p);
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

