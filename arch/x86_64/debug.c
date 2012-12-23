/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/debug.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "common.h"
#include "mach_dep.h"
#if 0
static void debugFault(struct fault_ctx *ctx) {

	ctx->istack_frame->rflags =(ctx->istack_frame->rflags) | 0x10000; /* set RF bit */

	trace[tr_index]=ctx->istack_frame->rip;
	tr_index++;
	if (tr_index > MAX_TRACE){
		tr_index = 0;
	}

}

static int debug_enabled=0;
void start_debugtrace(){
	static int init_intr = 0;
	if (init_intr == 0) {
		ar_registerInterrupt(1, debugFault, "debugFault", NULL);
		init_intr = 1;
	}
	debug_enabled=1;
}
#endif
/**********************************************************************/
#define MAX_BREAKPOINTS 2000
#define MAX_BRK_HASH 2048
#define	DEBUG_PUSHL_EBP		0x55
#define	DEBUG_PATCHVAL		0xcc /* int 3 */
#define	DEBUG_RET			0xc3
#define	DEBUG_LEAVE		0xc9

enum {
	BRKPOINT_START=1,
	BRKPOINT_RET=2
};

static int total_breakpoints=0;
typedef  struct breakpoint_struct breakpoint_t;
struct breakpoint_struct {
	 int symb_index; /* index to the symbol table where the break point is placed */
	 unsigned char type;
	 unsigned long addr;
	 struct {
		 int call_count;
	 }stats;
	 breakpoint_t *next;
};
static breakpoint_t breakpoint_table[MAX_BREAKPOINTS];
static breakpoint_t *brk_hash_table[MAX_BRK_HASH];
static spinlock_t breakpoint_lock = SPIN_LOCK_UNLOCKED;

#define MAX_TRACES 2048
typedef struct {
	int breakpoint_index;
	int stack_depth;
	int cpu_id;
	int pid;
	int count;

} trace_t;

static int g_conf_trace_enable=1;
static int trace_index=0;
static trace_t traces[MAX_TRACES];

//static spinlock_t trace_lock = SPIN_LOCK_UNLOCKED;

static int breakpoint_add_trace(int brk_index) {

	traces[trace_index].breakpoint_index = brk_index;
	traces[trace_index].cpu_id = getcpuid();
	traces[trace_index].pid = g_current_task->pid ;
	traces[trace_index].count=0;
	if (breakpoint_table[brk_index].type == BRKPOINT_START){
		g_current_task->trace_stack_length++;
	}else{
		int prev_brk_indx,prev_sym_indx;

		g_current_task->trace_stack_length--;
#if 1  /* trace collapse : 1) begin and end collapse, 2) identical calls on same cpu */
		if (trace_index > 0 && traces[trace_index-1].cpu_id==traces[trace_index].cpu_id) {
			prev_brk_indx=traces[trace_index-1].breakpoint_index;
			prev_sym_indx=breakpoint_table[prev_brk_indx].symb_index;
			if (prev_sym_indx == breakpoint_table[brk_index].symb_index) {
				traces[trace_index-1].count++;
				trace_index--;
				if (trace_index>0 && traces[trace_index-1].breakpoint_index == traces[trace_index].breakpoint_index && traces[trace_index-1].cpu_id==traces[trace_index].cpu_id) {
					traces[trace_index-1].count=traces[trace_index-1].count+traces[trace_index].count;
					trace_index--;
				}
				trace_index++;

				return 1;
			}
		}
#endif
	}
	traces[trace_index].stack_depth = g_current_task->trace_stack_length;
	trace_index++;
	if (trace_index >= MAX_TRACES){
		g_conf_trace_enable=0;
		//trace_index = 0;
	}

	return 1;
}
static int breakpoint_fault(struct fault_ctx *ctx) {
	unsigned long addr = ctx->istack_frame->rip - 1;
	int index = addr % MAX_BRK_HASH;
	breakpoint_t *brk_p = brk_hash_table[index];

	while (brk_p != 0 && brk_p->addr != addr) {
		brk_p = brk_p->next;
	}
	if (brk_p != 0 && brk_p->addr == addr) {
		if (brk_p->symb_index == -1)
			BUG();
		brk_p->stats.call_count++;
		if (g_conf_trace_enable == 1) {
			int brk_index = ((unsigned long)brk_p - (unsigned long)(&breakpoint_table[0]))/(sizeof(breakpoint_t));
			breakpoint_add_trace(brk_index);
		}
		return brk_p->type;
	}
	BUG();
	return 0;
}
int Jcmd_enable_trace(unsigned char *arg_name, unsigned char *arg2){
	trace_index=0;
	g_conf_trace_enable=1;
	return 1;
}
int Jcmd_print_trace(unsigned char *arg_pid, unsigned char *arg_cpu_id) {
	int i, j;
    int brk_idx,sym_idx;
    int pid = -1;
    int count=0;

    if (arg_pid != 0){
    	pid = ut_atoi(arg_pid);
    }
	for (i = 0; i < trace_index && i < MAX_TRACES; i++) {
		if (pid!=-1 && traces[i].pid!=pid) continue;
		count++;
		ut_printf("%d:",traces[i].cpu_id);
		for (j = 0; j < traces[i].stack_depth; j++) {
			ut_printf("  ");
		}
		brk_idx = traces[i].breakpoint_index;
		sym_idx = breakpoint_table[brk_idx].symb_index;
		if (breakpoint_table[brk_idx].type == BRKPOINT_START) {
			if (traces[i].count>0)
				ut_printf("-:%s:%s(%d)[%d]\n",g_symbol_table[sym_idx].name, g_symbol_table[sym_idx].file_lineno, traces[i].count, traces[i].pid);
			else
				ut_printf(">:%s:%s[%d]\n",g_symbol_table[sym_idx].name, g_symbol_table[sym_idx].file_lineno, traces[i].pid);
		} else {
			ut_printf("  <:%s[%d]\n",g_symbol_table[sym_idx].name, traces[i].pid);
		}
	}
	ut_printf(" Total trace records : %d \n",count);
	return 1;
}

