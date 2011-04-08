#include "common.h"
#include "interface.h"
#include "isr.h"

unsigned long SYS_printf(unsigned long *args);
unsigned long SYS_exit(unsigned long args);
unsigned long SYS_fork();

long SYS_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags,unsigned long fd, unsigned long off);
unsigned long syscallnull(unsigned long *args);

typedef struct {
        unsigned long (*func)(unsigned long *args);
} syscalltable_t;
syscalltable_t syscalltable[]=
{
	{syscallnull} ,
	{SYS_printf}, /* 1  */
	{SYS_fs_open},
	{SYS_fs_write}, 
	{SYS_fs_read}, 
	{SYS_fs_close}, /* 5  */
	{SYS_fs_fadvise}, 
	{SYS_fs_fdatasync}, 
	{SYS_fs_lseek}, 
	{SYS_sc_exit}, 
	{SYS_sc_execve},/* 10 */
	{SYS_sc_fork},
	{SYS_mmap}, 
	{syscallnull} 
};

unsigned long SYS_printf(unsigned long *args)
{
	DEBUG("INSIDE THE new SYSCALLin printf \n");
	ut_printf("%s\n",args);
	return 1;
}


long SYS_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags,unsigned long fd, unsigned long off)
{

}
unsigned long syscallnull(unsigned long *args)
{
	DEBUG("SYSCALL null as hit \n");	
	return 11;
}
