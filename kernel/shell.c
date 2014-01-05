/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/shell.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "common.h"
#include "device.h"
#include "task.h"
#include "vfs.h"
#include "interface.h"
#include "mach_dep.h"
#include "../test/expirements/vm_core_test.c"
typedef struct {
	char *usage;
	char *help;
	char *command_name;
	int (*func)(unsigned char *arg1, unsigned char *arg2);
} commands_t;
#define MAX_COMMANDS 500
static int sh_create(unsigned char *arg1, unsigned char *arg2);


static int print_help(unsigned char *arg1, unsigned char *arg2);
static int Jcmd_cat(unsigned char *arg1, unsigned char *arg2);
static int sh_cp(unsigned char *arg1, unsigned char *arg2);
static int sh_kill(unsigned char *arg1, unsigned char *arg2);
static int sh_alloc_mem(unsigned char *arg1, unsigned char *arg2);
static int sh_free_mem(unsigned char *arg1, unsigned char *arg2);
static int sh_sync(unsigned char *arg1, unsigned char *arg2);
static int sh_del(unsigned char *arg1, unsigned char *arg2);
static int sh_vmcore_test(unsigned char *arg1, unsigned char *arg2);

static int Jcmd_memleak(unsigned char *arg1, unsigned char *arg2);
static int sh_mmap(unsigned char *arg1, unsigned char *arg2);
static int sh_pci(unsigned char *arg1, unsigned char *arg2);

int conf_set(unsigned char *arg1, unsigned char *arg2);

static int debug_trace(unsigned char *arg1, unsigned char *arg2);

int g_conf_debug_level = 1;

int conf_set(unsigned char *arg1,unsigned char *arg2) {
	int ret;

	if (arg1 == 0) {
		ut_printf("Conf variables:\n");
		ut_symbol_show(SYMBOL_CONF);
	} else {
		ret = ut_symbol_execute(SYMBOL_CONF, arg1, arg2,0);
	}
	return ret;
}

int cmd(unsigned char *arg1,unsigned  char *arg2) {
	int ret;

	if (arg1 == 0) {
		ut_printf("Command list:\n");
		ret = ut_symbol_show(SYMBOL_CMD);
	} else {
		ret = ut_symbol_execute(SYMBOL_CMD, arg1, arg2,0);
	}
	if (ret == 0)
		ut_printf("Not Found: %s\n", arg1);
	return ret;
}
commands_t cmd_list[] = {

				 {
				0, 0, 0, cmd } /* at last check for command */
};



