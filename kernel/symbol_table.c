/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/symbol_table.c
 *   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
/***********
 *    Jconf_xxx  - conf variables
 *    Jcmd_xxx   - commands in the format cmd arg1 arg2
 *    Jcmd_xxx_stat  - display stats
 */
#include "common.h"


symb_table_t *g_symbol_table = 0;
unsigned long g_total_symbols = 0;
/**************************************
 * Subsytems: sc    -schedule ,
 *            fs    -vfs file system ,
 *            SYS   -syscalls,
 *            ar    -machineDepenedent,
 *            sock  -network
 *            vm    -virtual memory
 *            mem   -memory
 *            ipc   -sys,ipc ...  : ipc layer
 *            pc    -page cache
 *            sysctl-Jcmd_,g_conf_
 *            ut    -utilities
 *
 *            lwip
 *
 */
static char *subsystems[]={"SYS_","sc_","ut_","fs_","p9_","ipc_","pc_","ar_","mm_","vm_","Jcmd_","virtio_",0};
int init_symbol_table(unsigned long unused) {
	int i,j;
	int confs = 0;
	int stats = 0;
	int cmds = 0;

	if (g_multiboot_mod1_addr==0) {
		ut_log("ERROR: Symbols are not Loaded\n");
		return 0;
	}
	g_symbol_table = module_load_kernel_symbols(__va(g_multiboot_mod1_addr),g_multiboot_mod1_len);
	if (g_symbol_table == 0){
		BUG();
	}
	for (i = 0;  g_symbol_table[i].name != 0; i++) {
		unsigned char sym[100], dst[100];

		g_symbol_table[i].subsystem_type = 0;
		g_symbol_table[i].file_lineno = "UNKNOWN-lineno"; /* TODO need to extract from kernel file */
		/* detect subsytem type */
		for (j=0; subsystems[j]!=0; j++){
			if (ut_strstr(g_symbol_table[i].name, (unsigned char *) subsystems[j]) != 0){
				g_symbol_table[i].subsystem_type = subsystems[j];
				break;
			}
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[7] = '\0'; /* g_conf_ */
		ut_strcpy(dst, (unsigned char *)"g_conf_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_CONF;
			confs++;
			continue;
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[5] = '\0'; /* Jcmd_ */
		ut_strcpy(dst, (unsigned char *)"Jcmd_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_CMD;
			cmds++;
			continue;
		}
	}
	g_total_symbols = i;
	ut_log("	confs:%d  cmds:%d  totalsymbols:%d \n",
			confs, cmds, g_total_symbols);
//	init_breakpoints();
	return 0;
}

int ut_symbol_show(int type){
	int i,count=0;
    int *conf;

	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != type) continue;
		conf=(int *)g_symbol_table[i].address;
		if (type==SYMBOL_CONF)
		    ut_printf("   %9s = %d\n",g_symbol_table[i].name,*conf);
		else
		    ut_printf("   %s: \n",&g_symbol_table[i].name[5]);
		count++;
	}
	return count;
}

int ut_symbol_execute(int type, char *name, uint8_t *argv1,uint8_t *argv2){
    int i,*conf;
	int (*func)(char *argv1,char *argv2);

	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != type) continue;

		if (type==SYMBOL_CONF){
			if (ut_strcmp((unsigned char *)g_symbol_table[i].name,(unsigned char *) name) != 0) continue;
		    conf=(int *)g_symbol_table[i].address;
		    if (argv1==0) return 0;
		    *conf=(int)ut_atoi((unsigned char *)argv1);
		    return 1;
		}else {/*this is Jcmd_  leave 5 characters and match */
			if (ut_strcmp((unsigned char *)&g_symbol_table[i].name[5], (unsigned char *)name) != 0) continue;
			func=(void *)g_symbol_table[i].address;
			func(argv1,argv2);
			return 1;
		}
	}
	return ut_mod_symbol_execute(type,name,argv1,argv2);
}

