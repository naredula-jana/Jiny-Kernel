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
#include "mm.h"
#include "task.h"
/* Forward declarations.  */
extern void init_descriptor_tables();
extern void init_isr();
extern void init_pci();
extern int init_driver_keyboard();
extern void init_tasking();
extern void init_serial();

/* SLAB cache for vm_area_struct structures */
kmem_cache_t *vm_area_cachep;
/* SLAB cache for mm_struct structures (tsk->mm) */
kmem_cache_t *mm_cachep;
extern void syscall_handler(struct fault_ctx *ctx);
int init_kernel(unsigned long end_addr)
{
	ut_printf("Initalising ISR & descriptors \n");
	init_descriptor_tables();
	init_driver_keyboard();
	init_serial();
	ut_printf("Initalising MEMORY physical memory highest addrss:%x \n",end_addr);
	g_serial_output=1;
	init_memory(end_addr);
	g_serial_output=1;
	kmem_cache_init();
	kmem_cache_sizes_init();
	/* SLAB cache for vm_area_struct structures */
	vm_area_cachep = kmem_cache_create("vm_area_struct",sizeof(struct vm_area_struct), 0,0, NULL, NULL);
	mm_cachep = kmem_cache_create("mm_struct",sizeof(struct mm_struct), 0,0,NULL,NULL);

	init_tasking();
	init_pci();
	init_vfs();
	ar_registerInterrupt(128,syscall_handler);
	ut_printf("Initialization completed \n");
	return 1 ;
}
