#include "common.h"
#include "interface.h"
#include "isr.h"

unsigned long SYS_printf(unsigned long *args);
unsigned long SYS_exit(unsigned long args);
unsigned long SYS_fork();
int g_syscall_debug=1;
long SYS_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags,unsigned long fd, unsigned long off);
unsigned long snull(unsigned long *args);
unsigned long SYS_uname(unsigned long *args);

typedef struct {
        unsigned long (*func)(unsigned long *args);
} syscalltable_t;

syscalltable_t syscalltable[]=
{
	{SYS_fs_read} ,
	{SYS_fs_write}, /* 1  */
	{SYS_fs_open},
	{SYS_fs_close}, 
	{snull},
	{snull}, /* 5 */
	{snull},
	{snull},
	{snull}, 
	{SYS_vm_mmap}, 
	{SYS_vm_mprotect},/* 10 */
	{SYS_vm_munmap},
	{SYS_vm_brk},
	{snull}, 
	{snull},
	{snull}, /* 15 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 20 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 25 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 30 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 35 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 40 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 45 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 50 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 55 */
	{SYS_sc_clone},
	{SYS_sc_fork},
	{snull}, 
	{SYS_sc_execve}, 
	{SYS_sc_exit}, /* 60 */
	{snull},
	{SYS_sc_kill},
	{SYS_uname}, 
	{snull}, 
	{snull}, /* 65 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 70 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{SYS_fs_fdatasync}, /* 75 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 80 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 85 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 90 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 95 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 100 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 105 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 110 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 115 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 120 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 125 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 130 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull}, /* 135 */
	{snull},
	{snull},
	{snull}, 
	{snull}, 
	{snull} 
};


#define UTSNAME_LENGTH	65 
/* Structure describing the system and machine.  */
struct utsname
  {
    /* Name of the implementation of the operating system.  */
    char sysname[UTSNAME_LENGTH];
    /* Name of this node on the network.  */
    char nodename[UTSNAME_LENGTH];
    /* Current release level of this implementation.  */
    char release[UTSNAME_LENGTH];
    /* Current version level of this release.  */
    char version[UTSNAME_LENGTH];
    /* Name of the hardware type the system is running on.  */
    char machine[UTSNAME_LENGTH];
  };

struct utsname g_utsname;
//uname({sysname="Linux", nodename="njana-desk", release="2.6.35-22-generic", version="#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010", machine="x86_64"}) = 0
static int init_utsname()
{
	ut_strcpy(g_utsname.sysname,"Linux");	
	ut_strcpy(g_utsname.nodename,"njana-desk");	
	ut_strcpy(g_utsname.release,"2.6.35-22-generic");	
	ut_strcpy(g_utsname.version,"#33-Ubuntu SMP Sun Sep 19 20:32:27 UTC 2010");	
	ut_strcpy(g_utsname.machine,"x86_64");	
}
static int init_uts_done=0;
	
unsigned long SYS_uname(unsigned long *args)
{
	SYS_DEBUG("uname args:%x \n",args);
	if (init_uts_done==0) init_utsname();
	ut_printf(" Inside uname : %s \n",g_utsname.sysname);
	ut_memcpy(args,(unsigned char *)&g_utsname,sizeof(g_utsname));
	return 0;
}
unsigned long snull(unsigned long *args)
{
	unsigned long syscall_no;

	asm volatile("movq %%rax,%0" : "=r" (syscall_no));
	ut_printf("ERROR: SYSCALL null as hit :%d \n",syscall_no);	
	while(1);
	return 1;
}
