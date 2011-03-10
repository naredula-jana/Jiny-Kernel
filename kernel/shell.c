#include "common.h"
#include "task.h"
#include "vfs.h"

typedef struct {
	unsigned char *usage;
	unsigned char *help;
	unsigned char *command_name;
	void (*func)(char *arg1,char *arg2);
} commands_t;
#define MAX_COMMANDS 500
static int sh_create(char *arg1,char *arg2);
static int print_help(char *arg1,char *arg2);
static int sh_cat(char *arg1,char *arg2);
static int sh_cp(char *arg1,char *arg2);
static int sh_sync(char *arg1,char *arg2);
static int sh_pc(char *arg1,char *arg2);
static int sh_ls(char *arg1,char *arg2);
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
	{"ls        ","ls","ls",sh_ls},
	{"pc        ","page cache stats","pc",sh_pc},
	{"cat <file>","Cat file       ","cat",sh_cat},
	{"cp <f1> <f2>","copy f1 f2       ","cp",sh_cp},
	{"sync <f1>","sync f1       ","sync",sh_sync},
	{0,0,0,0}
};

extern void ut_putchar (int c); 
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
	unsigned char *p;

	p=0x104f20;
	ut_printf(" Before Writing in to this Adress %x %x \n",p,*p);
	(*p)=12;	
	ut_printf(" After Writing in to this Adress %x  %x\n",p,*p);
	return 1;
}
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
static int sh_test2(char *arg1,char *arg2)
{
	uint64_t *p,val;
	p=0x103000;
	val=0x281;
	ut_printf("Before writing table \n");
	*p=val;
	flush_tlb();
	ut_printf("After writing table \n");
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
	 * write access to read-only pages from supervisor mode.
	 */
	val |= CR0_WP;
	write_cr0(val);
	ut_printf(" after making write protect \n");
	return 1;
}

static char buf[6024];
static int sh_cp(char *arg1,char *arg2)
{
        //      unsigned char buf[1024];
        struct file *fp,*wfp;
        int i,ret,wret;
        fp=fs_open(arg1,0);
        wfp=fs_open(arg2,1);
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
static int sh_sync(char *arg1,char *arg2)
{
        //      unsigned char buf[1024];
        struct file *wfp;
        int i,ret,wret;

        wfp=fs_open(arg1,0);
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
	//	unsigned char buf[1024];
	struct file *fp;
	int i,ret;
	fp=fs_open(arg1,0);
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
			ut_printf("%d: DATA Read sr :%s: %d \n",i,buf,ret);
		}else
		{
			ut_printf(" Return value of read :%i: \n",ret);
		}
		i++;
	}
	return 0;
}
static int sh_pc(char *arg1,char *arg2)
{
	pc_stats();
	return 1;
}
static int sh_ls(char *arg1,char *arg2)
{
	fs_printInodes();
	return 1;
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
	ut_printf("Version 1.70 stacksize:%x  \n",STACK_SIZE);
	for (i=0; i<MAX_COMMANDS; i++)
	{
		if (cmd_list[i].usage == 0) break;
		ut_printf(" %s %s \n",cmd_list[i].usage,cmd_list[i].help);
	}	

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
void tokenise( char *p,char *tokens[])
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
	unsigned char c,line[MAX_LINE_LENGTH];
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
	}
}
