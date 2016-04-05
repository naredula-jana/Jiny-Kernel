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
#include "file.hh"

extern "C" {
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
static int add_symbol_types(unsigned long unused);
extern int init_kernel_module(symb_table_t *symbol_table, int total_symbols);
unsigned char *symbols_end = 0;
static char *subsystems[] = { "SYS_", "sc_", "ut_", "fs_", "p9_", "ipc_", "pc_", "ar_", "mm_", "vm_", "Jcmd_", "virtio_", 0 };
extern unsigned char kernel_args[1024];
int init_kernel_args(unsigned long arg1) {
	unsigned char tmp_arg[200], tmp_value[200];
	int i, j, k, flag_arg;

	/* read and load kernel args */
	j = 0;
	tmp_arg[0] = 0;
	tmp_value[0] = 0;
	flag_arg = 0;
	//return ret;
	ut_log("		kernel args :%s: \n", kernel_args);
	for (i = 0; i < 1023 && j < 200; i++) {
		//	ut_log(" args : %c : %x \n",kernel_args[i],kernel_args[i]);
		//	continue;
		if (kernel_args[i] == 0) {
			break;
		}
#if 1
		if (flag_arg == 0)
			tmp_arg[j] = kernel_args[i];
		else
			tmp_value[j] = kernel_args[i];

		if (kernel_args[i] == '=') {
			tmp_arg[j] = 0;
			j = 0;
			flag_arg = 1;
			continue;
		}

		if (kernel_args[i] == ' ' || kernel_args[i] == ',' || kernel_args[i + 1] == '\0') {
			if (kernel_args[i + 1] == '\0') {
				if (flag_arg == 0) {
					tmp_arg[j + 1] = 0;
				} else {
					tmp_value[j + 1] = 0;
				}
			} else {
				if (flag_arg == 0) {
					tmp_arg[j] = 0;
				} else {
					tmp_value[j] = 0;
				}
			}
			ut_log("		Applying the kernel arg  %s=%s \n", tmp_arg, tmp_value);
			ut_symbol_execute(SYMBOL_CONF, tmp_arg, tmp_value, 0);
			j = -1;
			tmp_arg[0] = 0;
			tmp_value[0] = 0;
			flag_arg = 0;
		}
#endif
		j++;
	}
	return JSUCCESS;
}
unsigned long init_symbol_table(unsigned long bss_start, unsigned long bss_end) {
	int i = 0, k;
	unsigned char *p;
	symb_table_t *sym_table;
	unsigned char *addr, *name;
	unsigned char *start_p = 0;
	unsigned char *end_p, *plen;
	int len;
	unsigned long ret = 0;

	symbols_end = 0;
	g_symbol_table = 0;
	p = bss_start;
	sym_table = bss_end + 0x20;
	g_symbol_table = sym_table;
	while (*p != '\0') {
		if (p[0] == '_' && p[1] == 'S' && p[2] == 'E' && p[3] == 'G' && p[4] == 0xa) {
			start_p = p - 23;
			p = p + 5;
			break;
		}
		p++;
	}
	if (start_p == 0)
		return 0;
	i = 0;
	while (p[0] != 0) {
		addr = p;
		k = 0;
		while (p[0] != 0xa) {
			if (p[0] == ' ') {
				p[0] = 0;
				k++;
				if (k == 1) {
					sym_table->type = p[1];
					plen = p + 1;
				} else if (k == 2 && p[1] != '0') {
					sym_table->type = p[1];
				}
				name = p + 1;
			}
			p++;
		}
		p[0] = 0;
		if (plen && plen[0] == '0') {
			sym_table->len = ut_atoi(plen, FORMAT_HEX);
		} else {
			sym_table->len = 0;
		}
		sym_table->address = ut_atol(addr, FORMAT_HEX);
		sym_table->name = name;
		sym_table++;
//while(1);
		p++;
		i++;
	}
	end_p = p;
	sym_table++;
	g_total_symbols = i + 1;

	p = (unsigned char *) sym_table;
	for (i = 0; i < g_total_symbols - 1; i++) {
		ut_strcpy(p, g_symbol_table[i].name);
		g_symbol_table[i].name = p;
		p = p + ut_strlen(g_symbol_table[i].name) + 1;
	}
	g_symbol_table[i].name = 0;
	ret = p;

	len = end_p - start_p;
	ut_memset(start_p, 0, len);
	symbols_end = ret;
	add_symbol_types(0);
	init_kernel_module(g_symbol_table, g_total_symbols);
	//init_kernel_args(0);
	return ret;
}
#if 1
#define MAX_CLASSESS 100
struct class_types {
	int count; /* currently active objects */
	jobject *list;   /* list of objects */
	unsigned char *name;
	unsigned long sz;
	unsigned long stat_add,stat_remove;
};
//#define CLASS_ID_START 0x5 /* this is to avoid 0 index */
static struct class_types classtype_list[MAX_CLASSESS];
static int class_count = 0;
}

