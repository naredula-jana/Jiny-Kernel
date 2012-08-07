#ifndef __OSDEP_H
#define __OSDEP_H

#define MAX_STATIC_OBJS 100
#define MAX_STATIC_SCAN_AREAS 100

#if 0
#include "common.h"
#include "task.h"

#else

#include "list.h"
#include "spinlock.h"
#include "atomic.h"
#define MAX_TASK_NAME 40
#define NULL ((void *) 0)
#define TASK_SIZE 4*(0x1000)
typedef long  size_t;

#endif

extern unsigned long g_jiffies;
extern struct task_struct *g_current_task;
extern long g_idle_tasks;
extern void *placement_address;
#define printf ut_printf
#define pr_debug ut_printf
#define strcmp ut_strcmp

#endif