static int breakpoint_delete(int sym_index) {
	int i = 0;
	int ret=0;
	int index;
	unsigned long flags;
	unsigned char *instr;

	while (i < MAX_BREAKPOINTS) {
		if (breakpoint_table[i].symb_index == sym_index) {

			spin_lock_irqsave(&breakpoint_lock, flags);
			instr = breakpoint_table[i].addr;
			if (instr[0] == DEBUG_PATCHVAL){
				if (breakpoint_table[i].type == BRKPOINT_START){
					instr[0] = DEBUG_PUSHL_EBP;
				}else if (breakpoint_table[i].type == BRKPOINT_RET){
					instr[0] = DEBUG_LEAVE;
				}else {
					BUG();
				}
			}else{
				BUG();
			}
			index=breakpoint_table[i].addr % MAX_BRK_HASH;

			breakpoint_t *ptr =(breakpoint_t *) brk_hash_table[index];
			if (ptr == &breakpoint_table[i]){
				brk_hash_table[index]=breakpoint_table[i].next;
			}else{
				breakpoint_t *prev_ptr = ptr;
				while (ptr != 0 && ptr!=&breakpoint_table[i]){
					prev_ptr = ptr;
					ptr = ptr->next;
				}
				if (ptr == &breakpoint_table[i]){
                    prev_ptr->next = breakpoint_table[i].next;
				}else{
					BUG();
				}
			}
			breakpoint_table[i].symb_index = -1;
			breakpoint_table[i].addr = 0;
			breakpoint_table[i].next = 0;
			spin_unlock_irqrestore(&breakpoint_lock, flags);

			ret++;
		} else {
			i++;
		}
	}
	total_breakpoints = total_breakpoints -ret;
	return ret;
}

static int breakpoint_add(int sym_index, unsigned long addr, unsigned char type) {
	unsigned long flags;
	int index,i;

	spin_lock_irqsave(&breakpoint_lock, flags);
	for (i = 0; i < MAX_BREAKPOINTS; i++)
		if (breakpoint_table[i].addr == 0) {
			break;
		}
	breakpoint_table[i].symb_index = sym_index;
	breakpoint_table[i].addr = addr;
	breakpoint_table[i].type = type;
	breakpoint_table[i].stats.call_count = 0;
	index = addr % MAX_BRK_HASH;
	breakpoint_table[i].next = brk_hash_table[index];
	brk_hash_table[index] = &breakpoint_table[i];

	total_breakpoints++;
	spin_unlock_irqrestore(&breakpoint_lock, flags);

	return 1;
}

