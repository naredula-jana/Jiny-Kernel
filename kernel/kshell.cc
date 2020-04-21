/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kshell.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/

extern "C" {
#include "common.h"
}
#include "jdevice.h"
#define MAX_LINE_LENGTH 200
#define CMD_PROMPT "-->"
#define MAX_CMD_HISTORY 50
#define MAX_COMMANDS 500

class kshell {
	unsigned char cmd_history[MAX_CMD_HISTORY][MAX_LINE_LENGTH];
	int curr_line_no;
	int his_line_no;
	unsigned char curr_line[MAX_LINE_LENGTH];

	int tokenise(unsigned char *p, unsigned char *tokens[]);
	int get_cmd(unsigned char *line);
	int process_command(int cmd, unsigned char *p);
	int cmd_func(unsigned char *arg1, unsigned char *arg2, unsigned char *arg3);

public:
	int input_device;
	void kshell_process();
	void execute_startupfile();
	int main(void *arg);
};
kshell console_ksh; /* kernel shellon console */
kshell serial_ksh; /* kernel shell on serial line , incase file system not present  */

enum {
	CMD_GETVAR = 1, CMD_FILLVAR, CMD_UPARROW, CMD_DOWNARROW, CMD_LEFTARROW
};
int putchar(unsigned char c){
	unsigned char buf[10];
	buf[0]=c;
	return fs_fd_write(1,buf,1);
}
int kshell::tokenise(unsigned char *p, unsigned char *tokens[]) {
	int i, k, count;

	i = 0;
	k = 1;
	count = 1;
	tokens[0] = p;
	while (p[i] != '\0') {
		if (p[i] == ' ') {
			p[i] = '\0';
			k = 0;
			i++;
			continue;
		}
		if (k == 0) {
			tokens[count] = p + i;
			count++;
			k = 1;
		}
		i++;
		if (count > 2)
			return count;
	}
	return count;
}
int kshell::cmd_func(unsigned char *arg1, unsigned char *arg2, unsigned char *arg3) {
	int ret;
#if 1
	if (arg1 == 0) {
		ut_printf("Command list:\n");
		ret = ut_symbol_show(SYMBOL_CMD);
	} else {
		ret = ut_symbol_execute(SYMBOL_CMD, (char *) arg1, arg2, arg3);
	}
	if (ret == 0)
		ut_printf("Not Found: %s\n", arg1);
#endif
	return ret;
}

int kshell::process_command(int cmd, unsigned char *p) {
	int i, symbls;
	unsigned char *token[5];

	if (cmd == CMD_UPARROW) {
		his_line_no--;
		if (his_line_no < 0)
			his_line_no = MAX_CMD_HISTORY - 1;
		ut_strcpy(p, cmd_history[his_line_no]);
		putchar((int) '\n');
		return 0;
	} else if (cmd == CMD_DOWNARROW) {
		his_line_no++;
		if (his_line_no >= MAX_CMD_HISTORY)
			his_line_no = 0;
		ut_strcpy(p, cmd_history[his_line_no]);
		putchar((int) '\n');
		return 0;
	} else if (cmd == CMD_LEFTARROW) {
		curr_line[0] = '\0';
		putchar((int) '\n');
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
	symbls = tokenise(p, token);

	cmd_func(token[0], token[1], token[2]);
	if (cmd == CMD_FILLVAR && symbls > 0) {
		ut_printf("\n");
		return 1;
	}
	p[0] = '\0';
	return 0;
}

int kshell::get_cmd(unsigned char *line) {
	int i;
	int cmd;

	i = 0;
	cmd = CMD_FILLVAR;
	for (i = 0; line[i] != '\0' && i < MAX_LINE_LENGTH; i++) {
//		putchar((int) line[i]);
	}
	while (i < MAX_LINE_LENGTH) {
		int c;

		SYS_fs_read(0,&line[i],1);

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
			putchar((int) '\n');
			line[i] = '\0';
			break;
		}
		i++;
	}
	line[i] = '\0';
	return cmd;
}
int g_conf_only_kshell = 0;
unsigned char *envs[] = { (unsigned char *) "HOSTNAME=jana",
		(unsigned char *) "USER=jana", (unsigned char *) "HOME=/",
		(unsigned char *) "PWD=/", 0 };
