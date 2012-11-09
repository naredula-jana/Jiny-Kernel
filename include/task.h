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

//#include "common.h"
#include "mm.h"
#include "ipc.h"


#define TASK_RUNNING            0
#define TASK_INTERRUPTIBLE      1
#define TASK_UNINTERRUPTIBLE    2
#define TASK_STOPPED            4
#define TASK_DEAD               64


struct user_thread {
	unsigned long ip,sp;
	unsigned long argc,argv;

	unsigned long user_stack,user_ds,user_es,user_gs;
	unsigned long user_fs,user_fs_base;
};
struct thread_struct {
	void *sp; /* kernel stack pointer when scheduling start */
	void *ip; /* kernel ip when scheduling start */
	int(*real_ip)(void *);
	unsigned char **argv;
	struct user_thread userland;
};
#define MAX_FDS 100
struct file;
struct fs_struct {
	struct file *filep[MAX_FDS];
	int total;
};

struct mm_struct {
	struct vm_area_struct *mmap; /* list of VMAs */
	unsigned long pgd;
	atomic_t count; /* How many references to "struct mm_struct" (users count as 1) */
	struct fs_struct fs;
	unsigned long brk_addr, brk_len;
	unsigned long anonymous_addr;
};


// This structure defines a 'task' - a process.
#define MAX_TASK_NAME 40
/*
 - task can be on run queue or in wait queues */
struct task_struct {
	volatile long state;    /* -1 unrunnable, 0 runnable, >0 stopped */
	unsigned long flags;    /* per process flags, defined below */
	unsigned long pending_signal;	
	unsigned long pid,ppid;
	unsigned char name[MAX_TASK_NAME+1];
	int cpu;
	int counter;
	long sleep_ticks;
	unsigned long ticks;	
	struct thread_struct thread;
	struct mm_struct *mm;

	struct list_head run_link; /* run queue */
	struct list_head task_link; /* task queue */
	struct list_head wait_queue; /* wait queue */

	struct {
		unsigned long ticks_consumed;
	} stats;

	unsigned long magic_numbers[4]; /* already stack is default fill with magic numbers */
}; 
#define is_kernelThread(task) (task->mm == g_kernel_mm)
#define IPI_INTERRUPT 200
extern int getcpuid();
extern struct task_struct *g_idle_tasks[];
extern struct task_struct *g_current_tasks[];

static inline struct task_struct *current_task(void)
{
	unsigned long addr,p;
	addr = (unsigned long)&p;
	addr=addr & (~(TASK_SIZE-1));

    return (struct task_struct *)addr;
}
#define g_current_task current_task()


#endif
