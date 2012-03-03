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
#include "task.h"
#include "vfs.h"
#include "interface.h"

typedef struct {
	char *usage;
	char *help;
	char *command_name;
	int (*func)(char *arg1,char *arg2);
} commands_t;
#define MAX_COMMANDS 500
static int sh_create(char *arg1,char *arg2);
static int sh_exit(char *arg1,char *arg2);
static int sh_syscalldebug(char *arg1,char *arg2);
static int sh_debug(char *arg1,char *arg2);
static int print_help(char *arg1,char *arg2);
static int sh_cat(char *arg1,char *arg2);
static int sh_cp(char *arg1,char *arg2);
static int sh_kill(char *arg1,char *arg2);
static int sh_alloc_mem(char *arg1,char *arg2);
static int sh_free_mem(char *arg1,char *arg2);
static int sh_sync(char *arg1,char *arg2);
static int sh_del(char *arg1,char *arg2);
int ar_printIrqStat(char *arg1,char *arg2);
static int  test_hostshm(char *arg1,char *arg2);
void ut_cls();

void test_proc();

int test_prod(char *arg1,char *arg2 );
int test_cons(char *arg1,char *arg2 );
static int sh_mmap(char *arg1,char *arg2);
extern int xen_time(char *arg1,char *arg2);
extern int xen_readcmd(char *arg1,char *arg2);
extern int xen_writecmd(char *arg1,char *arg2);
extern int p9_cmd(char *arg1,char *arg2);
extern int ut_logFlush(char *arg1, char *arg2);
int scan_pagecache(char *arg1 , char *arg2);
commands_t cmd_list[]=
{
	{"help      ","Print Help Menu","help",print_help},
	{"c <prog>  ","Create test thread","c",sh_create},
	{"e   ","exit thread","e",sh_exit},
	{"d   ","toggle debug","d",sh_debug},
	{"s   ","toggle SYSCALL debug","s",sh_syscalldebug},
	{"i         ","Print IRQ stats","i",ar_printIrqStat},
	{"t         ","Print thread list","t",sc_threadlist},
#ifdef XEN
	{"tp        ","test produce","tp",test_prod},
	{"tc        ","test consume","tc",test_cons},

	{"xw         ","xen write","xw",xen_writecmd},
	{"xr         ","xen read time","xr",xen_readcmd},
	{"x        ","Print time","x",xen_time},
#endif
	{"p9 ","9p commands","p9",p9_cmd},
	{"logflush ","Flush the log","logflush",ut_logFlush},
	{"kill <pid> ","kill process","kill",sh_kill},
	{"cls       ","clear screen ","cls",ut_cls},
	{"mp        ","Memory free areas","mp",mm_printFreeAreas},
	{"maps      ","Memory map areas","maps",vm_printMmaps},
	{"ls        ","ls","ls",fs_printInodes},
	{"pc        ","page cache stats","pc",pc_stats},
	{"scan        ","scan page cache ","scan",scan_pagecache},
	{"mem        ","memstat","mem",mm_printFreeAreas},
	{"mmap <file> <addr>","mmap file","mmap",sh_mmap},
	{"amem <order>","mem allocate ","amem",sh_alloc_mem},
	{"fmem <address>","mem allocate ","fmem",sh_free_mem},
	{"cat <file>","Cat file       ","cat",sh_cat},
	{"del <file>","flush file-remove from page cache       ","del",sh_del},
	{"cp <f1> <f2>","copy f1 f2       ","cp",sh_cp},
	{"sync <f1>","sync f1       ","sync",sh_sync},
	{0,0,0,0}
};

extern void ut_putchar (int c); 


extern struct wait_struct g_hfs_waitqueue;
#define CR0_WP 0x00010000 /* Write protect */
static inline uint64_t read_cr0(void)       
{                                             
	uint64_t ret;                               
	__asm__ volatile ("movq %%cr0, %0\n"   
			: "=r" (ret));            
	return ret;                                 
}

