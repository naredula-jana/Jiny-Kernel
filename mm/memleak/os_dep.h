#ifndef __OSDEP_H
#define __OSDEP_H

#define MAX_STATIC_OBJS 100
#define MAX_STATIC_SCAN_AREAS 100



#include "list.h"
#include "common.h"
#include "atomic.h"
#define MAX_TASK_NAME 40
#define NULL ((void *) 0)
#define TASK_SIZE 4*(0x1000)




extern unsigned long g_jiffies;
//extern long g_idle_tasks;
extern unsigned long  end;
#define printf ut_printf
#define pr_debug ut_printf
#define strcmp ut_strcmp

#endif