static unsigned long curr_obj_id=1; /* running serial number */
int ut_obj_add(jobject *obj,unsigned char *name, int sz) {
	/* Note: constructor is needed for every class, otherwise entire object initiazed by c++ with zero after new, during that all the entries created in obj class willbe erased. */
	int i;
	int found=0;
	unsigned long flags;

	spin_lock_irqsave(&g_global_lock, flags);
	for (i = 0; i < class_count; i++) {
		if (ut_strstr(classtype_list[i].name, name) != 0) {
			classtype_list[i].count++;

			classtype_list[i].sz = sz;
			obj->next_obj = classtype_list[i].list;
			classtype_list[i].list = obj;
			obj->jobject_id = curr_obj_id;
			obj->jclass_id = i;
			curr_obj_id++;
			classtype_list[i].stat_add++;
			found=1;
			break;
		}
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
#if 1
	if (found==0){
		ut_log("%d: ERROR Class NOT found:%s \n",curr_obj_id,name);
	}else{
	//	ut_log("%d: Class found:%s \n",curr_obj_id,name);
	}
#endif
	return 0;
}
int ut_obj_free(jobject *obj) {
	unsigned long flags;
	int i = obj->jclass_id;
	int ret=JFAIL;

	spin_lock_irqsave(&g_global_lock, flags);

	if (i >= 0 && (i < class_count)) {
		classtype_list[i].count--;
		classtype_list[i].stat_remove++;

		jobject *curr_obj=classtype_list[i].list;
		jobject *prev_obj=classtype_list[i].list;
		while(curr_obj!=0){
			if (curr_obj == obj){
				if (prev_obj == classtype_list[i].list){
					classtype_list[i].list = obj->next_obj;
				}else{
					prev_obj->next_obj = obj->next_obj;
				}
				break;
			}
			prev_obj=curr_obj;
			curr_obj=curr_obj->next_obj;
		}
		ret = JSUCCESS;
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
	return ret;
}

extern "C" {
void Jcmd_obj_list(unsigned char *arg1,unsigned char *arg2) {
	int i;
	int class_id=-1;
	jobject *obj;

	if (arg1 != 0){
		class_id = ut_atoi(arg1, FORMAT_DECIMAL);
	}

	ut_printf("  ClassName           Count    [ add/removed ]  size\n");
	for (i = 0; i < class_count; i++) {
		if (class_id == i ){
			obj=classtype_list[i].list;
			while(obj != 0){
				ut_printf(" %d : ",obj->jobject_id);
				obj->print_stats(0,0);
				obj=obj->next_obj;
			}
		}else if (classtype_list[i].count > 0){
			ut_printf("%d:  %9s  -> %d  [%d/%d]: %d\n",i, &classtype_list[i].name[5], classtype_list[i].count,classtype_list[i].stat_add,classtype_list[i].stat_remove, classtype_list[i].sz);
		}

	}
	return;
}
static int add_symbol_types(unsigned long unused) {
	int i, j;
	int confs = 0;
	int stats = 0;
	int cmds = 0;

	for (i = 0; i < g_total_symbols && g_symbol_table[i].name != 0; i++) {
		unsigned char sym[100], dst[100];

		g_symbol_table[i].subsystem_type = 0;
		g_symbol_table[i].file_lineno = "UNKNOWN-lineno"; /* TODO need to extract from kernel file */
		/* detect subsytem type */
		for (j = 0; subsystems[j] != 0; j++) {
			if (ut_strstr(g_symbol_table[i].name, (unsigned char *) subsystems[j]) != 0) {
				g_symbol_table[i].subsystem_type = subsystems[j];
				break;
			}
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[7] = '\0'; /* g_conf_ */
		ut_strcpy(dst, (unsigned char *) "g_conf_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_CONF;
			confs++;
			continue;
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[7] = '\0'; /* g_conf_ */
		ut_strcpy(dst, (unsigned char *) "g_stat_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_STAT;
			confs++;
			continue;
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[5] = '\0'; /* Jcmd_ */
		ut_strcpy(dst, (unsigned char *) "Jcmd_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_CMD;
			cmds++;
			continue;
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[4] = '\0'; /* Jcmd_ */
		ut_strcpy(dst, (unsigned char *) "_ZTV");
		if (ut_strcmp(sym, dst) == 0) {
			classtype_list[class_count].name = g_symbol_table[i].name;
			classtype_list[class_count].count = 0;
			classtype_list[class_count].list = 0;
			class_count++;
			continue;
		}
	}

	ut_log("	confs:%d  cmds:%d  totalsymbols:%d \n", confs, cmds, g_total_symbols);
//	init_breakpoints();
	return JSUCCESS;
}
#endif
int ut_symbol_show(int type) {
	int i, count = 0;
	int *conf;

	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != type)
			continue;
		conf = (int *) g_symbol_table[i].address;
		if (type == SYMBOL_CONF)
			ut_printf("   %9s = %d\n", g_symbol_table[i].name, *conf);
		else
			ut_printf("   %s: \n", &g_symbol_table[i].name[5]);
		count++;
	}
	return count;
}


static void display_values(int type) {
	int i, len, count;

	count = 0;
	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type == type) {
			//len = g_symbol_table[i+1].address - g_symbol_table[i].address;
			len = g_symbol_table[i].len;
			if (len > 8) {
				unsigned char *val = g_symbol_table[i].address;
				ut_printf("%d: %s -> %s\n", count, g_symbol_table[i].name, val);
			} else if (len == 4) {
				unsigned int *val = g_symbol_table[i].address;
				ut_printf("%d: %s -> %d\n", count, g_symbol_table[i].name, *val);
			} else {
				unsigned long *val = g_symbol_table[i].address;
				ut_printf("%d: %s -> %d\n", count, g_symbol_table[i].name, *val);
			}
			count++;
		}
	}


	return;
}
void Jcmd_clearstats(){
	int i, len, count;

	count = 0;
	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type == SYMBOL_STAT) {
			//len = g_symbol_table[i+1].address - g_symbol_table[i].address;
			len = g_symbol_table[i].len;
			if (len > 8) {
				unsigned char *val = g_symbol_table[i].address;
				ut_printf("clearing %d: %s -> %s\n", count, g_symbol_table[i].name, val);
				*val=0;
			} else if (len == 4) {
				unsigned int *val = g_symbol_table[i].address;
				ut_printf("clearing %d: %s -> %d\n", count, g_symbol_table[i].name, *val);
				*val=0;
			} else {
				unsigned long *val = g_symbol_table[i].address;
				ut_printf("clearing %d: %s -> %d\n", count, g_symbol_table[i].name, *val);
				*val=0;
			}
			count++;
		}
	}
	display_values(SYMBOL_STAT);
}
extern int g_conf_func_debug;
int ut_symbol_execute(int type, unsigned char *name, uint8_t *argv1, uint8_t *argv2) {
	int i, k, *confint;
	unsigned long *conflong;
	unsigned char new_name[200];
	int (*func)(char *argv1, char *argv2);

	if (argv1 ==0 && (type == SYMBOL_CONF || type == SYMBOL_STAT)){
		display_values(type);
		return 0;
	}
	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != type)
			continue;

		if (type == SYMBOL_CONF) {
			ut_snprintf(new_name, 200, "g_conf_%s", name);
			if (ut_strcmp((unsigned char *) g_symbol_table[i].name, (unsigned char *) new_name) != 0)
				continue;
			if (g_symbol_table[i].len == 4) {
				confint = (int *) g_symbol_table[i].address;
				*confint = (int) ut_atoi((unsigned char *) argv1, FORMAT_DECIMAL);
				ut_log("		Setting conf variable %s->:%d: (%s)  \n", g_symbol_table[i].name, *confint, argv1,
						g_symbol_table[i].address);
				ut_printf("		Setting conf variable %s->:%d: (%s)  \n", g_symbol_table[i].name, *confint, argv1,
						g_symbol_table[i].address);
			} else if (g_symbol_table[i].len == 8) {
				conflong = (unsigned long *) g_symbol_table[i].address;
				*conflong = (int) ut_atol((unsigned char *) argv1, FORMAT_DECIMAL);
				ut_log("		Setting conf variable %s->:%d: (%s)  \n", g_symbol_table[i].name, *conflong, argv1,
						g_symbol_table[i].address);
				ut_printf("		Setting conf variable %s->:%d: (%s)  \n", g_symbol_table[i].name, *conflong, argv1,
										g_symbol_table[i].address);
			} else if (g_symbol_table[i].len > 8) {
				ut_strcpy(g_symbol_table[i].address, argv1);
				ut_log("	Setting conf variable %s->:%s  \n", g_symbol_table[i].name, argv1, g_symbol_table[i].address);
			} else{
				ut_printf(" Error in setting the variable: %s\n",new_name);
			}

			return 1;
		} else {/*this is Jcmd_  leave 5 characters and match */
			if (ut_strcmp((unsigned char *) &g_symbol_table[i].name[5], (unsigned char *) name) != 0)
				continue;

			func = (void *) g_symbol_table[i].address;
			func(argv1, argv2);
			return 1;
		}
	}
	return ut_mod_symbol_execute(type, name, argv1, argv2);
}