static unsigned char buf[26024];
static int sh_mmap(unsigned char *arg1,unsigned char *arg2) {  /* TODO the function broken if given empty args */
	struct file *fp;
	unsigned long addr;
	unsigned char c, *p;

	fp = fs_open(arg1, 0, 0);
	addr = ut_atol(arg2);

	ut_printf(" filename:%s: addr :%x: \n", arg1, addr);

	vm_mmap(fp, addr, 0, 0, 0, 0,"test_shell");
	p = (unsigned char *)addr;
	p = p + 10;
	c = *p;
	return 0;
}
static int sh_cp(unsigned char *arg1, unsigned char *arg2) {
	struct file *fp, *wfp;
	int i, ret, wret;
	fp = (struct file *)fs_open(arg1, 0, 0);
	wfp = (struct file *)fs_open(arg2, 1, 0);
	ut_printf("filename :%s: %s \n", arg1, arg2);
	if (fp == 0 || wfp == 0) {
		ut_printf(" Error opening file :%s: \n", arg1);
		return 0;
	}
	buf[1000] = 0;
	ret = 1;
	i = 1;
	while (ret > 0) {
		ret = fs_read(fp, (uint8_t *)buf, 5000);
		buf[5001] = '\0';
		if (ret > 0) {
			wret = fs_write(wfp, buf, ret);
			ut_printf("%d: DATA Read :%c: ret: %d wret:%d \n", i, buf[0], ret,
					wret);
		} else {
			ut_printf(" Return value of read :%i: \n", ret);
		}
		i++;
	}
	return 0;
}
static void ipc_thr(){
//	ipc_test1();
	SYS_sc_exit(22);
}
int start_debugtrace();
static int debug_trace(unsigned char *arg1, unsigned char *arg2) {
	int ret=4;
	int *p;
	p=5;
	*p=2;
	ret = sc_createKernelThread(ipc_thr, 0, (unsigned char *)"ipc_test");

	return 1;
}
/******************************************************************/
#if 0
#include "/opt_src/Jiny-Kernel/drivers/lwip/arch/sys_arch.h"
sys_mbox_t mbox;
int g_conf_start_test=0;
int total_messages = 1000000;
int prod_cpu=0;
int cons_cpu=0;
static void mutex_prod() {
	unsigned long ts;
	int i;
	while (g_conf_start_test == 0)
		;
	ts=g_jiffies;
	prod_cpu = g_current_task->allocated_cpu ;
	for (i = 1; i < total_messages; i++) {
		sys_mbox_post(&mbox, i + 100);
	}
	//ut_printf("Completed sending %d messages in %d \n",total_messages,(g_jiffies-ts));
	SYS_sc_exit(22);
}
static void mutex_cons(){
	unsigned long ts;
	int i;
	unsigned long msg;
	while(g_conf_start_test==0);

	ts=g_jiffies;
	prod_cpu = g_current_task->allocated_cpu ;
	for (i = 1; i < total_messages; i++) {
		while(sys_arch_mbox_fetch(&mbox, &msg,100)!=0);
	}
	ut_printf("Completed CONSUM %d messages in %d  cpus:%d:%d\n",total_messages,(g_jiffies-ts),prod_cpu,cons_cpu);
	SYS_sc_exit(22);
}
void Jcmd_mutextest(){
	int ret;
	static int init=0;
	if (init==0){
		sys_mbox_new(&mbox,2);
		init =1;
	}
	g_conf_start_test=0;
	ret = sc_createKernelThread(mutex_prod, 0, (unsigned char *)"mbox_prod");
	ret = sc_createKernelThread(mutex_cons, 0, (unsigned char *)"mbox_cons");

//start_test=1;
	return ;
}
#endif
/**********************************************************************/
static int sh_pci(unsigned char *arg1, unsigned char *arg2) {
	//device_t dev;
	//list_pci();

	init_devClasses();
    return 1;
}
static int sh_del(unsigned char *arg1, unsigned char *arg2) {
	int fd;

	fd = SYS_fs_open(arg1, 0, 0);
	if (fd == 0) {
		ut_printf(" Error opening file :%s: \n", arg1);
		return 0;
	}
	SYS_fs_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
	SYS_fs_close(fd);
	ut_printf("after del \n");
	return 0;
}
int i1 = 0;
void t11() {
	while (1)
		i1++;
}
int i2 = 0;
void t22() {
	while (1)
		i2++;
}
static unsigned long test_mp;
static int Jcmd_memleak(unsigned char *arg1, unsigned char *arg2) {
	unsigned long mp;

	test_mp =(unsigned long) mm_malloc(100, 0);
	mm_free(test_mp);

	test_mp = (unsigned long)mm_malloc(100, 0);
	ut_printf("alloced memory: %x\n", test_mp);
	mp = (unsigned long)mm_malloc(120, 0);
	ut_printf("alloced memory: %x mp_addr:%x \n", mp, &mp);
	mp = (unsigned long)mm_malloc(130, 0);
	ut_printf("alloced memory: %x\n", mp);

	kmemleak_scan();
	return 0;

}
static int sh_sync(unsigned char *arg1, unsigned char *arg2) {
	struct file *wfp;

	wfp = fs_open(arg1, 0, 0);
	if (wfp == 0) {
		ut_printf(" Error opening file :%s: \n", arg1);
		return 0;
	}
	ut_printf("Before syncing \n");
	fs_fdatasync(wfp);
	ut_printf("after syncing \n");
	return 0;
}
static int Jcmd_cat(unsigned char *arg1, unsigned char *arg2) {
	struct file *fp;
	int i, ret;

	if (arg1 == 0)
		return 0;
	fp = fs_open(arg1, 0, 0);
	ut_printf("filename :%s: \n", arg1);
	if (fp == 0) {
		ut_printf(" Error opening file :%s: \n", arg1);
		return 0;
	}
	buf[1000] = 0;
	ret = 1;
	i = 1;

	while (ret > 0) {
		ret = fs_read(fp, buf, 20000);
		buf[20001] = '\0';
		if (ret > 0) {
			buf[20] = '\0';
			ut_printf("%d: DATA Read  %d \n", i, ret);
		} else {
			ut_printf(" Return value of read :%i: \n", ret);
		}
		i++;
	}
	return 0;
}
static int sh_kill(unsigned char *arg1, unsigned char *arg2) {
	unsigned long pid;
	if (arg1 == 0)
		return 0;
	pid = ut_atol(arg1);
	ut_printf(" about to kill the process:%d \n", pid);
	SYS_sc_kill(pid, 9);
	return 1;
}
static int sh_alloc_mem(unsigned char *arg1, unsigned char *arg2) {
	unsigned long order;
	unsigned long addr;

	order = ut_atol(arg1);
	addr = mm_getFreePages(MEM_CLEAR, order);
	ut_printf(" calloc order:%x addr:%x \n", order, addr);
	return 1;
}
static int sh_free_mem(unsigned char *arg1, unsigned char *arg2) {
	unsigned long addr;
	unsigned long order;

	addr = ut_atol(arg1);
	order = ut_atol(arg2);
	mm_putFreePages(addr, order);
	ut_printf(" free addr  %s :%x  order:%x\n", arg1, addr, order);
	return 1;
}
static unsigned char *tmp_arg[100]; /* TODO :   need to find some permanent solution to pass the arguments */
static unsigned char *envs[] = { (unsigned char *)"HOSTNAME=jana", (unsigned char *)"USER=jana",
		(unsigned char *)"HOME=/", (unsigned char *)"PWD=/", 0 };