/* this is for serial line, if file system is present then it loads busybox otherwise kshell */
static int thread_launch_serial(void *arg1, void *arg2) {
	void **argv = sc_get_thread_argv();

	if (g_conf_only_kshell == 0) {
		if (argv == 0) {
			BUG();
		} else {
			void *arg[5];
			//ut_strcpy(name, (const unsigned char *)argv[0]);
			arg[0] = argv[0];
			arg[1] = argv[1];
			arg[2] = argv[2];
			arg[3] = 0;
			//serial_ksh.input_device = argv[3];
			SYS_sc_execve((unsigned char *) argv[0], (unsigned char **) arg, envs);
		}
		ut_printf(" Error: User Space shell(%s) not found, fallback to kernel shell\n",arg1);
	}
	serial_ksh.input_device = argv[3];
	serial_ksh.kshell_process();
	return 1;
}
void *tmp_arg[5];
static int sh_create(unsigned char *bin_file, unsigned char *name, unsigned char *arg, int serial_dev) {
	int ret;

	tmp_arg[0] =(void *) bin_file;
	tmp_arg[1] =(void *) name;
	tmp_arg[2] =(void *) arg;
	tmp_arg[3] =(void *) serial_dev;
	sc_set_fsdevice(serial_dev, serial_dev);  /* all user level thread on serial line */
	//BRK;
	ret = sc_createKernelThread(thread_launch_serial, (void **) &tmp_arg,name,0);

	return ret;
}
extern "C" {
int g_conf_hp_mode=0;
void Jcmd_jdevices(unsigned char *arg1,unsigned char *arg2);
void Jcmd_cpu(unsigned char *arg1,unsigned char *arg2);
void enable_ext_interrupt();
//void enable_avx();

}
#define MAX_START_FILE 3096
static unsigned char startup_buf[MAX_START_FILE+1];
void kshell::execute_startupfile(){
	int i;
	struct file *file = 0;
	file = (struct file*) fs_open("/data/start", 0, 0);
	if (file == 0) {
		ut_printf("failed to open startup file\n");
	} else {
		ut_printf("started executing the start file:\n");
		fs_lseek(file, 0, 0);
		int file_size = fs_read(file, (unsigned char*) startup_buf,
				MAX_START_FILE);
		int start;
		start = 0;

		for (i = 0; i < file_size; i++) {
			if (startup_buf[i] == '\n') {
				startup_buf[i] = 0;
				//ut_printf(" executing command  :%s:\n ", &startup_buf[start]);
				process_command(CMD_GETVAR, &startup_buf[start]);
				start = i + 1;
			}
		}

	}
}
void kshell::kshell_process(){
	int  cmd_type;

	curr_line[0] = '\0';
//	enable_avx();
	//ut_printf(" JINY OS .. STARTED with startup script .....\n");

	while (1) {
		ut_printf(CMD_PROMPT);
		cmd_type = get_cmd(curr_line);
	//	ut_printf("executing the cmd :%s: \n",curr_line);
		process_command(cmd_type, curr_line);
	}
}
#define USERLEVEL_SHELL "/busybox"

extern struct jdevice *serial2_device;
//#define USERLEVEL_SHELL "/jiny_root/busybox"
int kshell::main(void *arg) {
	int i, cmd_type;
	int ret = 0;

	sc_set_fsdevice(DEVICE_SERIAL1, DEVICE_SERIAL1); /* kshell on vga console */
	input_device = DEVICE_SERIAL1;

	execute_startupfile();
	if (g_conf_hp_mode == 0) {
		ut_log("   loading the kernel shell :%s:\n", USERLEVEL_SHELL);
		ret = sh_create((unsigned char *) USERLEVEL_SHELL,(unsigned char *) "sh", 0, DEVICE_SERIAL1); // start the user level shell
#if 1
		if (serial2_device != 0 && serial2_device->driver != 0) {
			sc_sleep(2000);
			ret = sh_create((unsigned char *) USERLEVEL_SHELL,(unsigned char *) "sh", 0, DEVICE_SERIAL2);
		}
#endif
		sc_set_fsdevice(DEVICE_KEYBOARD, DEVICE_SERIAL2); /* kshell on vga console */
		input_device = DEVICE_KEYBOARD;
	}else{
		sc_set_fsdevice(DEVICE_SERIAL1, DEVICE_SERIAL1); /* kshell on vga console */
		input_device = DEVICE_SERIAL1;
	}

	ut_log(" user shell thread creation ret :%x\n", ret);
	//ut_strncpy(g_current_task->name, "shell", MAX_TASK_NAME);
	for (i = 0; i < MAX_CMD_HISTORY; i++)
		cmd_history[i][0] = '\0';

	kshell_process();

	return 1;
}
extern "C" {
unsigned char test_tcp_data[4096];
void kernel_web_server(void *arg1, void *arg2){
	struct sockaddr addr,raddr;
	int ret;

	int fd = SYS_socket(2,1,0);
	addr.addr =0;
	addr.sin_port = 0x5000;
	if (fd < 0){
		return ;
	}
	SYS_bind(fd,&addr,0);
	while(1){
		int nfd = SYS_accept(fd,0,0);
		while (nfd != 0){
			ret = SYS_fs_read(nfd,test_tcp_data,4096);
			if (ret > 0){
				test_tcp_data[ret]=0;
				SYS_fs_write(nfd,test_tcp_data,ret);
			}
			ut_log("read data :%d data:%s\n",ret,test_tcp_data);
			//SYS_fs_write(nfd,test_tcp_data,ret);
		}
		sc_sleep(1);
	}
}
void shell_main(){
	//sc_createKernelThread(kernel_web_server, 0 ,"kernel_ws",0);
	console_ksh.main(0);
}
void Jcmd_help(){
	ut_printf("Conf variables:\n");
	ut_symbol_show(SYMBOL_CONF);
	ut_printf("Cmd variables:\n");
	ut_symbol_show(SYMBOL_CMD);
}
void Jcmd_set(unsigned char *variable, unsigned char *value){
	ut_printf("setting conf variable: %s -> %s \n",variable,value);
	ut_symbol_execute(SYMBOL_CONF, variable, value,0);
}
void Jcmd_run(unsigned char *command, unsigned char *options){
	ut_printf("Executing command: %s -> %s \n",command,options);

	sh_create((unsigned char *) command,(unsigned char *) options, 0, DEVICE_SERIAL1);
}
}

