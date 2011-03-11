/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/kernel.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/

#include "common.h"
#include "interface.h"

void __stack_chk_fail(){
}

/* Check if the bit BIT in FLAGS is set.  */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))


/* Forward declarations.  */
void cmain ();
void ut_cls (void);
void init_memory(addr_t end_addr);
extern int shell_main();
extern int g_debug_level;

unsigned long g_multiboot_info_ptr;
unsigned long g_multiboot_magic ;
unsigned long g_multiboot_mod_addr=0;
unsigned long g_multiboot_mod_len=0;
symb_table_t *g_symbol_table=0;
unsigned long g_total_symbols=0;
/* Check if MAGIC is valid and print the Multiboot information structure
   pointed by ADDR.  */
void cmain ()
{
	multiboot_info_t *mbi;
	unsigned long max_addr;
	int i; 
	/* Clear the screen.  */
	ut_cls ();

	/* Am I booted by a Multiboot-compliant boot loader?  */
	if (g_multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC)
	{
		ut_printf ("INVALID  magic:%x addr :%x   \n", g_multiboot_magic,g_multiboot_info_ptr);
		while(1);
		return;
	}

	/* Set MBI to the address of the Multiboot information structure.  */
	mbi = (multiboot_info_t *) g_multiboot_info_ptr;
	ut_printf("mem_lower = %xKB , mem_upper= %xKB count:%d addr:%x mmaplen:%d mmpaddr:%x \n",  mbi->mem_lower, mbi->mem_upper,mbi->mods_count,mbi->mods_addr,mbi->mmap_length,mbi->mmap_addr);
	/* Are mmap_* valid?  */
	if (CHECK_FLAG (mbi->flags, 6))
	{
		memory_map_t *mmap;

		ut_printf ("mmap_addr = 0x%x, mmap_length = 0x%x\n",
				(unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
		for (mmap = (memory_map_t *) mbi->mmap_addr;
				(unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
				mmap = (memory_map_t *) ((unsigned long) mmap
					+ mmap->size + sizeof (mmap->size)))
		{
			ut_printf (" size = 0x%x, base_addr = 0x%x %x,"
					" length = 0x%x %x, type = 0x%x\n",
					(unsigned) mmap->size,
					(unsigned) mmap->base_addr_high,
					(unsigned) mmap->base_addr_low,
					(unsigned) mmap->length_high,
					(unsigned) mmap->length_low,
					(unsigned) mmap->type);
			if (mmap->base_addr_high==0x0 && mmap->base_addr_low==0x100000)
				max_addr=0x100000 + (unsigned long)mmap->length_low ;
				
		}
	}
	if  (mbi->mods_count > 0)
	{
		multiboot_mod_t *mod;
	
		mod=mbi->mods_addr;
		g_multiboot_mod_addr=mod->mod_start;	
		g_multiboot_mod_len=mod->mod_end - mod->mod_start;	
	}
	init_kernel(max_addr);
	sc_createThread(shell_main);
	sc_schedule();
	while(1) 
	{
		if ( g_debug_level==1 ) 
		{
	//		ut_printf(" Inside the Idle Task \n");
		}
		__asm__("hlt");
	}
	return ;
}    