static int exec_thread(unsigned char *arg1, unsigned char *arg2) {
//	char *argv[]={"First argument","second argument",0};
//	char *argv[]={0};
	if (g_current_task->thread.argv == 0) {
		unsigned char *arg[5];
		arg[0] = arg1;
		arg[1] = arg2;
		arg[2] = 0;

		SYS_sc_execve(arg1, arg, envs);
	} else {
		char name[100];
		unsigned char *arg[5];

		ut_strcpy(name, g_current_task->thread.argv);
		arg[0] = g_current_task->thread.argv[0];
		arg[1] = g_current_task->thread.argv[1];
		arg[2] = g_current_task->thread.argv[2];
		arg[3] = 0;
		//	BRK;
		SYS_sc_execve(g_current_task->thread.argv[0], arg, envs);
	}
	ut_printf(" ERROR: COntrol Never Reaches\n");
	return 1;
}
static int sh_vmcore_test(unsigned char *arg1, unsigned char *arg2){
	int ret;
	tmp_arg[0] = arg1;
	tmp_arg[1] = arg2;
	tmp_arg[2] = 0;

	ret = sc_createKernelThread(generate_vm_core_thread, &tmp_arg, arg1);
	ret = sc_createKernelThread(memory_pollute_thread, &tmp_arg, arg1);
    return 1;
}

static int sh_create(unsigned char *arg1, unsigned char *arg2) {
	int ret;

	tmp_arg[0] = arg1;
	tmp_arg[1] = arg2;
	tmp_arg[2] = 0;
	ret = sc_createKernelThread(exec_thread, &tmp_arg, arg1);

//	sc_sleep(5000000);
	return ret;
}
static int print_help(unsigned char *arg1, unsigned char *arg2) {
	int i;
	ut_printf("JINY 1.0 Stacksize:%x  \n", TASK_SIZE);
	for (i = 0; i < MAX_COMMANDS; i++) {
		if (cmd_list[i].usage == 0)
			break;
		ut_printf("%9s %s \n", cmd_list[i].usage, cmd_list[i].help);
	}

	return 1;
}
/**************************************** shell logic ***********************/

#define MAX_LINE_LENGTH 200
#define CMD_PROMPT "->"
#define MAX_CMD_HISTORY 50
static unsigned char cmd_history[MAX_CMD_HISTORY][MAX_LINE_LENGTH];
static int curr_line_no = 0;
static int his_line_no = 0;
unsigned char curr_line[MAX_LINE_LENGTH];
enum {
	CMD_GETVAR = 1, CMD_FILLVAR, CMD_UPARROW, CMD_DOWNARROW, CMD_LEFTARROW
};