static inline void write_cr0(uint64_t val)  
{                                             
	__asm__ volatile ("movq %0, %%cr0 \n" 
			:: "r" (val));          
}
#define CR0_AM 0x00040000 /* Alignment mask */
#if 0
static int sh_test2(char *arg1,char *arg2)
{
	uint64_t *p,val;
	p=0x103000;
	val=0x281;
	ut_printf("Before writing table \n");
	*p=val;
	flush_tlb(0x101000);
	ut_printf("After writing table \n");
	return 1;
}
static int sh_test3(char *arg1,char *arg2)
{
	uint64_t val;
	/*	ut_printf(" Before wait hfs: %d \n",g_jiffies);
		sc_wait(&g_hfs_waitqueue,1000);
		ut_printf(" After wait hfs: %d \n",g_jiffies);*/
	ut_printf(" BEFefore making write protect \n");

	val = read_cr0();
	val &= ~CR0_AM; /* Disable alignment-check */
	/*
	 * Set write protect bit in order to protect
	 * write access to read-only pages from supervisor mode
	 */
	val |= CR0_WP;
	write_cr0(val);
	ut_printf(" after making write protect \n");
	return 1;
}
#endif

static char buf[6024];
static int sh_mmap(char *arg1,char *arg2)
{
	struct file *fp;
	unsigned long addr;
	unsigned char c,*p;	

	fp=fs_open(arg1,0,0);
	addr=ut_atol(arg2);

	ut_printf(" filename:%s: addr :%x: \n",arg1,addr);

	vm_mmap(fp,  addr, 0,0,0,0);
	p=addr;
	p=p+10;
	c=*p;	
	return 0;
}
static int sh_cp(char *arg1,char *arg2)
{
	struct file *fp,*wfp;
	int i,ret,wret;
	fp=fs_open(arg1,0,0);
	wfp=fs_open(arg2,1,0);
	ut_printf("filename :%s: %s \n",arg1,arg2);
	if (fp ==0 || wfp==0)
	{
		ut_printf(" Error opening file :%s: \n",arg1);
		return 0;
	}
	buf[1000]=0;
	ret=1;
	i=1;
	while (ret > 0)
	{
		ret=fs_read(fp,buf,5000);
		buf[5001]='\0';
		if (ret > 0)
		{
			wret=fs_write(wfp,buf,ret);
			ut_printf("%d: DATA Read :%c: ret: %d wret:%d \n",i,buf[0],ret,wret);
		}else
		{
			ut_printf(" Return value of read :%i: \n",ret);
		}
		i++;
	}
	return 0;
}
static int sh_del(char *arg1,char *arg2)
{
	struct file *wfp;
	int i,ret,wret;

	wfp=fs_open(arg1,0,0);
	if (wfp==0)
	{
		ut_printf(" Error opening file :%s: \n",arg1);
		return 0;
	}
	fs_fadvise(wfp,0,0,POSIX_FADV_DONTNEED);
	ut_printf("after del \n");
	return 0;
}
static int sh_sync(char *arg1,char *arg2)
{
	struct file *wfp;
	int i,ret,wret;

	wfp=fs_open(arg1,0,0);
	if (wfp==0)
	{
		ut_printf(" Error opening file :%s: \n",arg1);
		return 0;
	}
	ut_printf("Before syncing \n");
	fs_fdatasync(wfp);
	ut_printf("after syncing \n");
	return 0;
}
static int sh_cat(char *arg1,char *arg2)
{
	struct file *fp;
	int i,ret;
	fp=fs_open(arg1,0,0);
	ut_printf("filename :%s: \n",arg1);
	if (fp ==0)
	{
		ut_printf(" Error opening file :%s: \n",arg1);
		return 0;
	}
	buf[1000]=0;
	ret=1;
	i=1;
	while (ret > 0)
	{	
		ret=fs_read(fp,buf,5000);
		buf[5001]='\0';
		if (ret > 0)
		{
			buf[20]='\0';
			ut_printf("%d: DATA Read  %d \n",i,ret);
		}else
		{
			ut_printf(" Return value of read :%i: \n",ret);
		}
		i++;
	}
	return 0;
}
static int sh_kill(char *arg1,char *arg2)
{
	unsigned long pid;
	if (arg1 == 0) return 0;
	pid=ut_atol(arg1);
	ut_printf(" about to kill the process:%d \n",pid);
	SYS_sc_kill(pid,9);
	return 1;
}
static int sh_alloc_mem(char *arg1,char *arg2)
{
	unsigned long order;
	unsigned long addr;

	order=ut_atol(arg1);
	addr=mm_getFreePages(0,order);
	ut_printf(" alloc order:%x addr:%x \n",order,addr);
	return 1;	
}
static int sh_free_mem(char *arg1,char *arg2)
{
	unsigned long addr;
	unsigned long order;

	addr=ut_atol(arg1);
	order=ut_atol(arg2);
	mm_putFreePages(addr,order);
	ut_printf(" free addr  %s :%x  order:%x\n",arg1,addr,order);
	return 1;	
}

