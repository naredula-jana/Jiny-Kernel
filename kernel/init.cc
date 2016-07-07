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
extern "C" {
#include "common.h"

#include "pci.h"
#include "mm.h"
#include "task.h"
#include "mach_dep.h"
#include "interface.h"

int g_conf_debug_level __attribute__ ((section ("confdata"))) = 1;
int g_conf_stat_counters_enable __attribute__ ((section ("confdata"))) =0 ;
int __gxx_personality_v0=0; /*TODO:  WORKAROUND: this is to link c++ files with gcc */
/* SLAB cache for vm_area_struct structures */
kmem_cache_t *vm_area_cachep;
/* SLAB cache for mm_struct structures (tsk->mm) */
kmem_cache_t *mm_cachep;

int brk_pnt=0;
uint32_t g_cpu_features;
void *g_print_lock=0;


extern int init_physical_memory(unsigned long unused);
extern int init_kmem_cache(unsigned long arg1);
extern int init_vfs(unsigned long arg1);
extern int init_networking(unsigned long arg1);
extern int init_clock(unsigned long arg1);
extern int init_symbol_table(unsigned long arg1,unsigned long arg2);
extern int init_devClasses(unsigned long arg1);
extern int init_modules(unsigned long arg1);
extern int  init_log_file(unsigned long arg1);
extern int init_jslab(unsigned long arg1);
extern int init_jdevices(unsigned long arg1);
int init_kernel_vmaps(unsigned long arg1);
int  init_code_readonly(unsigned long arg1);
int init_kmemleak(unsigned long arg1);
int init_acpi(unsigned long arg1);
int init_network_stack(unsigned long arg1);
int  init_kernel_args(unsigned long arg1);
int init_tasking(unsigned long unused);
int init_serial(unsigned long unused);
int init_vfs(unsigned long arg);
int init_smp_force(unsigned long ncpus);
int init_syscall(unsigned long cpuid);
int init_networking(unsigned long arg);
extern int init_ipc(unsigned long arg);
extern int init_procfs(unsigned long unused);

typedef struct {
	int (*func)(unsigned long arg);
	unsigned long arg1;
	char *comment;
	int log_level;  /* 0-no print , 9-extensive */
} inittable_t;

static inittable_t inittable[] = {
		{init_physical_memory,0,"PhysicalMemory and Symbol table",1},
		{init_descriptor_tables,0,"ISR and Descriptors",0},
		{init_kernel_args,0, "Kernel Args",0},
		{init_syscall,0,       "syscalls",0},
		{init_memory,0,           "Main memory",0},
#ifndef JINY_SLAB
		{init_kmem_cache,0,       "kmem cache",0},
#endif
		{init_ipc,0,       "ipc",0},
		{init_jslab,0,"Jslab initialization",0},
		//{init_syscall,0,       "syscalls",0},

		{init_vfs,0,       "vfs",0},
		{init_tasking,0,       "tasking",0},
		{init_clock,0,       "clock",0},
		{init_driver_keyboard,0,       "keyboard",0},
		{init_serial,0,       "serial",0},
#ifdef MEMLEAK_TOOL
		{init_kmemleak,0,       "kmemleak",0},
#endif
#ifdef SMP
		{init_smp_force, 8,       "smp_init",0},  /* TODO:  MAX CPU'S CANNOT GO BEYOND 8, IF IT GOS beyond 8 , cpu spend lot of time in init_clock  */
#endif
#ifdef NETWORKING
		{init_networking,0,       "network_sched",0},  /* should be after smp,since it uses number of ax core */
#endif
	//	{init_clock,0,       "clock"},
//		{init_code_readonly,0,       "Making code readonly",0},
		{init_kernel_vmaps, 0, "Kernel Vmaps",0},
		{init_jdevices,0,       "devices in c++ ",1},
		{init_procfs,0,"Procfs",0},
		{init_acpi,0,       "ACPI initialzed ",0},
#ifdef NETWORKING
		{init_network_stack,0,       "network stacks",0},
#endif
//		{init_modules,0,       "modules",0},
//		{init_log_file,0, "log file ",0},
		{0,0,0}
};

unsigned char cmdline[9024]={"dummy"};
unsigned long g_phy_mem_size=0;
/* Check if the bit BIT in FLAGS is set.  */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))
extern int _edata; // data and bsss start
extern addr_t end; // end of code and data region
unsigned char kernel_args[1024];
unsigned long g_multiboot_info_ptr;
unsigned long g_multiboot_magic;
unsigned long g_multiboot_module_start,g_multiboot_module_end;