unsigned long ut_get_symbol_addr(unsigned char *name) {
	int i;

	for (i = 0; i < g_total_symbols; i++) {
		if (ut_strcmp(name, g_symbol_table[i].name) == 0) {
			return g_symbol_table[i].address;
		}
	}
	return ut_mod_get_symbol_addr(name); /* search in the modules */
}
unsigned char *ut_get_symbol(addr_t addr) {
	int i;

	for (i = 0; i < g_total_symbols; i++) {
		if ((addr >= g_symbol_table[i].address) && (addr < g_symbol_table[i + 1].address)) {
			//ut_printf("   :%s + %x addr:%x i:%d\n",g_symbol_table[i].name,(addr-g_symbol_table[i].address),addr,i);
			return g_symbol_table[i].name;
		}
	}
	return 0;
}

void ut_printBackTrace(unsigned long *bt, unsigned long max_length) {
	int i;
	unsigned char *name;

	for (i = 0; i < max_length; i++) {
		if (bt[i] == 0)
			return;
		name = ut_get_symbol(bt[i]);
		ut_log(" %d :%s - %x\n", i + 1, name, bt[i]);
	}
	return;
}
void ut_storeBackTrace(unsigned long *bt, unsigned long max_length) {
	unsigned long *rbp;
	unsigned long lower_addr, upper_addr;
	int i;

	asm("movq %%rbp,%0" : "=m" (rbp));
	lower_addr = g_current_task;
	upper_addr = lower_addr + TASK_SIZE;
	for (i = 0; i < max_length; i++) {
		bt[i] = 0;
	}
	for (i = 0; i < max_length; i++) {
		bt[i] = *(rbp + 1);
		rbp = *rbp;
		if (rbp < lower_addr || rbp > upper_addr)
			break;
	}
	return;
}