unsigned long ut_get_symbol_addr(unsigned char *name) {
	int i;
	for (i=0; i< g_total_symbols; i++)
	{
		if (ut_strcmp(name,g_symbol_table[i].name)==0){
			return g_symbol_table[i].address;
		}
	}
	return ut_mod_get_symbol_addr(name); /* search in the modules */
}
unsigned char *ut_get_symbol(addr_t addr) {
	int i;
	for (i=0; i< g_total_symbols; i++)
	{
		if ((addr>=g_symbol_table[i].address) && (addr<g_symbol_table[i+1].address))
		{
			//ut_printf("   :%s + %x addr:%x i:%d\n",g_symbol_table[i].name,(addr-g_symbol_table[i].address),addr,i);
			return g_symbol_table[i].name;
		}
	}
	return 0;
}

void ut_printBackTrace(unsigned long *bt, unsigned long max_length){
	int i;
	unsigned char *name;


	for (i=0; i<max_length; i++){
		if (bt[i] == 0) return;
		name = ut_get_symbol(bt[i]);
		ut_log(" %d :%s - %x\n",i+1,name,bt[i]);
	}
	return;
}
void ut_storeBackTrace(unsigned long *bt, unsigned long max_length){
	unsigned long *rbp;
	unsigned long lower_addr,upper_addr;
	int i;

	asm("movq %%rbp,%0" : "=m" (rbp));
	lower_addr = g_current_task;
	upper_addr = lower_addr + TASK_SIZE;
	for (i=0; i<max_length; i++){
		bt[i]=0;
	}
	for (i=0; i<max_length; i++){
		bt[i] = *(rbp+1);
		rbp=*rbp;
		if (rbp<lower_addr || rbp>upper_addr) break;
	}
	return;
}

void ut_getBackTrace(unsigned long *rbp, unsigned long task_addr, backtrace_t *bt){
	int i;
	unsigned char *name;
	unsigned long lower_addr,upper_addr;

	if (rbp == 0) {
		asm("movq %%rbp,%0" : "=m" (rbp));
		lower_addr = g_current_task;
		upper_addr = lower_addr + TASK_SIZE;
	} else {
		lower_addr = task_addr;
		upper_addr = lower_addr + TASK_SIZE;
	}
	if (bt == 0){
		struct task_struct *t = lower_addr;
		ut_log("taskname: %s  pid:%d\n",t->name,t->pid);
	}
	for (i=0; i<MAX_BACKTRACE_LENGTH; i++){
		if (rbp<lower_addr || rbp>upper_addr) break;
		name = ut_get_symbol(*(rbp+1));
		if (bt){
			bt->entries[i].name = name;
			bt->entries[i].ret_addr = *(rbp+1);
			bt->count = i+1;
		}else{
			ut_log(" %d :%s - %x\n",i+1,name,*(rbp+1));
		}
		rbp=*rbp;
		if (rbp<lower_addr || rbp>upper_addr) break;
	}
	return;
}

void Jcmd_bt(unsigned char *arg1,unsigned char *arg2){

	ut_getBackTrace(0,0,0);
}
/*****************************************Dwarf related code  ***********************************************/

static int dwarf_count=0;
static struct dwarf_entry *dwarf_table;
static int dwarf_init_done=0;
static void init_dwarf() {
	int ret,tret;
	unsigned char *p;

	dwarf_table = vmalloc(0x1f0000,0);
	if (dwarf_table == 0){
		ut_log(" init_dwarf: ERROR : fail allocated memory \n");
		return;
	}
	struct file *file = fs_open("dwarf_datatypes", 0, 0);
	if (file == 0) {
		ut_log(" init_dwarf: ERROR : fail to open the file \n");
	}
	p=dwarf_table;
	ret=0;
	do {
		tret = fs_read(file, p, 0x200000-ret);
		if (tret>0) {
			p=p+tret;
			ret=ret+tret;
		}
		if (ret <= 0) {
			ut_log(" init_dwarf: ERROR : fail to read the file: %x \n", ret);
			break;
		}
	}while(tret>0 && ((0x200000-ret)>0));

	fs_close(file);
	dwarf_count = ret / sizeof(struct dwarf_entry);
	if (dwarf_count < 0){
		dwarf_count = 0;
	}
	ut_log("init_dwarf : read number of bytes :%d records:%d \n", ret,
			dwarf_count);
	if (ret > 0) {
		dwarf_init_done = 1;
	}
}