int init_physical_memory(unsigned long unused){
	multiboot_info_t *mbi;
	multiboot_mod_t *mod_p;
	unsigned long *mbi_ptr,*magic_ptr;
	unsigned long max_addr;

	mbi_ptr =__va(0x4000);
	magic_ptr = __va(0x4010);
	mbi=__va(*mbi_ptr);
	mod_p = __va(mbi->mods_addr);

	if (*magic_ptr != MULTIBOOT_BOOTLOADER_MAGIC){
		BUG();
	}

	/* Set MBI to the address of the Multiboot information structure.  */
	ut_log("		mbi: %x mem_lower = %x(%d)KB , mem_upper=%x(%d)KB mod count:%d addr:%x mmaplen:%d mmpaddr:%x Flags:%x\n", mbi, mbi->mem_lower, mbi->mem_lower, mbi->mem_upper, mbi->mem_upper,
			mbi->mods_count, mbi->mods_addr, mbi->mmap_length, mbi->mmap_addr, mbi->flags);

	INIT_LOG("	mod start_addr:%x end_addr:%x vamod :%x-%x	\nmbi: syms[0]:%x syms[1]:%x  syms[2]:%x syms[3]:%x cmdline:%x\n",
			mod_p->mod_start, mod_p->mod_end,__va(mod_p->mod_start), __va(mod_p->mod_end),mbi->syms[0],mbi->syms[1],mbi->syms[2],mbi->syms[3],mbi->cmdline);

	/* Are mmap_* valid?  */
	if (CHECK_FLAG (mbi->flags, 6)) {
		memory_map_t *mmap;
		INIT_LOG("		mmap_addr = 0x%x, mmap_length = 0x%x\n", (unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
		for (mmap = (memory_map_t *) __va(mbi->mmap_addr); (unsigned long) mmap < __va(mbi->mmap_addr) + mbi->mmap_length; mmap = (memory_map_t *) ((unsigned long) mmap
				+ mmap->size + sizeof(mmap->size))) {
			INIT_LOG("		mmap:%x size=0x%x, base_addr high=0x%x low=0x%x,"
				" length = %x %x, type = 0x%x\n", mmap, (unsigned) mmap->size, (unsigned) mmap->base_addr_high, (unsigned) mmap->base_addr_low, (unsigned) mmap->length_high,
					(unsigned) mmap->length_low, (unsigned) mmap->type);
			if (mmap->base_addr_high == 0x0 && mmap->base_addr_low == 0x100000)
				max_addr = 0x100000 + (unsigned long) mmap->length_low;
		}
	}

	g_multiboot_module_start = __va(mod_p->mod_start);
	g_multiboot_module_end = __va(mod_p->mod_end);
	g_phy_mem_size = max_addr;
	INIT_LOG("		Physical memory size :%x (%d)  magic_ptr :%x cmdline: %x :%s\n",g_phy_mem_size,g_phy_mem_size,magic_ptr,mbi->cmdline,__va(mbi->cmdline));
	INIT_LOG("		end of data :%x  image end:%x\n",&_edata, &end);
	ut_memcpy(&kernel_args[0], __va(mbi->cmdline),1023);
	max_addr = &end;
	if ((max_addr < __va(mod_p->mod_end)) && (mbi->mods_count!=0)){
		max_addr = __va(mod_p->mod_end);
	}
	init_symbol_table(&_edata, max_addr);
	INIT_LOG("	    After of symbol processing:  \n");
	//while(1);
	return JSUCCESS;
}
/* Forward declarations.  */
void cmain ();
void ut_cls (void);
void idleTask_func();
/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR.  */
void __stack_chk_fail(){
}
int  init_code_readonly(unsigned long arg1){
  /* To make first 2M or code pages in to Readonly */
	unsigned long *page_table;
	static int init_readonly=0;
	unsigned long intr_flags;
	int cpu;

	spin_lock_irqsave(&g_global_lock, intr_flags);
	if (init_readonly == 0) {/* only one thread should do , other page fault will occur */
		page_table = __va(0x00103000); /* to make first 2M(text) read only */
		*page_table = 0x281;
		init_readonly = 1;
	}
#if 0
	void *p=alloc_page(0, 0);
	ut_memcpy(p,__va(0x101000),PAGE_SIZE);
	flush_tlb(__pa(p));
#endif
	cpu=getcpuid();

	flush_tlb(0x101000);
	flush_tlb_entry(KERNEL_CODE_START);
	ar_flushTlbGlobal();
	ut_log(" TLB flushed by cpu :%d \n",cpu);
	spin_unlock_irqrestore(&g_global_lock, intr_flags);
	return 0;
}

/****************************************House Keeper *******************************************/
extern int housekeep_zeropage_cache();
void housekeeper_thread(void *arg){
	sc_sleep(30);  /* TODO : need to wait some part of initilization*/
	init_log_file(0);
	while(1){
		sc_sleep(100);
		housekeep_zeropage_cache();
		pc_housekeep();
	}
}
/*************************************************************************************************/
int g_boot_completed=0;

/* The video memory address.  */
#define  VIDEO  0xB8000
extern volatile unsigned char *g_video_ram;
unsigned long g_vmalloc_start=0;
unsigned long g_vmalloc_size=0;
extern int shell_main(void *arg);
extern int init_jslab_vmalloc();
int init_kernel_vmaps(unsigned long arg1){
	unsigned long ret;
	int map_size;
	unsigned long vaddr;

#if 1
	if ((ret = vm_mmap(0, (unsigned long) KADDRSPACE_START, g_phy_mem_size,
			PROT_WRITE, MAP_FIXED, KADDRSPACE_START,"phy_mem")) == 0) {
		ut_log("	ERROR: kernel address map Fails \n");
	}else{
		ut_log("	Kernel vmap: physical ram: %x-%x size:%dM\n",KADDRSPACE_START,KADDRSPACE_START+g_phy_mem_size,g_phy_mem_size/1000000);
	}

	if (vm_mmap(0, HIGHPRIORITY_APP_START, HIGHPRIORITY_APP_LEN, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS, 0,"highpriority")==0){
		ut_log("	ERROR: kernel highpriority address map Fails \n");
	}else{
		ut_log("	Kernel high priority map:%x len:%d \n",HIGHPRIORITY_APP_START, HIGHPRIORITY_APP_LEN);
	}

#endif
	g_video_ram = vm_create_kmap("videoram",0x8000000,PROT_WRITE,MAP_FIXED,VIDEO);
	g_vmalloc_size=0x8000000;

	g_vmalloc_start = vm_create_kmap("vmalloc",g_vmalloc_size,PROT_WRITE,MAP_ANONYMOUS,0);
	if (g_vmalloc_start != 0){
		init_jslab_vmalloc();
	}

	return JSUCCESS;
}
extern int g_conf_func_debug;
int g_init_loglevel=1;
extern void Jcmd_remove_old_pte();
void cmain() {  /* This is the first c function to be executed */
	int i,ret;
	unsigned long  current_task;
	unsigned long current_stack_pointer asm("esp");

	current_task = (((unsigned long)(&i) & ~(TASK_SIZE - 1)));
	g_cpu_state[0].md_state.kernel_stack = current_task + TASK_SIZE;
//BRK;
//	ut_log(" Before g_conf_func_debug-> :%x(%d) current task:%x\n",g_conf_func_debug,g_conf_func_debug,g_cpu_state[0].current_task);
	for (i=0; inittable[i].func != 0; i++){
		g_init_loglevel = inittable[i].log_level ;
		ut_log("%d : INITIALIZING :%s  ...\n",i, inittable[i].comment);
		ret = inittable[i].func(inittable[i].arg1);
		if (ret==JSUCCESS){

		}else{
			ut_log("	%s : ....Failed error:%d\n",inittable[i].comment,ret);
		}
	}
	//ar_updateCpuState(g_cpu_state[0].current_task,0);

	uint32_t val[5];
	do_cpuid(1,val);
	//ut_log("	cpuid result %x : %x :%x :%x \n",val[0],val[1],val[2],val[3]);
	g_cpu_features=val[3]; /* edx */

	/* link global locks for debugging purpose */
	arch_spinlock_link(&g_global_lock);

	Jcmd_remove_old_pte();
	g_boot_completed=1;
	sti(); /* start the interrupts finally */

#if 1
	sc_createKernelThread(shell_main, 0, (unsigned char *)"shell_main",0);
	sc_createKernelThread(housekeeper_thread, 0, (unsigned char *)"house_keeper",0);
#endif
	ut_log("	HP version 1.004 Initialization COMPLETED\n-------------------\n");

	idleTask_func();
	return;
}

extern int acpi_shutdown();
void Jcmd_shutdown(){
	acpi_shutdown();
}
}