static int load_test1(char *arg1,char *arg2)
{
//	char *argv[]={"First argument","second argument",0};
	char *argv[]={0};
	if (g_current_task->thread.argv==0)
	{
		char *arg[5];
		arg[0]="test123";
		arg[1]=0;
        //	SYS_sc_execve("/home/njana/jiny/test/test3",argv,0);
        	SYS_sc_execve("test123",arg,0);
	}else
	{
        	SYS_sc_execve(g_current_task->thread.argv,argv,0);
	}
	ut_printf(" ERROR: COntrol Never Reaches\n");
	return 1;
}
static int load_test(char *arg1,char *arg2)
{
	sc_createKernelThread(load_test1,0,"load_test");
	return 1;
}
static int sh_create(char *arg1,char *arg2)
{
	int ret;

	ut_printf("test FORKING before \n"); 
	ret=sc_createKernelThread(load_test,arg1,"test");
	ut_printf(" Parent process : pid: %d  \n",ret);
	return 1;
}
unsigned long g_debug_level=1;
int g_test_exit=0;
static int sh_exit(char *arg1,char *arg2)
{
	if (g_test_exit == 1) g_test_exit=0;
	else g_test_exit=1;
	ut_printf(" Textexit :%d\n",g_test_exit);
	return 1;
}
static int sh_debug(char *arg1,char *arg2)
{
	if (g_debug_level == 1) g_debug_level=0;
	else g_debug_level=1;
	ut_printf(" debug level :%d\n",g_debug_level);
	return 1;
}
static int sh_syscalldebug(char *arg1,char *arg2)
{
	if (g_syscall_debug == 1) g_syscall_debug=0;
	else g_syscall_debug=1;
	ut_printf(" debug level :%d\n",g_syscall_debug);
	return 1;
}
static int print_help(char *arg1,char *arg2)
{
	int i;
	ut_printf("JINY 0.6 Stacksize:%x  \n",TASK_SIZE);
	for (i=0; i<MAX_COMMANDS; i++)
	{
		if (cmd_list[i].usage == 0) break;
		ut_printf(" %s %s \n",cmd_list[i].usage,cmd_list[i].help);
	}	

	return 1;
}
/**************************************** shell logic ***********************/

#define MAX_LINE_LENGTH 200
#define CMD_PROMPT "->"
#define MAX_CMD_HISTORY 50
static char cmd_history[MAX_CMD_HISTORY][MAX_LINE_LENGTH];
static int curr_line_no=0;
static int his_line_no=0;
unsigned char curr_line[MAX_LINE_LENGTH];
enum{
	CMD_GETVAR=1,
	CMD_FILLVAR,
	CMD_UPARROW,
	CMD_DOWNARROW,
	CMD_LEFTARROW
};