void ut_getBackTrace(unsigned long *rbp, unsigned long task_addr, backtrace_t *bt) {
	int i;
	unsigned char *name;
	unsigned long lower_addr, upper_addr;

	if (rbp == 0) {
		asm("movq %%rbp,%0" : "=m" (rbp));
		lower_addr = g_current_task;
		upper_addr = lower_addr + TASK_SIZE;
	} else {
		lower_addr = task_addr;
		upper_addr = lower_addr + TASK_SIZE;
	}
	if (bt == 0) {
		struct task_struct *t = lower_addr;
		ut_log("taskname: %s  pid:%d\n", t->name, t->task_id);
	}
	for (i = 0; i < MAX_BACKTRACE_LENGTH; i++) {
		if (rbp < lower_addr || rbp > upper_addr)
			break;
		name = ut_get_symbol(*(rbp + 1));
		if (bt) {
			bt->entries[i].name = name;
			bt->entries[i].ret_addr = *(rbp + 1);
			bt->count = i + 1;
		} else {
			ut_log(" %d :%s - %x\n", i + 1, name, *(rbp + 1));
		}
		rbp = *rbp;
		if (rbp < lower_addr || rbp > upper_addr)
			break;
	}
	return;
}

void Jcmd_bt(unsigned char *arg1, unsigned char *arg2) {

	ut_getBackTrace(0, 0, 0);
}
/*****************************************Dwarf related code  ***********************************************/

static int dwarf_count = 0;
static struct dwarf_entry *dwarf_table;
static int dwarf_init_done = 0;
static void init_dwarf() {
	int ret, tret;
	unsigned char *p;

	dwarf_table = vmalloc(0x1f0000, 0);
	if (dwarf_table == 0) {
		ut_log(" init_dwarf: ERROR : fail allocated memory \n");
		return;
	}
	struct file *file = fs_open("dwarf_datatypes", 0, 0);
	if (file == 0) {
		ut_log(" init_dwarf: ERROR : fail to open the file \n");
	}
	p = (unsigned char *) dwarf_table;
	ret = 0;
	do {
		tret = fs_read(file, p, 0x200000 - ret);
		if (tret > 0) {
			p = p + tret;
			ret = ret + tret;
		}
		if (ret <= 0) {
			ut_log(" init_dwarf: ERROR : fail to read the file: %x \n", ret);
			break;
		}
	} while (tret > 0 && ((0x200000 - ret) > 0));

	fs_close(file);
	dwarf_count = ret / sizeof(struct dwarf_entry);
	if (dwarf_count < 0) {
		dwarf_count = 0;
	}
	ut_log("init_dwarf : read number of bytes :%d records:%d \n", ret, dwarf_count);
	if (ret > 0) {
		dwarf_init_done = 1;
	}
}

