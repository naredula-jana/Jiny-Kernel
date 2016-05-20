/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   include/task.h
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#ifndef TASK_H
#define TASK_H

#include "mm.h"
#define SPINLOCK_DEBUG_LOG 1 /* TODO redefined */

#define SPINLOCK_DEBUG 1
#define RECURSIVE_SPINLOCK 1
typedef struct {  /* Do not change the order , initilization will break */
        volatile unsigned int lock;
        unsigned long stat_count;
#ifdef SPINLOCK_DEBUG
#define MAX_SPIN_LOG 100
        unsigned char *name;
        unsigned long stat_locks;
        unsigned long stat_unlocks;
        unsigned long stat_recursive_locks;
        unsigned long recursive_count;
        int recursion_allowed;
        unsigned long pid;
        unsigned long contention;
        unsigned int log_length;
        unsigned long stat_cpu_used[MAX_CPUS];
#ifdef SPINLOCK_DEBUG_LOG
        struct {
            int line;
            unsigned int task_id;
            unsigned int cpuid;
            unsigned long spins;
            unsigned char *name;
            unsigned long index;
        }log[MAX_SPIN_LOG];
#endif /* end of SPINLOCK_DEBUG_LOG */
#endif
} spinlock_t __attribute__ ((aligned (64)));



#include "descriptor_tables.h"

#define TASK_SIZE 4*(PAGE_SIZE)  /* TODO : it is redefined in multiboot.h  also */

enum{
 TASK_RUNNING = 0,
 TASK_INTERRUPTIBLE    =  0x1,
 /*  TASK_UNINTERRUPTIBLE  =  0x2,   NOT used */
 TASK_STOPPED          =  0x4,
 TASK_KILLING          =  0x8,
 TASK_DEAD             =  0x10,
 TASK_NONPREEMPTIVE    =  0x20
};

struct user_regs {
	struct gpregs gpres;
	struct intr_stack_frame isf;
};

struct user_thread {
	unsigned long ip,sp;
	unsigned long argc,argv;

	unsigned long user_stack,user_ds,user_es,user_gs;
	unsigned long user_fs,user_fs_base;
};
struct thread_struct {
	void *sp; /* kernel stack pointer when scheduling start */
	void *ip; /* kernel ip when scheduling start */
	void *rbp; /* kernel rbp, currently used for getting back trace */
	int(*real_ip)(void *, void *);
	void **argv;
	struct user_thread userland;
	struct user_regs user_regs;
};
#define MAX_FDS 100
//#define MAX_FILENAME 200
struct file;
struct fs_struct {
	struct file *filep[MAX_FDS];
	atomic_t count;
	int total;
	unsigned char cwd[200]; // change to MAX_FILENAME
};

struct mm_struct {
	struct vm_area_struct *mmap; /* list of VMAs */
	unsigned long pgd;
	atomic_t count; /* How many references to "struct mm_struct" (users count as 1) */

	struct file *exec_fp; /* execute file */

	unsigned long start_code,end_code ; /* used onlt for stats */
	unsigned long brk_addr, brk_len;
	unsigned long anonymous_addr;
	unsigned long stack_bottom; /* currenty user for stat pupose */
	unsigned long stat_page_allocs, stat_page_free;
};

// This structure defines a 'task' - a process.
#define MAX_TASK_NAME 40

#define MAGIC_CHAR 0xab
#define MAGIC_LONG 0xabababababababab

typedef struct task_queue {
	struct list_head head;
	char *name;
	int length;
} task_queue_t;

enum {
	THREAD_LOW_PRIORITY=0,
	THREAD_HIGH_PRIORITY=1
};
/* standard parameters compatabile with linux */
#define CLONE_VM             0x100
#define CLONE_FS             0x200
#define CLONE_VFORK         0x4000
#define CLONE_THREAD    0x00010000      /* Same thread group? */
#define CLONE_NEWNS     0x00020000      /* New mount namespace group */
#define CLONE_SYSVSEM   0x00040000      /* share system V SEM_UNDO semantics */
#define CLONE_SETTLS    0x00080000      /* create a new TLS for the child */

#define CLONE_HP_THREAD 0x40000000 /* proparatory */
#define CLONE_KERNEL_THREAD 0x80000000 /* proparatory */
/* - task can be on run queue or in wait queues */
#define MAX_SYSCALL 255
struct syscall_stat{
	int count;
};
struct task_struct {
	unsigned long unused[4];
	volatile long state;    /* -1 unrunnable, 0 runnable, >0 stopped */
	unsigned long flags;    /* per process flags, defined below */
	unsigned long pending_signals;	
	int killed; /* some as send send a kill signal or self killed */
	atomic_t count; /* usage count */
	int clone_flags; /* clone flags of the parent task, this is used for vfork call */

	unsigned long task_id;  /* unique id for each task */
	unsigned long parent_process_pid;  /* parent process id */
	unsigned long process_id;  /* same  task id for all with the same process*/
	unsigned char name[MAX_TASK_NAME+1];
	char HP_thread;
	unsigned long child_tid_address; /* used set_tid_address */

