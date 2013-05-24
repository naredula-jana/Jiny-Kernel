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

extern int init_physical_memory(unsigned long unused);
extern int init_kmem_cache(unsigned long arg1);
extern int init_vfs(unsigned long arg1);
extern int init_networking(unsigned long arg1);
extern int init_clock(unsigned long arg1);
extern int init_symbol_table(unsigned long arg1);
extern int init_devClasses(unsigned long arg1);
extern int init_modules(unsigned long arg1);
extern int  init_log_file(unsigned long arg1);

typedef struct {
	int (*func)(unsigned long arg);
	unsigned long arg1;
	unsigned *comment;
} inittable_t;

static inittable_t inittable[] = {
		{init_physical_memory,0,"PhysicalMemory"},
		{init_descriptor_tables,0,"ISR and Descriptors"},
		{init_memory,0,           "Main memory"},
		{init_kmem_cache,0,       "kmem cache"},
		{init_syscall,0,       "syscalls"},
		{init_vfs,0,       "vfs"},
		{init_tasking,0,       "tasking"},
		{init_driver_keyboard,0,       "keyboard"},
		{init_serial,0,       "serial"},
#ifdef MEMLEAK_TOOL
		{init_kmemleak,0,       "kmemleak"},
#endif
#ifdef SMP
		{init_smp_force,4,       "kmemleak"},
#endif
#ifdef NETWORKING
		{init_networking,0,       "networking"},
#endif
		{init_clock,0,       "clock"},
		{init_symbol_table,0,       "symboltable"},
		{init_devClasses,0,       "devicesclasses"},
		{init_modules,0,       "modules"},
//		{init_log_file,0, "log file "},
		{0,0,0}
};

unsigned long g_multiboot_info_ptr;
unsigned long g_multiboot_magic ;
unsigned long g_multiboot_mod_addr=0;
unsigned long g_multiboot_mod_len=0;
unsigned long g_phy_mem_size=0;
/* Check if the bit BIT in FLAGS is set.  */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))
int init_physical_memory(unsigned long unused){
	multiboot_info_t *mbi;
	unsigned long max_addr;
	/* Am I booted by a Multiboot-compliant boot loader?  */
	if (g_multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		ut_log("INVALID  magic:%x addr :%x   \n", g_multiboot_magic, g_multiboot_info_ptr);
		while (1)
			;
		return -1;
	}

	/* Set MBI to the address of the Multiboot information structure.  */
	mbi = (multiboot_info_t *) g_multiboot_info_ptr;
	mbi = __va(mbi);
	ut_log("	mbi: %x mem_lower = %x(%d)KB , mem_upper=%x(%d)KB mod count:%d addr:%x mmaplen:%d mmpaddr:%x Flags:%x\n", mbi, mbi->mem_lower, mbi->mem_lower, mbi->mem_upper, mbi->mem_upper,
			mbi->mods_count, mbi->mods_addr, mbi->mmap_length, mbi->mmap_addr, mbi->flags);
	ut_log("		mbi: syms[0]:%x syms[1]:%x  syms[2]:%x syms[3]:%x cmdline:%x\n",mbi->syms[0],mbi->syms[1],mbi->syms[2],mbi->syms[3],mbi->cmdline);

	/* Are mmap_* valid?  */
	if (CHECK_FLAG (mbi->flags, 6)) {
		memory_map_t *mmap;
		ut_log("	mmap_addr = 0x%x, mmap_length = 0x%x\n", (unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
		for (mmap = (memory_map_t *) __va(mbi->mmap_addr); (unsigned long) mmap < __va(mbi->mmap_addr) + mbi->mmap_length; mmap = (memory_map_t *) ((unsigned long) mmap
				+ mmap->size + sizeof(mmap->size))) {
			ut_log("	mmap:%x size=0x%x, base_addr high=0x%x low=0x%x,"
				" length = %x %x, type = 0x%x\n", mmap, (unsigned) mmap->size, (unsigned) mmap->base_addr_high, (unsigned) mmap->base_addr_low, (unsigned) mmap->length_high,
					(unsigned) mmap->length_low, (unsigned) mmap->type);
			if (mmap->base_addr_high == 0x0 && mmap->base_addr_low == 0x100000)
				max_addr = 0x100000 + (unsigned long) mmap->length_low;

		}
	}

	if (mbi->mods_count > 0) {
		multiboot_mod_t *mod;

		mod =(multiboot_mod_t *) mbi->mods_addr;
		g_multiboot_mod_addr = mod->mod_start;
		g_multiboot_mod_len = mod->mod_end - mod->mod_start;
	}
	g_phy_mem_size = max_addr;
	return 0;
}
/* Forward declarations.  */
void cmain ();
void ut_cls (void);
void idleTask_func();
/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR.  */
void __stack_chk_fail(){
}
void idleTask_func() {
	int k=0;
	ut_printf("Idle Thread Started cpuid: %d stack addrss:%x \n",getcpuid(),&k);
	while (1) {
		__asm__("hlt");
		sc_schedule();
	}
}
/****************************************House Keeper *******************************************/
Jcmd_initlog(){
	init_log_file(0);
}
#define MAX_PAGES_SYNC 100
void housekeeper_thread(){
	int ret;
	sc_sleep(3000);  /* TODO : need to wait some part of initilization*/
	init_log_file(0);
	while(1){
		sc_sleep(50);
		ret=MAX_PAGES_SYNC;
		while (ret == MAX_PAGES_SYNC) ret=fs_data_sync(MAX_PAGES_SYNC);

	}
}
/*************************************************************************************************/
int g_boot_completed=0;
extern int shell_main(void *arg);
void cmain() {  /* This is the first c function to be executed */
	multiboot_info_t *mbi;
	unsigned long max_addr;
	int i,ret;
	/* Clear the screen.  */
	ut_cls();

	for (i=0; inittable[i].func != 0; i++){
		ut_log("INITIALIZING :%s  ...\n",inittable[i].comment);
		ret = inittable[i].func(inittable[i].arg1);
		if (ret==0){
			//ut_log(" ... Success\n");
		}else{
			ut_log("	%s : ....Failed error:%d\n",inittable[i].comment,ret);
		}
	}

	uint32_t val[5];
	do_cpuid(1,val);
	ut_log("	cpuid result %x : %x :%x :%x \n",val[0],val[1],val[2],val[3]);
	g_cpu_features=val[3]; /* edx */
	if ((ret=vm_mmap(0,(unsigned long)KERNEL_ADDR_START ,g_phy_mem_size,PROT_WRITE,MAP_FIXED,KERNEL_ADDR_START)) == 0) /* this is for SMP */
	{
		ut_log("ERROR: kernel address map Fails \n");
	}


	sc_createKernelThread(shell_main, 0, (unsigned char *)"shell_main");
	sc_createKernelThread(housekeeper_thread, 0, (unsigned char *)"house_keeper");
	sti(); /* start the interrupts finally */
	g_boot_completed=1;
	idleTask_func();
	return;
}