static int get_type(int j, int *size) {
	int i;

	i = dwarf_table[j].type_index;
	if (i == -1)
		i = j;
	if ((dwarf_table[i].tag == DW_TAG_structure_type) || (dwarf_table[i].tag == DW_TAG_base_type)
			|| (dwarf_table[i].tag == DW_TAG_pointer_type)) {
		*size = dwarf_table[i].size;
		return i;
	} else {
		i = dwarf_table[i].type_index;
		if (i == -1)
			return 0;
		if ((dwarf_table[i].tag == DW_TAG_structure_type) || (dwarf_table[i].tag == DW_TAG_base_type)
				|| (dwarf_table[i].tag == DW_TAG_pointer_type)) {
			*size = dwarf_table[i].size;
			return i;
		}
	}
	return 0;
}
static void print_data_structures(int i, unsigned long addr) {
	int j;
	if (i == 0 || i == -1)
		return;
	if (i >= dwarf_count) {
		ut_printf(" ERROR: Dwarf index :%d  max:%d\n", i, dwarf_count);
		return;
	}

	if (dwarf_table[i].tag == DW_TAG_variable) {
		int size;
		unsigned long *p = addr;
		int next_type = dwarf_table[i].type_index;
		ut_printf("%d variable:%s value: 0x%x(%d) next_type:%d\n", i, dwarf_table[i].name, p, p, next_type);
		if (next_type == 0 || next_type == -1)
			return;
		if (dwarf_table[next_type].tag == DW_TAG_pointer_type) {
			print_data_structures(next_type, *p);
		} else {
			print_data_structures(next_type, p);
		}
	} else if (dwarf_table[i].tag == DW_TAG_structure_type) {

		ut_printf(" %d Structure:%s len:%d index:%d addr:%x \n", i, dwarf_table[i].name, dwarf_table[i].size,
				dwarf_table[i].type_index, addr);
		for (j = i + 1; dwarf_table[j].level > 1; j++) {
			int size;
			int member = get_type(j, &size);
			//	ut_printf("%d Member:%s  index:%d  location:%d", j,dwarf_table[j].name,  dwarf_table[j].type_index, dwarf_table[j].member_location);
			if (member != 0) {
				unsigned char *p = addr + dwarf_table[j].member_location;
				if (size == 1) {
					ut_printf(" %s->%s(%x) size:%d(%s) \n", dwarf_table[j].name, p, p, size, dwarf_table[member].name);
				} else if (size == 8) {
					unsigned long *data = (unsigned long *) p;
					ut_printf(" %s->%x(%x) size:%d(%s) \n", dwarf_table[j].name, *data, data, size, dwarf_table[member].name);
				} else {
					ut_printf(" %s->STRUCT size:%d(%s) \n", dwarf_table[j].name, size, dwarf_table[member].name);
				}
			} else {
				ut_printf(" Unknown type \n");
			}
		}
	} else {
		//	ut_printf("%d Other:tag :%d: %s len:%d index:%d \n",i, dwarf_table[i].tag,dwarf_table[i].name, dwarf_table[i].size, dwarf_table[i].type_index);
		print_data_structures(dwarf_table[i].type_index, addr);
	}

}
void Jcmd_dwarf(unsigned char *arg1, unsigned char *arg2) {
	unsigned long addr;
	if (dwarf_init_done == 0) {
		init_dwarf();
	}
	if (dwarf_init_done == 0)
		return;
	if (arg1 == NULL)
		return;

	addr = ut_get_symbol_addr(arg1);
	if (addr == 0) {
		ut_printf(" Error: cannot find the symbol\n");
		return;
	}
	int i;
	for (i = 0; (i < dwarf_count) && (dwarf_table[i].tag == DW_TAG_variable); i++) {
		if (ut_strcmp(arg1, dwarf_table[i].name) == 0) {
			print_data_structures(i, addr);
			return;
		}
	}
	ut_printf(" Error: cannot find the dwarf info\n");
}
}
