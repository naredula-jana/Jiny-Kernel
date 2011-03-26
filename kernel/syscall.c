#include "common.h"
#include "interface.h"
#include "isr.h"

unsigned long sys_printf(unsigned long *args);
unsigned long sys_open(unsigned long *args);
unsigned long sys_read(unsigned long *args);
unsigned long sys_write(unsigned long *args);
unsigned long sys_close(unsigned long *args);
unsigned long sys_fadvise(unsigned long *args);
unsigned long sys_fdatasync(unsigned long *args);
unsigned long syscallnull(unsigned long *args);

typedef struct {
        unsigned long (*func)(unsigned long *args);
} syscalltable_t;
syscalltable_t syscalltable[]=
{
	{sys_printf}, /* 1  */
	{sys_open},
	{sys_write}, 
	{sys_read}, 
	{sys_close}, /* 5  */
	{sys_fadvise}, 
	{sys_fdatasync}, 
	{syscallnull} 
};
void syscall_handler(struct fault_ctx *ctx)
{
	unsigned long syscall_id;
	unsigned long args[5];

	syscall_id=ctx->gprs->rax;
	if (syscall_id > 7 || syscall_id < 1) 
	{
	}
	args[0]=ctx->gprs->rbx;
	args[1]=ctx->gprs->rcx;
	args[2]=ctx->gprs->rdx;
	DEBUG(" syscall id :%d args1:%x arg2:%x arg3:%x  \n",syscall_id,args[0],args[1],args[2]);
	ctx->gprs->rax=syscalltable[syscall_id-1].func(args);	
}

unsigned long sys_printf(unsigned long *args)
{
	unsigned char *p;

	p=(unsigned char *)args[0];
	if (p != 0)
	ut_printf("%s\n",p);
	return 1;
}
unsigned long sys_open(unsigned long *args)
{
	struct file *fp;
	fp=fs_open((unsigned char *)args[0],(int)args[1]);
	return (unsigned long )fp;
}

unsigned long sys_read(unsigned long *args)
{

	return fs_read((struct file *)args[0],(unsigned char *)args[1],(unsigned long)args[2]);		
}

unsigned long sys_write(unsigned long *args)
{
		
	return fs_write((struct file *)args[0],(unsigned char *)args[1],(unsigned long)args[2]);		
}

unsigned long sys_close(unsigned long *args)
{
	return fs_close((struct file *)args[0]);
}

unsigned long sys_fadvise(unsigned long *args)
{
	
	return 1;
}
unsigned long sys_fdatasync(unsigned long *args)
{
	return fs_fdatasync((struct file *)args[0]);
}
unsigned long syscallnull(unsigned long *args)
{
	ut_printf("SYSCALL null as hit \n");	
	return 11;
}
