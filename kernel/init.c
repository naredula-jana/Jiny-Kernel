/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/init.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "common.h"

#include "pci.h"
#include "mm.h"
#include "task.h"
#include "mach_dep.h"


/* SLAB cache for vm_area_struct structures */
kmem_cache_t *vm_area_cachep;
/* SLAB cache for mm_struct structures (tsk->mm) */
kmem_cache_t *mm_cachep;
int brk_pnt=0;
uint32_t g_cpu_features;

#if 0
typedef struct {
	void *func;
	unsigned long arg1;
	unsigned *comment;
} inittable_t;

static inittable_t inittable[] = {
		{init_descriptor_tables,0,"ISR and Descriptors"},
		{init_memory,0,           "Main memory"},
		{init_kmem_cache,0,       "kmem cache"},
		0
};
#endif

int init_kernel(unsigned long end_addr)
{
	int ret;
	ut_printf("Init: ISR descriptors.. \n");
	init_descriptor_tables();

	ut_printf("Initialising: MEMORY physical memory highest addrss:%x \n",end_addr);
	init_memory(end_addr);
	//BRK;
	kmem_cache_init();
	kmem_cache_sizes_init();
	/* SLAB cache for vm_area_struct structures */
	vm_area_cachep = kmem_cache_create("vm_area_struct",sizeof(struct vm_area_struct), 0,0, NULL, NULL);
	mm_cachep = kmem_cache_create("mm_struct",sizeof(struct mm_struct), 0,0,NULL,NULL);

	ut_printf("Initalising: syscall,tasks.. \n");
	init_syscall(0); /* init cpu calls for boot cpu */
	init_tasking();

	ut_printf("Initialising: keyboard \n");
	init_driver_keyboard(); /* this should be done after wait queues in init_tasking */

	ut_printf("Initialising: serial\n");
	init_serial();

#ifdef MEMLEAK_TOOL
	kmemleak_init();
#endif

#ifdef SMP
	/* 0xfee00000 - 0xfef00000 for lapic */
	if ((ret=vm_mmap(0,(unsigned long)__va(0xFee00000) ,0x100000,PROT_WRITE,MAP_FIXED,0xFee00000)) == 0) /* this is for SMP */
	{
		ut_printf("SMP: ERROR : mmap fails for \n");
		return 0;
	}

	ret=init_smp_force(4);
	ut_printf("NEW SMP: completed, ret:%d maxcpus: %d \n",ret,getmaxcpus());
	ut_printf("SECOND SMP: completed, ret:%d maxcpus: %d \n",ret,getmaxcpus());
#endif

	cli();  /* disable interrupt incase if it is enabled while apic is started */



	ut_printf("Initalising: VFS.. cpudid:%d \n",getcpuid());
	init_vfs();

	init_networking();


	init_clock();

	init_symbol_table();

	init_devClasses();
	init_modules();
	ut_printf("Initialization completed: cpuid: %d \n",getcpuid());


	uint32_t val[5];
	do_cpuid(1,val);
	ut_printf("cpuid result %x : %x :%x :%x \n",val[0],val[1],val[2],val[3]);
	g_cpu_features=val[3]; /* edx */


	return 1 ;
}

