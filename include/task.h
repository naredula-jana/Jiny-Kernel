#ifndef TASK_H
#define TASK_H

#include "common.h"
#include "mm.h"

#define TASK_RUNNING            0
#define TASK_INTERRUPTIBLE      1
#define TASK_UNINTERRUPTIBLE    2
#define TASK_STOPPED            4
#define TASK_DEAD               64

struct wait_struct {
	struct task_struct *queue;	
	spinlock_t lock;
};

struct thread_struct {
	void *sp;
	void *ip;
};

struct mm_struct {
        struct vm_area_struct *mmap;           /* list of VMAs */
        unsigned long pgd; 
        atomic_t mm_users;                      /* How many users with user space? */
        atomic_t count;                      /* How many references to "struct mm_struct" (users count as 1) */
        int map_count;                          /* number of VMAs */
        spinlock_t page_table_lock;             /* Protects task page tables and mm->rss */

        unsigned long start_code, end_code, start_data, end_data;
        unsigned long start_brk, brk, start_stack;
        unsigned long arg_start, arg_end, env_start, env_end;
        unsigned long rss, total_vm, locked_vm;
        unsigned long def_flags;
        unsigned long cpu_vm_mask;
        unsigned long swap_address;
};
// This structure defines a 'task' - a process.
/*
 - task can be on run queue or in wait queues */
struct task_struct {
	volatile long state;    /* -1 unrunnable, 0 runnable, >0 stopped */
	unsigned long flags;    /* per process flags, defined below */
	int pid;
	int counter;
	int sleep_ticks;
	unsigned long ticks;	
	struct thread_struct thread;
	struct mm_struct *mm;
	struct list_head run_link; /* run queue */
	struct list_head task_link; /* task queue */
	struct task_struct *next_wait,  *prev_wait;  /* wait queue */
	unsigned long magic_numbers[4]; /* already stack is default fill with magic numbers */
}; 

extern struct task_struct *g_current_task;

#endif
