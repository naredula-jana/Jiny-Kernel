#include "common.h"
#include "task.h"
#include "vfs.h"

typedef struct {
	unsigned char *usage;
	unsigned char *help;
	unsigned char *command_name;
	void (*func)(addr_t arg1,addr_t arg2);
} commands_t;
#define MAX_COMMANDS 500
static int sh_create(char *arg1,char *arg2);
static int print_help(char *arg1,char *arg2);
static int sh_cat(char *arg1,char *arg2);
void ar_printIrqStat(char *arg1,char *arg2);
static int  test_hostshm(char *arg1,char *arg2);
void mm_printFreeAreas(void);
void vm_printMmaps();
void ut_cls();

void test_proc();
static int sh_test1(char *arg1,char *arg2);
static int sh_test2(char *arg1,char *arg2);
commands_t cmd_list[]=
{
	{"help      ","Print Help Menu","help",print_help},
	{"c <arg1>  ","Create arg1 number of threads","c",sh_create},
	{"i         ","Print IRQ stats","i",ar_printIrqStat},
	{"cls       ","clear screen ","cls",ut_cls},
	{"mp        ","Memory free areas","mp",mm_printFreeAreas},
	{"test1     ","test1 ","test1",sh_test1},
	{"test2     ","test2 ","test2",sh_test2},
	{"maps      ","Memory map areas","maps",vm_printMmaps},
	{"host      ","host shm test","host",test_hostshm},
	{"cat <file>","Cat file       ","cat",sh_cat},
	{0,0,0,0}
};

extern void ut_putchar (int c); 
static int sh_cat(char *arg1,char *arg2);
static int test_hostshm(char *arg1,char *arg2)
{
	unsigned int *a;
	unsigned int v,av;
	a=ut_atol(arg1);
	av=ut_atoi(arg2);

	ut_printf(" arg1: %x :%x \n",a,v);
	a=(unsigned char *)HOST_SHM_CTL_ADDR+8;
	v=*a;
	ut_printf(" hostshm : %x :%x \n",a,v);
/*	a=(unsigned char *)HOST_SHM_CTL_ADDR;
	*a=0xffffffff;*/ /* set the proper mask */
	a=(unsigned char *)HOST_SHM_CTL_ADDR+12;
	*a=av;
	ut_printf(" new  hostshm : %x :%x \n",a,av);
	return 1;
}
static int sh_test1(char *arg1,char *arg2)
{
	ut_printf(" Before wait sleep: %d \n",g_jiffies);
	sc_sleep(1000);
	ut_printf(" After wait sleep: %d \n",g_jiffies);
	return 1;
}
extern struct wait_struct g_hfs_waitqueue;
static int sh_test2(char *arg1,char *arg2)
{
	ut_printf(" Before wait hfs: %d \n",g_jiffies);
	sc_wait(&g_hfs_waitqueue,1000);
	ut_printf(" After wait hfs: %d \n",g_jiffies);
	return 1;
}
static unsigned char buf[1024];
static int sh_cat(char *arg1,char *arg2)
{
//	unsigned char buf[1024];
	struct file *fp;
	int ret;
	fp=fs_open(arg1);
	ut_printf("filename :%s: \n",arg1);
	if (fp ==0)
	{
		ut_printf(" Error opening file :%s: \n",arg1);
		return 0;
	}
	buf[1000]=0;
	ret=fs_read(fp,buf,500);
	if (ret > 0 && ret < 501) buf[ret]='\0';
	buf[500]='\0';
	if (ret > 0)
	{
		ut_printf(" Data Read :%s:\n",buf);
	}else
	{
		ut_printf(" Return value of read :%i: \n",ret);
	}
	return 0;
}
static int sh_create(char *arg1,char *arg2)
{
	ut_printf(" arg1: %s arg2: %s: \n",arg1,arg2);
	ut_printf("FORKING \n");  sc_createThread(test_proc);
	return 1;
}
static int print_help(char *arg1,char *arg2)
{
	int i;
	ut_printf("Version 1.62 stacksize:%x  \n",STACK_SIZE);
	for (i=0; i<MAX_COMMANDS; i++)
	{
		if (cmd_list[i].usage == 0) break;
		ut_printf(" %s %s \n",cmd_list[i].usage,cmd_list[i].help);
	}	
	//dr_serialWrite("TEST\n",5);
	return 1;
}

addr_t g_debug_level=1;
int g_test_exit=0;
#define MAX_LINE_LENGTH 200
#define CMD_PROMPT "->"

enum{
	CMD_GETVAR=1,
	CMD_FILLVAR
};
void tokenise(char *p,char *tokens[])
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
	unsigned char *token[5];

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
	return 0;
}
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
static int get_cmd(char *line)
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
		while((line[i]=dr_kbGetchar())==0);
		if (line[i]=='\t')
		{ 
			line[i]='\0';
			break;
		}
		if (line[i]=='\n' )
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
static int div_by_zero()
{
	int i,k;
	k=0;
	i=32/k;
}
int shell_main()
{
	char c,line[MAX_LINE_LENGTH];
	int i,cmd_type;

	int pos=0;
	line[0]='\0';
	while(1)
	{
		ut_printf(CMD_PROMPT);
		cmd_type=get_cmd(line);
		if (process_command(cmd_type,line)==0)
		{
			line[0]='\0';
		}
#if 0
		switch(line[0])
		{
			case 'a' : vm_brk(g_current_task->mm->start_brk,0x8000);
				   g_current_task->mm->start_brk=g_current_task->mm->start_brk+0x8000;
				   vm_printMmaps(); break;
			case 'b' : 
				   {
					   addr_t *p;
					   p=0xc0000000;
					   *p=123;
					   break;
				   }
			case 'f' : 
				   {
					   addr_t *p;
					   p=0xd0000000;
					   *p=0xabc;
					   break;
				   }
			case 'h' : print_help(); break;
			case 'p' : mm_printFreeAreas() ; break;
			case 'd' : if (g_debug_level == 0) g_debug_level=1;
					   else g_debug_level =0; 
					   ut_printf(" New g_debug_level : %d \n",g_debug_level);
					   break;
			case 'x' :
					   {
						   unsigned long *addr=mm_malloc(1123,0);
						   ut_printf("kmalloc  addr :%x \n",addr);
						   mm_free(addr);
						   ut_printf("free  addr :%x \n",addr);
						   break;
					   }
			case 'c' : ut_printf("FORKING \n");  sc_createThread(test_proc);   break; 
			case 'e' : if (g_test_exit == 0) g_test_exit=1;
					   else g_test_exit=0;
					   ut_printf(" New g_test_exit: %d \n",g_test_exit); break;
			case 'm' : 
					   for (i=0; i<30; i++)
					   {
						   ut_printf(CMD_PROMPT);
						   ut_printf("mass FORKING \n");  sc_createThread(test_proc); 
					   }
					   break; 
			case 's' : 
					   ut_printf("SCHEDULE \n");sc_schedule(); 
					   ut_printf("shell Proc p:%d laddr:%x c:%x  sp:%x slen:%d \n",g_current_task->pid,g_current_task,&c,g_current_task->thread.sp,((void *)&c-(void*)g_current_task));
					   break; 
			case 'i' : ar_printIrqStat(); break;
			case 'z' : div_by_zero(); break;
			default: ut_printf("UNSUpported   \n"); break;
		}
		line[0]='\0';
#endif
	}
}