static void tokenise(char *p, char *tokens[]) {
	int i, k, j;

	i = 0;
	k = 1;
	j = 1;
	tokens[0] = p;
	while (p[i] != '\0') {
		if (p[i] == ' ') {
			p[i] = '\0';
			k = 0;
			i++;
			continue;
		}
		if (k == 0) {
			tokens[j] = p + i;
			j++;
			k = 1;
		}
		i++;
		if (j > 2)
			return;
	}
	return;
}
static int process_command(int cmd, unsigned char *p) {
	int i, ret, symbls;
	unsigned char *token[5];

	if (cmd == CMD_UPARROW) {
		his_line_no--;
		if (his_line_no < 0)
			his_line_no = MAX_CMD_HISTORY - 1;
		ut_strcpy(p, cmd_history[his_line_no]);
		ut_putchar((int) '\n');
		return 0;
	} else if (cmd == CMD_DOWNARROW) {
		his_line_no++;
		if (his_line_no >= MAX_CMD_HISTORY)
			his_line_no = 0;
		ut_strcpy(p, cmd_history[his_line_no]);
		ut_putchar((int) '\n');
		return 0;
	} else if (cmd == CMD_LEFTARROW) {
		curr_line[0] = '\0';
		ut_putchar((int) '\n');
		return 0;
	}
	if (p[0] != '\0') {
		ut_strcpy(cmd_history[curr_line_no], p);
		curr_line_no++;
		if (curr_line_no >= MAX_CMD_HISTORY)
			curr_line_no = 0;
		his_line_no = curr_line_no;
	}
	for (i = 0; i < 3; i++)
		token[i] = 0;
	tokenise(p, token);
	symbls = 0;
	for (i = 0; i < MAX_COMMANDS; i++) {
		if (cmd_list[i].usage == 0) {
			if (cmd_list[i].func != 0) {
				ret = cmd_list[i].func(token[0], token[1]);
				ret = 0;
			}
			break;
		}
		ret = ut_strcmp(token[0], cmd_list[i].command_name);
		if (ret == 0) {
			cmd_list[i].func(token[1], token[2]);
			break;
		} else if (ret == 2 && cmd == CMD_FILLVAR) /* if p is having some subsets in symbol table */
		{
			symbls++;
			if (symbls == 1)
				ut_printf("\n");
			ut_printf("%s ", cmd_list[i].command_name);
		}
	}
	if (cmd == CMD_FILLVAR && symbls > 0) {
		ut_printf("\n");
		return 1;
	} else if (ret != 0)
		ut_printf("Not found :%s: \n", p);
	p[0] = '\0';
	return 0;
}

static int get_cmd(unsigned char *line) {
	int i;
	int cmd;

	i = 0;
	cmd = CMD_FILLVAR;
	for (i = 0; line[i] != '\0' && i < MAX_LINE_LENGTH; i++) {
		ut_putchar((int) line[i]);
	}
	while (i < MAX_LINE_LENGTH) {
		int c;
		while ((line[i] = dr_kbGetchar(g_current_task->mm->fs.input_device)) == 0)
			;
		c = line[i];
		if (line[i] == 1) /* upArrow */
		{
			cmd = CMD_UPARROW;
			line[i] = '\0';
			break;
			//	ut_printf(" Upp Arrow \n");
		}
		if (line[i] == 2) /* upArrow */
		{
			cmd = CMD_DOWNARROW;
			line[i] = '\0';
			break;
			//	ut_printf(" DOWNArrow \n");
		}
		if (line[i] == 3) /* leftArrow */
		{
			cmd = CMD_LEFTARROW;
			line[i] = '\0';
			break;
			//	ut_printf(" LeftArrow \n");
		}
		if (line[i] == '\t') {
			line[i] = '\0';
			break;
		}
		if (line[i] == '\n' || line[i] == '\r') {
			cmd = CMD_GETVAR;
			ut_putchar((int) '\n');
			line[i] = '\0';
			break;
		}
		ut_putchar( line[i]);
		i++;
	}
	line[i] = '\0';
	return cmd;
}

#define USERLEVEL_SHELL "./busybox"
int shell_main(void *arg) {
	int i, cmd_type;
	void *ret=1;

//	ret = fs_open(USERLEVEL_SHELL,0,0);
	if (ret != 0){
//		fs_close(ret);
		ret = sh_create(USERLEVEL_SHELL, "sh");  // start the user level shell
	}else{
		/* attach kernel shell to serial line since user level shell fails */
		g_kernel_mm->fs.input_device = DEVICE_SERIAL;
		g_kernel_mm->fs.output_device = DEVICE_SERIAL;
	}
	ut_log(" user shell thread creation ret :%x\n",ret);
	ut_strncpy(g_current_task->name, "shell", MAX_TASK_NAME);
	for (i = 0; i < MAX_CMD_HISTORY; i++)
		cmd_history[i][0] = '\0';

	curr_line[0] = '\0';
	while (1) {
		ut_printf(CMD_PROMPT);
		cmd_type = get_cmd(curr_line);
		process_command(cmd_type, curr_line);
	}
	return 1;
}
