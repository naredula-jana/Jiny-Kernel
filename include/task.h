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
#include "ipc.h"
#include "descriptor_tables.h"

#define TASK_SIZE 4*(PAGE_SIZE)  /* TODO : it is redefined in multiboot.h  also */

#define TASK_RUNNING            0
#define TASK_INTERRUPTIBLE      1
#define TASK_UNINTERRUPTIBLE    2
#define TASK_STOPPED            4
#define TASK_KILLING            8
#define TASK_DEAD               64

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
	int(*real_ip)(void *);
	unsigned char **argv;
	struct user_thread userland;
	struct user_regs user_regs;
};
#define MAX_FDS 100
//#define MAX_FILENAME 200
struct file;
struct fs_struct {
	struct file *filep[MAX_FDS];
	int total;
	unsigned char cwd[200]; // change to MAX_FILENAME
	unsigned int input_device,output_device; /* for user level thread serial line will be input/output*/
};

struct mm_struct {
	struct vm_area_struct *mmap; /* list of VMAs */
	unsigned long pgd;
	atomic_t count; /* How many references to "struct mm_struct" (users count as 1) */
	struct fs_struct fs;
	struct file *exec_fp; /* execute file */
	unsigned long brk_addr, brk_len;
	unsigned long anonymous_addr;
};

// This structure defines a 'task' - a process.
#define MAX_TASK_NAME 40

#define MAGIC_CHAR 0xab
#define MAGIC_LONG 0xabababababababab

typedef struct task_queue {
	struct list_head head;
	char *name;
} task_queue_t;
/*
 - task can be on run queue or in wait queues */
struct task_struct {
	volatile long state;    /* -1 unrunnable, 0 runnable, >0 stopped */
	unsigned long flags;    /* per process flags, defined below */
	unsigned long pending_signals;	
	int killed; /* some as send send a kill signal or self killed */
	atomic_t count; /* usage count */

	unsigned long pid,ppid;
	unsigned char name[MAX_TASK_NAME+1];

	int allocated_cpu;
	int current_cpu;
	int stick_to_cpu; /* by default run on any cpu */

	int counter;
	long sleep_ticks;
	unsigned long cpu_contexts; /* every cpu context , it will be incremented */
	unsigned long ticks;	
	struct thread_struct thread;
	struct mm_struct *mm;
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
		unsigned long ticks_consumed;
		unsigned long mem_consumed;
		unsigned wait_start_tick_no;
		int wait_line_no; /* line number where mutex lock as triggered */
		int syscall_count;
	} stats;

	unsigned long magic_numbers[4]; /* already stack is default fill with magic numbers */
}; 
struct cpu_state {
	struct md_cpu_state md_state; /* This should be at the first location */

	struct task_struct *current_task;
	struct task_struct *idle_task;
	task_queue_t run_queue;
	int run_queue_length;

	unsigned char cpu_priority;
	int active; /* only active cpu will pickup the tasks , otherwise they can only run idle threads */
	int intr_disabled; /* interrupts disabled except apic timer */

	int intr_nested_level;

	unsigned long stat_total_contexts;
	unsigned long stat_nonidle_contexts;
} __attribute__ ((aligned (64))) ;


struct cpu_state g_cpu_state[];
#define is_kernelThread(task) (task->mm == g_kernel_mm)
#define IPI_INTERRUPT 200
#define IPI_CLEARPAGETABLE 201
extern int getcpuid();

static inline struct task_struct *current_task(void)
{
	unsigned long addr,p;
	addr = (unsigned long)&p;
	addr=addr & (~(TASK_SIZE-1));

    return (struct task_struct *)addr;
}
#define g_current_task current_task()
#define is_kernel_thread (g_current_task->mm == g_kernel_mm)

typedef struct backtrace{
	struct{
	unsigned long ret_addr;
	unsigned char *name;
	}entries[MAX_BACKTRACE_LENGTH];
	int count;
}backtrace_t;

#endif