static int get_type(int j, int *size){
	int i;

	i = dwarf_table[j].type_index;
	if (i==-1) i=j;
	if ((dwarf_table[i].tag == DW_TAG_structure_type) || (dwarf_table[i].tag == DW_TAG_base_type) || (dwarf_table[i].tag == DW_TAG_pointer_type)){
		*size = dwarf_table[i].size;
		return i;
	}else{
		i = dwarf_table[i].type_index;
		if (i==-1) return 0;
		if ((dwarf_table[i].tag == DW_TAG_structure_type) || (dwarf_table[i].tag == DW_TAG_base_type) || (dwarf_table[i].tag == DW_TAG_pointer_type)){
			*size = dwarf_table[i].size;
			return i;
		}
	}
	return 0;
}
static void print_data_structures(int i,unsigned long addr) {
	int j;
	if (i == 0 || i==-1)
		return;
	if (i >= dwarf_count ) {
		ut_printf(" ERROR: Dwarf index :%d  max:%d\n", i, dwarf_count);
		return;
	}

	if (dwarf_table[i].tag == DW_TAG_variable) {
		int size;
		unsigned long *p=addr;
		int next_type=dwarf_table[i].type_index;
		ut_printf("%d variable:%s value: 0x%x(%d) next_type:%d\n", i,dwarf_table[i].name, p,p,next_type);
		if (next_type==0 || next_type==-1) return ;
		if (dwarf_table[next_type].tag == DW_TAG_pointer_type){
			print_data_structures(next_type,*p);
		}else{
			print_data_structures(next_type,p);
		}
	} else if (dwarf_table[i].tag == DW_TAG_structure_type) {

		ut_printf(" %d Structure:%s len:%d index:%d addr:%x \n", i,dwarf_table[i].name, dwarf_table[i].size, dwarf_table[i].type_index,addr);
		for (j=i+1; dwarf_table[j].level>1; j++){
			int size;
			int member = get_type(j,&size);
		//	ut_printf("%d Member:%s  index:%d  location:%d", j,dwarf_table[j].name,  dwarf_table[j].type_index, dwarf_table[j].member_location);
			if (member !=0){
				unsigned char *p=addr+dwarf_table[j].member_location;
				if (size==1){
					ut_printf(" %s->%s(%x) size:%d(%s) \n",dwarf_table[j].name,p,p,size,dwarf_table[member].name);
				}else if (size==8){
					unsigned long *data=p;
					ut_printf(" %s->%x(%x) size:%d(%s) \n",dwarf_table[j].name,*data,data,size,dwarf_table[member].name);
				}else {
					ut_printf(" %s->STRUCT size:%d(%s) \n",dwarf_table[j].name,size,dwarf_table[member].name);
				}
			}else{
				ut_printf(" Unknown type \n");
			}
		}
	}else{
	//	ut_printf("%d Other:tag :%d: %s len:%d index:%d \n",i, dwarf_table[i].tag,dwarf_table[i].name, dwarf_table[i].size, dwarf_table[i].type_index);
		print_data_structures(dwarf_table[i].type_index,addr);
	}

}
void Jcmd_dwarf(unsigned char *arg1, unsigned char *arg2) {
	unsigned long addr;
	if (dwarf_init_done == 0) {
		init_dwarf();
	}
	if (dwarf_init_done == 0)
		return;
	if (arg1 == NULL) return;

	addr = ut_get_symbol_addr(arg1);
	if (addr ==0){
		ut_printf(" Error: cannot find the symbol\n");
		return;
	}
	int i;
	for (i=0; (i<dwarf_count) && (dwarf_table[i].tag == DW_TAG_variable); i++){
		if (ut_strcmp(arg1, dwarf_table[i].name) == 0){
			print_data_structures(i, addr);
			return;
		}
	}
	ut_printf(" Error: cannot find the dwarf info\n");
}
