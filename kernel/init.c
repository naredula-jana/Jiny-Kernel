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


/* SLAB cache for vm_area_struct structures */
kmem_cache_t *vm_area_cachep;
/* SLAB cache for mm_struct structures (tsk->mm) */
kmem_cache_t *mm_cachep;
int brk_pnt=0;
int init_kernel(unsigned long end_addr)
{
	int ret;
	ut_printf("Initialising: ISR descriptors.. \n");
	init_descriptor_tables();

	ut_printf("Initialising: serial\n");
	init_serial();

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

	ret=init_smp_force(2);
	ut_printf("NEW SMP: completed, ret:%d maxcpus: %d \n",ret,getmaxcpus());
	ut_printf("SECOND SMP: completed, ret:%d maxcpus: %d \n",ret,getmaxcpus());
#endif

	cli();  /* disable interrupt incase if it is enabled while apic is started */



	ut_printf("Initalising: VFS.. \n");
	init_vfs();

	init_networking();

//	ar_registerInterrupt(128,syscall_handler);

	init_symbol_table();

	init_devClasses();
	init_modules();
	ut_printf("Initialization completed \n");


#ifdef NETWORKING


#endif

	return 1 ;
}

