/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/util.c
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

int init_symbol_table() {
	int i;
	int confs = 0;
	int stats = 0;
	int cmds = 0;

	for (i = 0; i < g_total_symbols; i++) {
		unsigned char sym[100], dst[100];

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[7] = '\0'; /* g_conf_ */
		ut_strcpy(dst, "g_conf_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_CONF;
			confs++;
			continue;
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[5] = '\0'; /* Jcmd_ */
		ut_strcpy(dst, "Jcmd_");
		if (ut_strcmp(sym, dst) == 0) {
			g_symbol_table[i].type = SYMBOL_CMD;
			cmds++;
			continue;
		}

		ut_strcpy(sym, g_symbol_table[i].name);
		sym[12] = '\0'; /* Jcmd_ */
		ut_strcpy(dst, "deviceClass_");
		if (ut_strcmp(sym, dst) == 0) {
            add_deviceClass(g_symbol_table[i].address);
			continue;
		}

	}
	ut_printf(
			"Symbol Intilization:  confs:%d stats:%d cmds:%d  totalsymbols:%d \n",
			confs, cmds, stats, g_total_symbols);
	return 1;
}

int display_symbols(int type){
	int i,count=0;
    int *conf;

	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != type) continue;
		conf=g_symbol_table[i].address;
		if (type==SYMBOL_CONF)
		    ut_printf("   %9s = %d\n",g_symbol_table[i].name,*conf);
		else
		    ut_printf("   %s: \n",&g_symbol_table[i].name[5]);
		count++;
	}
	return count;
}

int execute_symbol(int type, char *name, char *argv1,char *argv2){
    int i,*conf;
	int (*func)(char *argv1,char *argv2);

	for (i = 0; i < g_total_symbols; i++) {
		if (g_symbol_table[i].type != type) continue;

		if (type==SYMBOL_CONF){
			if (ut_strcmp(g_symbol_table[i].name, name) != 0) continue;
		    conf=g_symbol_table[i].address;
		    if (argv1==0) return 0;
		    *conf=ut_atoi(argv1);
		    return 1;
		}else {
			if (ut_strcmp(&g_symbol_table[i].name[5], name) != 0) continue;
			func=g_symbol_table[i].address;
			func(argv1,argv2);
			return 1;
		}
	}
	return 0;
}

void print_symbol(addr_t addr)
{
	int i;
	for (i=0; i< g_total_symbols; i++)
	{
		if ((addr>=g_symbol_table[i].address) && (addr<g_symbol_table[i+1].address))
		{
			ut_printf("   :%s + %x addr:%x i:%d\n",g_symbol_table[i].name,(addr-g_symbol_table[i].address),addr,i);
			return ;
		}
	}
	ut_printf("   :%x \n",addr);
	return;
}
