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
/*******************************   Usage *****************************
Generating Trace:
k break_install all   --  adds the breakpoint in to code in all location
k trace_start SYS_fs_open  -- start tracing from some start function
  Run some application
k trace_start     -- remove starting function to trace

k print_trace    -- print trace
k call_stats     -- function frequesncy
k break_install delete  -- delete all the breakpoints

Generating call graph:
k break_install all   -- add the break points
  run the app or what ever code so the graph need to be captured
k break_install delete
k call_graph

********************************************************************/
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
#define MAX_CALLOUTS 40
typedef  struct breakpoint_struct breakpoint_t;
struct breakpoint_struct {
	 int symb_index; /* index to the symbol table where the break point is placed */
	 unsigned char type;
	 unsigned long addr;
	 struct {
		 int call_count;

	 }stats;
	 struct {
		 int symb_index;
		 int count;
	 }call_outs[MAX_CALLOUTS];
	 int call_out_count;

	 breakpoint_t *next;
};
static breakpoint_t breakpoint_table[MAX_BREAKPOINTS];
static breakpoint_t *brk_hash_table[MAX_BRK_HASH];
static spinlock_t breakpoint_lock = SPIN_LOCK_UNLOCKED("breakpoint");
#define MAX_BREAK_STARTINGPOINTS 100
static int startpoint_count=0;
static int breakpoint_starts[MAX_BREAK_STARTINGPOINTS];

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
static int g_conf_trace_user=1; /* trace only user space threads inside the kernel , especially interested in syscall */
static trace_t traces[MAX_TRACES];



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
  /* trace collapse : 1) begin and end collapse, 2) identical calls on same cpu */
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
    int j;
    unsigned long flags;

	while (brk_p != 0 && brk_p->addr != addr) {
		brk_p = brk_p->next;
	}
	if (brk_p != 0 && brk_p->addr == addr) {
		if (brk_p->symb_index == -1)
			BUG();
		brk_p->stats.call_count++;

		/* storing callout data */
		spin_lock_irqsave(&breakpoint_lock, flags);
		//if (brk_p->type == BRKPOINT_START && g_current_task->mm != g_kernel_mm) { /* only user level threads */
		if (brk_p->type == BRKPOINT_START ) {
			int top = g_current_task->callstack_top;
			breakpoint_t *prev_brk_p =0;
			if (top > 0)
				prev_brk_p = g_current_task->callstack[top-1];
			else {
				if (top<0) top=0;
			}

			if (prev_brk_p != 0) {
				int i;
				int call_found = 0;
				int count = prev_brk_p->call_out_count;
				for (i = 0; i < count; i++) {
					if (prev_brk_p->call_outs[i].symb_index
							== brk_p->symb_index) {
						prev_brk_p->call_outs[i].count++;
						call_found = 1;
						break;
					}
				}
				if (call_found == 0
						&& (prev_brk_p->call_out_count < MAX_CALLOUTS)) {
					i = prev_brk_p->call_out_count;
					prev_brk_p->call_out_count++;
					prev_brk_p->call_outs[i].symb_index = brk_p->symb_index;
					prev_brk_p->call_outs[i].count = 1;
				}
			}
			top++;
			if (top <= MAX_DEBUG_CALLSTACK){
				g_current_task->callstack[top-1] = brk_p;
				g_current_task->callstack_top =top;
			}

		}else{
			g_current_task->callstack_top--;
			if (g_current_task->callstack_top < 0){
				g_current_task->callstack_top = 0;
			}
		}
		spin_unlock_irqrestore(&breakpoint_lock, flags);

		if (g_conf_trace_enable == 1) {
			if (g_conf_trace_user == 1) { /* user level threads */
				int found=0;
				if (g_current_task->mm == g_kernel_mm)  return brk_p->type;
				for (j = 0; j < startpoint_count; j++) {
					if (breakpoint_starts[j] == brk_p->symb_index) {
						found =1;
						if (g_current_task->trace_on == 1 && brk_p->type == BRKPOINT_RET){
							g_current_task->trace_on = 0;
						}else if (g_current_task->trace_on == 0 && brk_p->type == BRKPOINT_START){
							g_current_task->trace_on = 1;
						}
						break;
					}
				}
				if (found == 0 && g_current_task->trace_on==0){
					return brk_p->type;
				}

				int brk_index = ((unsigned long) brk_p - (unsigned long) (&breakpoint_table[0])) / (sizeof(breakpoint_t));
				breakpoint_add_trace(brk_index);
			}else{ /* kernel thread */
				int brk_index = ((unsigned long) brk_p - (unsigned long) (&breakpoint_table[0])) / (sizeof(breakpoint_t));
				breakpoint_add_trace(brk_index);
			}
		}
		return brk_p->type;
	}
	BUG();
	return 0;  /* unreachable */
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
	breakpoint_table[i].call_out_count = 0 ;
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
	}else{
		return ret;
	}

	/* patchup the returns: there should only one return  */
	for (k = 0; (instr + k) < g_symbol_table[sym_index + 1].address; k++) {
		if (instr[k] == DEBUG_LEAVE && instr[k + 1] == DEBUG_RET) {
			if (breakpoint_add(sym_index, (unsigned long) instr+k, BRKPOINT_RET) == 1) {
				instr[k] = DEBUG_PATCHVAL;
				ret++;
				if (ret >2 ){
					ut_printf("ERROR:ret:%d instr:%x in:%x s_ind:%d  addr:%x \n",ret,instr,instr+k,sym_index,g_symbol_table[sym_index + 1].address);
				    goto last;
				}
			}
		}
	}