static int breakpoint_create(int sym_index){
	unsigned char *instr;
	int  k;
	int ret=0;

	instr = g_symbol_table[sym_index].address;
	if (instr[0] != DEBUG_PUSHL_EBP) {
		return ret;
	}

	/* patchup start of function */
	if (breakpoint_add(sym_index, (unsigned long) instr, BRKPOINT_START) == 1) {
		instr[0] = DEBUG_PATCHVAL;
		ret++;
	}

	/* patchup the returns: there should only one return  */
	for (k = 0; (instr + k) < g_symbol_table[sym_index + 1].address; k++) {
		if (instr[k] == DEBUG_LEAVE && instr[k + 1] == DEBUG_RET) {
			if (breakpoint_add(sym_index, (unsigned long) instr+k, BRKPOINT_RET) == 1) {
				instr[k] = DEBUG_PATCHVAL;
				ret++;
				if (ret >2 ){
					ut_printf("ERROR:ret:%d instr:%x in:%x s_ind:%d  addr:%x \n",ret,instr,instr+k,sym_index,g_symbol_table[sym_index + 1].address);
				}
			}
		}
	}
	if (ret!=0 && ret!=2){
		ut_printf("ERROR: something Wrong name :%s:  rt:%d \n",g_symbol_table[sym_index].name, ret);
	}
	return ret;
}

int init_breakpoints(){
	int i;

	for (i=0; i<MAX_BREAKPOINTS; i++){
		breakpoint_table[i].symb_index = -1;
		breakpoint_table[i].next = 0;
		breakpoint_table[i].addr = 0;
		breakpoint_table[i].stats.call_count = 0;
	}
	for (i=0; i<MAX_BRK_HASH; i++){
		brk_hash_table[i] = 0;
	}
	ar_registerInterrupt(3, breakpoint_fault, "trapFault", NULL);
	return 0;
}
int Jcmd_call_stats(unsigned char *arg1,unsigned char * arg2){
	int i,j,k;

	k=0;
	for (i=0; i<MAX_BREAKPOINTS; i++){
		if (breakpoint_table[i].addr==0 || breakpoint_table[i].stats.call_count==0 || breakpoint_table[i].type!=BRKPOINT_START) continue;

		j=breakpoint_table[i].symb_index;
		ut_printf(" %8s : %5d\n",g_symbol_table[j].name,breakpoint_table[i].stats.call_count);
		k=k+breakpoint_table[i].stats.call_count;
		breakpoint_table[i].stats.call_count=0;
	}
	ut_printf("Total calls :%d\n",k);
	return 1;
}
// "/task.c:","/kernel.c:",
static unsigned char *discard_patterns[]={".s:",".S:",".h:","/x86_64/","/debug.c:","serial.c:","/display.c:",0};
int Jcmd_break_group(unsigned char *arg_name, unsigned char *arg2) {
	int i, k;
	int ret = 0;

	if (arg_name!=0 && ut_strcmp(arg_name, "delete") == 0) {
		for (i = 3; i < g_total_symbols; i++) {
			if (g_symbol_table[i].type != SYMBOL_TEXT) continue;
			ret = ret + breakpoint_delete(i);
		}
		ut_printf(" %d breakpoints deleted\n");
	}
	for (i = 3; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != SYMBOL_TEXT)
			continue;
		if (g_symbol_table[i].file_lineno[0]=='\0') continue;
		if (ut_strcmp(arg_name, "all") == 0) {
			int k = 0;
			while (discard_patterns[k] != 0) {
				if (ut_strstr((unsigned char *) g_symbol_table[i].file_lineno,discard_patterns[k]) != 0)
					goto next;
				k++;
			}
		} else {
			if (ut_strstr((unsigned char *) g_symbol_table[i].file_lineno,(unsigned char *) arg_name) == 0)
				continue;
		}

		if (arg2 != 0 && ut_strcmp(arg2, "delete") == 0) {
			ret = ret + breakpoint_delete(i);
		} else {
			ret = ret + breakpoint_create(i);
		//	ut_printf("breakpoint inserted for the func :%s: \n",g_symbol_table[i].name);
		}

		next: ;
	}
	ut_printf(" Total breaks :%s: ret=%d  index=%d\n", arg_name, ret, i);
	return ret;
}

int Jcmd_break(unsigned char *arg_name, unsigned char *arg2) {
	int i, k;
	int ret = 0;

	if (arg_name==0 || arg_name[0]=='\0') return 0;
	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != SYMBOL_TEXT)
			continue;
		if (ut_strcmp((unsigned char *) g_symbol_table[i].name, (unsigned char *) arg_name) != 0)
			continue;
		break;
	}

	if (i != g_total_symbols) {
		if (arg2!=0 && ut_strcmp(arg2,"delete")==0){
			ret = breakpoint_delete(i);
		}else{
			ret = breakpoint_create(i);
		}

	}

	ut_printf(" break :%s: ret=%d\n", arg_name, ret);
	return ret;
}
int Jcmd_break_delete(unsigned char *arg_name, unsigned char *arg2){
	return Jcmd_break(arg_name,"delete");
}