	int allocated_cpu;
	int current_cpu;
	int stick_to_cpu; /* by default run on any cpu */

	int counter;  /* ticks for every 1 context switch*/
	long sleep_ticks;
	unsigned long last_jiffie;

	struct thread_struct thread;
	struct mm_struct *mm;
	struct fs_struct *fs;

	int locks_sleepable;
	int locks_nonsleepable;
	int wait_for_child_exit;

#define MAX_DEBUG_CALLSTACK 10
	int trace_stack_length; /* used for trace purpose */
	char trace_on; /* used for trace */
	void *callstack[MAX_DEBUG_CALLSTACK];  /* current function when collecting call graph */
	int callstack_top;

	task_queue_t dead_tasks;
	int exit_code;
	int curr_syscall_id;
#define MAX_TASK_STATUS_DATA 100
	unsigned char status_info[MAX_TASK_STATUS_DATA];

	struct list_head run_queue; /* run queue */
	struct list_head task_queue; /* task queue */
	struct list_head wait_queue; /* wait queue */

	struct {
		unsigned long total_contexts;
		unsigned long mem_consumed;
		unsigned wait_start_tick_no;
		int wait_line_no; /* line number where mutex lock as triggered */
		int syscall_count;
		int fault_count;
		unsigned long ticks_consumed;
		unsigned long start_time;
		unsigned long start_tsc;
#if 1
		struct syscall_stat *syscalls;
#endif
	} stats;

	unsigned long magic_numbers[4]; /* already stack is default fill with magic numbers */
}; 
struct cpu_state {
	struct md_cpu_state md_state; /* This should be at the first location */

	struct task_struct *current_task;
	struct task_struct *idle_task;
	spinlock_t lock; /* currently this a) protect run queue, since it is updated globally b) before schedule this is taken to disable interrupts */
	task_queue_t run_queue;
	volatile int run_queue_length;
	int net_BH; /* enable if the net_RX BH need to be kept on the cpu on a priority basis */
	struct {
		int inprogress;
		int pmd_active;  /* poll mode driver active */
		unsigned long pkts_processed;
		unsigned long ticks;
	}net_bh;
	spinlock_t *sched_lock; /* lock to relase after schedule, it is filled before the schedule */
	unsigned long system_times_ns;

	unsigned char cpu_priority;
	int active; /* only active cpu will pickup the tasks , otherwise they can only run idle threads */
	int intr_disabled; /* interrupts disabled except apic timer */
	volatile int idle_state; /* if the cpu is in idle state set it to 1 */
	int task_on_wait; /* set if a task if this cpu is on wait and can wakeup any time */

	int intr_nested_level;
	unsigned long last_total_contexts; /* used for house keeping */
	struct{
		int clock_interrupts;
		int nonclock_interrupts;
		unsigned long hits;
	}cpu_spinstate;

	struct {
		unsigned long total_contexts;
		unsigned long nonidle_contexts;
		unsigned long idleticks; /* in idle function when the timer arrives */
		unsigned long syscalls;
		unsigned long netbh_recv;
		unsigned long netbh_send;
		unsigned long rip;
		unsigned long netbh;
	} stats;
	void *isr_ctxts[10]; /* TODO: remove later, only for testing */
} __attribute__ ((aligned (64))) ;


extern struct cpu_state g_cpu_state[];
#define is_kernelThread(task) (task->mm == g_kernel_mm)
#define IPI_INTERRUPT 200
#define IPI_CLEARPAGETABLE 201

static inline struct task_struct *bootup_task(void)
{
	unsigned long addr,p;
	addr = (unsigned long)&p;
	addr=addr & (~(TASK_SIZE-1));

    return (struct task_struct *)addr;
}


static inline struct task_struct *getcurrenttask()  __attribute__((always_inline));
static inline struct task_struct *getcurrenttask() {
	//struct task_struct *task;
	//asm volatile("movq %%gs:0x58,%0" : "=r" (task)); // TODO : Hardcoded 58 need to replace with define symbol
	return g_cpu_state[0].current_task;
}

register unsigned long current_stack_pointer asm("esp");
#define boot_g_current_task ((struct task_struct *)(current_stack_pointer & ~(TASK_SIZE - 1)))
//#define g_current_task ((struct task_struct *)(current_stack_pointer & ~(TASK_SIZE - 1)))
#define g_current_task (g_cpu_state[0].current_task)  /* TODO : this is temporary , later need to change */
//#define g_current_task (getcurrenttask())
//#define g_current_task ((g_boot_completed == 0)? (g_cpu_state[0].current_task):(getcurrenttask()))

#define is_kernel_thread (g_current_task->mm == g_kernel_mm)
typedef struct backtrace{
	struct{
	unsigned long ret_addr;
	unsigned char *name;
	}entries[MAX_BACKTRACE_LENGTH];
	int count;
}backtrace_t;

#endif