last:
	if (ret!=0 && ret!=2){

		if (ret ==1){  /* remove the breakpoint at the start of the function */
			breakpoint_delete(sym_index);
			ut_printf("WARNING : something Wrong name :%s:  rt:%d , REVERETED to old state\n",g_symbol_table[sym_index].name, ret);
		}else{
			ut_printf("ERROR: something Wrong name :%s:  rt:%d \n",g_symbol_table[sym_index].name, ret);
		}
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
static unsigned char *discard_subsystems[]={"ut_",0};
static int check_discard_subsystems(unsigned char *p){
	int j;

	for (j=0; discard_subsystems[j]!=0; j++){
		if (ut_strcmp(p, (unsigned char *) discard_subsystems[j]) == 0){
			return 1;
		}
	}
	return 0;
}
static unsigned char *subsystems[]={"SYS_","fs_",0};
static void print_clusters() {
	int i, j,k,l;
	for (j = 0; subsystems[j] != 0; j++) {
		int first = 1;
		ut_printf(
				"subgraph cluster_0 {\n style=filled;\n color=lightgrey; \n node [style=filled,color=white];\n rankdir = TB;\n");

		for (i = 0; i < g_total_symbols; i++) {
			if (ut_strcmp(subsystems[j], g_symbol_table[i].subsystem_type) != 0)
				continue;
			for (k = 0; k < MAX_BREAKPOINTS; k++) {
				if (breakpoint_table[k].call_out_count == 0)
					continue;
				if (breakpoint_table[k].symb_index == i)
					goto success;
				for (l = 0; l < breakpoint_table[k].call_out_count; l++) {
					if (breakpoint_table[k].call_outs[l].symb_index == i)
						goto success;
				}
			}
			continue;
	success:
			if (first == 1)
				ut_printf("%s ", g_symbol_table[i].name);
			else
				ut_printf(";%s ", g_symbol_table[i].name);
			first = 0;
		}
		ut_printf(";\nlabel = %s;\n}\n",subsystems[j]);
	}
	return;
}
int Jcmd_call_stats(unsigned char *arg1,unsigned char * arg2){
	int i,j,k,m;
    struct {
		unsigned long subsystem;
		int count;
	}subsystem_stats[30];
	int count=0;

	if (ut_strcmp(arg1,"help") ==0){
		ut_printf("call_stats <graph>  -- print graphs\n call_stats -- print subsystem stats \n");
	}
	k=0;
	if (ut_strcmp(arg1,"graph") !=0){
		for (i = 0; i < MAX_BREAKPOINTS; i++) {
			int found = 0;
			if (breakpoint_table[i].addr == 0
					|| breakpoint_table[i].stats.call_count == 0
					|| breakpoint_table[i].type != BRKPOINT_START)
				continue;
			k = breakpoint_table[i].symb_index;
			for (j = 0; j < count; j++) {
				if (subsystem_stats[j].subsystem == g_symbol_table[k].subsystem_type) {
					found = 1;
					subsystem_stats[j].count++;
					break;
				}
			}
			if (found == 0) {
				subsystem_stats[count].subsystem = g_symbol_table[k].subsystem_type;
				subsystem_stats[count].count = 1;
				count++;
			}
		}
		for (m = 0; m < count; m++) {
			int func_count=0;
			ut_printf("%5s -- %d\n",subsystem_stats[m].subsystem,subsystem_stats[m].count);
			for (i = 0; i < MAX_BREAKPOINTS; i++) {
				if (breakpoint_table[i].addr == 0
						|| breakpoint_table[i].stats.call_count == 0
						|| breakpoint_table[i].type != BRKPOINT_START )
					continue;
				j = breakpoint_table[i].symb_index;
				if (g_symbol_table[j].subsystem_type != subsystem_stats[m].subsystem ) continue;
				if((func_count % 5)==0) ut_printf("    ");
				ut_printf("%s(%d),", g_symbol_table[j].name,breakpoint_table[i].stats.call_count);
				func_count++;
				if ((func_count % 5)==0) ut_printf("\n");
				k = k + breakpoint_table[i].stats.call_count;
				breakpoint_table[i].stats.call_count = 0;
			}
			ut_printf("\n");
		}
		return 0;
	}

/* generate the graph */

	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		int sindex = breakpoint_table[i].symb_index;

		if (breakpoint_table[i].type == BRKPOINT_START
				&& breakpoint_table[i].call_out_count > 0) {

			if (check_discard_subsystems(g_symbol_table[sindex].subsystem_type)==1) continue;

			if (g_symbol_table[sindex].subsystem_type != 0
					&& ut_strcmp(g_symbol_table[sindex].subsystem_type, "SYS_") == 0) {
				ut_printf(" %s [style=filled, color=green];\n", g_symbol_table[sindex].name);
			} else if (g_symbol_table[sindex].subsystem_type != 0
					&& ut_strcmp(g_symbol_table[sindex].subsystem_type, "p9_") == 0) {
				ut_printf(" %s [style=filled, color=red];\n", g_symbol_table[sindex].name);
			} else if (g_symbol_table[sindex].subsystem_type != 0
					&& ut_strcmp(g_symbol_table[sindex].subsystem_type, "fs_") == 0) {
				ut_printf(" %s [style=filled, color=orange];\n", g_symbol_table[sindex].name);
			}

			for (j = 0; j < breakpoint_table[i].call_out_count; j++) {
				int s = breakpoint_table[i].symb_index;
				int d = breakpoint_table[i].call_outs[j].symb_index;
				if (check_discard_subsystems(g_symbol_table[d].subsystem_type)==1) continue;
				ut_printf(" %s -> %s  [label = %d];\n", g_symbol_table[s].name, g_symbol_table[d].name, breakpoint_table[i].call_outs[j].count);
			}
		}
	}
	print_clusters();

	ut_printf("Total calls :%d\n",k);
	return 1;
}
// "/task.c:","/kernel.c:",
static unsigned char *discard_patterns[]={".s:",".S:",".h:","/x86_64/","/debug.c:","serial.c:","/display.c:",0};
int Jcmd_break_install(unsigned char *arg_name, unsigned char *arg2) {
	int i, k;
	int ret = 0;

	if (ut_strcmp(arg_name,"help") ==0){
		ut_printf("break_install all : install break points all locations\n break_install delete : delete all existed break points \n");
		return 1;
	}
	if (arg_name!=0 && ut_strcmp(arg_name, "delete") == 0) {
		for (i = 3; i < g_total_symbols; i++) {
			if (g_symbol_table[i].type != SYMBOL_GTEXT && g_symbol_table[i].type != SYMBOL_LTEXT) continue;
			ret = ret + breakpoint_delete(i);
		}
		ut_printf(" %d breakpoints deleted\n");
	}
	for (i = 3; i < g_total_symbols; i++) {
		//if (g_symbol_table[i].type != SYMBOL_GTEXT && g_symbol_table[i].type != SYMBOL_LTEXT)
		if (g_symbol_table[i].type != SYMBOL_GTEXT)
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
	ut_printf(" Total breaks :%s: ret=%d  index=%d  cpuid:%d\n", arg_name, ret, i,getcpuid());
	return ret;
}
int Jcmd_trace_start(unsigned char *arg_name){
	int i,j;

	if (ut_strcmp(arg_name,"help") ==0){
		ut_printf("break_start <starting_fucntion>: break starts from this function \nbreak_start  <empty> : Removing all starting functions\n");
		return 0;
	}
	if (arg_name == NULL) {

		startpoint_count =0;
		return 0;
	}
	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != SYMBOL_GTEXT && g_symbol_table[i].type != SYMBOL_LTEXT)
			continue;
		if (ut_strcmp((unsigned char *) g_symbol_table[i].name,
				(unsigned char *) arg_name) != 0)
			continue;
		if (startpoint_count < MAX_BREAK_STARTINGPOINTS) {
			for (j = 0; j < startpoint_count; j++) {
				if (breakpoint_starts[j] == -1) {
					breakpoint_starts[j] = i;
					return 1;
				}
			}
			if (startpoint_count < (MAX_BREAK_STARTINGPOINTS-1)){
				breakpoint_starts[startpoint_count] = i;
				startpoint_count++;
				return 1;
			}
		}
	}
	ut_printf("Error : Unable to start the break point @ :%s: \n",arg_name);
	return 0;
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