static void tokenise( char *p,char *tokens[])
{
	int i,k,j;

	i=0;
	k=1;
	j=1;
	tokens[0]=p;
	while (p[i] != '\0')
	{
		if (p[i]==' ') 
		{
			p[i]='\0';
			k=0;
			i++;
			continue;
		}
		if (k==0)
		{
			tokens[j]=p+i;
			j++;
			k=1;
		}
		i++;	
		if (j>2) return;
	}
	return;
}
static int process_command(int cmd,char *p)
{
	int i,ret,symbls;
	char *token[5];

	if (cmd == CMD_UPARROW)
	{
		his_line_no--;
		if (his_line_no < 0) his_line_no=MAX_CMD_HISTORY-1;
		ut_strcpy(p,cmd_history[his_line_no]);
		ut_putchar((int)'\n');
		return 0;	
	}
	else if (cmd == CMD_DOWNARROW)
	{
		his_line_no++;
		if (his_line_no >= MAX_CMD_HISTORY) his_line_no=0;
		ut_strcpy(p,cmd_history[his_line_no]);
		ut_putchar((int)'\n');
		return 0;	
	}
	else if (cmd == CMD_LEFTARROW)
	{
		curr_line[0]='\0';
		ut_putchar((int)'\n');
		return 0;	
	}
	if (p[0] != '\0')
	{
		ut_strcpy(cmd_history[curr_line_no],p);
		curr_line_no++;
		if (curr_line_no >= MAX_CMD_HISTORY ) curr_line_no=0;
		his_line_no=curr_line_no;
	}
	for (i=0; i<3; i++) token[i]=0;
	tokenise(p,token);
	symbls=0;
	for (i=0; i<MAX_COMMANDS; i++)
	{
		if (cmd_list[i].usage == 0) break;
		ret=ut_strcmp(token[0],cmd_list[i].command_name);
		if (ret==0)
		{
			cmd_list[i].func(token[1],token[2]);		
			break;
		}else if (ret==2 && cmd==CMD_FILLVAR) /* if p is having some subsets in symbol table */
		{
			symbls++;
			if (symbls==1) ut_printf("\n");
			ut_printf("%s ",cmd_list[i].command_name);	
		}
	}
	if (cmd==CMD_FILLVAR && symbls>0)
	{
		ut_printf("\n");
		return 1;
	}else if (ret != 0)
		ut_printf("Not found :%s: \n",p);
	p[0]='\0';		
	return 0;
}
#if 0
static int process_symbol(int cmd,char *p)
{
	int i,ret,symbls;

	symbls=0;
	for (i=0; i< g_total_symbols; i++)
	{
		ret=ut_strcmp(p,g_symbol_table[i].name);
		if (ret==0)
		{
			ut_printf(" %s : %x  type:%d \n",p,g_symbol_table[i].address,g_symbol_table[i].type);
			return 0;
		}else if (ret==2 && cmd==CMD_FILLVAR) /* if p is having some subsets in symbol table */
		{
			symbls++;
			if (symbls==1) ut_printf("\n");
			ut_printf("%s ",g_symbol_table[i].name);	
		}
	}
	if (cmd==CMD_FILLVAR && symbls>0)
	{
		ut_printf("\n");		
		return 1;	
	}else
		ut_printf("Not found :%s: \n",p);
	return 0;
}
#endif
static int get_cmd(unsigned char *line)
{
	int i;
	int cmd;

	i=0;
	cmd=CMD_FILLVAR;
	for (i=0; line[i]!='\0' && i<MAX_LINE_LENGTH; i++)
	{
		ut_putchar((int)line[i]);
	}
	while (i<MAX_LINE_LENGTH)
	{ 
		int c;
		while((line[i]=dr_kbGetchar())==0);
		c=line[i];
		if (line[i]==1) /* upArrow */
		{
			cmd=CMD_UPARROW;
			line[i]='\0';
			break;
			//	ut_printf(" Upp Arrow \n");
		}
		if (line[i]==2) /* upArrow */
		{
			cmd=CMD_DOWNARROW;
			line[i]='\0';
			break;
			//	ut_printf(" DOWNArrow \n");
		}
		if (line[i]==3) /* leftArrow */
		{
			cmd=CMD_LEFTARROW;
			line[i]='\0';
			break;
			//	ut_printf(" LeftArrow \n");
		}
		if (line[i]=='\t')
		{ 
			line[i]='\0';
			break;
		}
		if (line[i]=='\n' || line[i]=='\r' )
		{
			cmd=CMD_GETVAR;
			ut_putchar((int)'\n');
			line[i]='\0';
			break;
		}
		ut_putchar((int)line[i]);
		i++;
	} 
	line[i]='\0';
	return cmd;
}

int shell_main()
{
	int i,cmd_type;

	ut_strncpy(g_current_task->name,"shell",MAX_TASK_NAME);
	for (i=0; i<MAX_CMD_HISTORY;i++)
		cmd_history[i][0]='\0';

	curr_line[0]='\0';
	while(1)
	{
		ut_printf(CMD_PROMPT);
		cmd_type=get_cmd(curr_line);
		process_command(cmd_type,curr_line);
	}
	return 1;
}
