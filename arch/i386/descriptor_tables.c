//
// descriptor_tables.c - Initialises the GDT and IDT, and defines the 
//                       default ISR and IRQ handler.
//                       Based on code from Bran's kernel development tutorials.
//                       Rewritten for JamesM's kernel development tutorials.
//

#include "descriptor_tables.h"
// Lets us access our ASM functions from our C code.
extern void gdt_flush(addr);
extern void idt_flush(addr);

// Internal function prototypes.
static void init_gdt();
static void init_idt();
static void gdt_set_gate(s32int,addr,addr,u8int,u8int);
static void idt_set_gate(u8int,addr,u16int,u8int);

gdt_entry_t gdt_entries[6];
gdt_ptr_t   gdt_ptr;
idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;

// Initialisation routine - zeroes all the interrupt service routines,
// initialises the GDT and IDT.
void init_descriptor_tables()
{

	// Initialise the global descriptor table.
	init_gdt();
	// Initialise the interrupt descriptor table.
	init_idt();

}

static void init_gdt()
{
	gdt_ptr.limit = (sizeof(gdt_entry_t) * 5) - 1;
	gdt_ptr.base  = (addr)&gdt_entries;

	gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
	//    gdt_set_gate(1, 0, 0, 0, 0);                // Null segment
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment

	gdt_flush((addr)&gdt_ptr);
}

// Set the value of one GDT entry.
static void gdt_set_gate(s32int num, addr base, addr limit, u8int access, u8int gran)
{
	gdt_entries[num].base_low    = (base & 0xFFFF);
	gdt_entries[num].base_middle = (base >> 16) & 0xFF;
	gdt_entries[num].base_high   = (base >> 24) & 0xFF;

	gdt_entries[num].limit_low   = (limit & 0xFFFF);
	gdt_entries[num].granularity = (limit >> 16) & 0x0F;

	gdt_entries[num].granularity |= gran & 0xF0;
	gdt_entries[num].access      = access;
}

static void init_idt()
{
	idt_ptr.limit = sizeof(idt_entry_t) * 256 -1;
	idt_ptr.base  = (addr)&idt_entries;

	memset(&idt_entries, 0, sizeof(idt_entry_t)*256);
	// Remap the irq table.
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);
	idt_set_gate( 0, (addr)isr0 , 0x08, 0x8E);
	idt_set_gate( 1, (addr)isr1 , 0x08, 0x8E);
	idt_set_gate( 2, (addr)isr2 , 0x08, 0x8E);
	idt_set_gate( 3, (addr)isr3 , 0x08, 0x8E);
	idt_set_gate( 4, (addr)isr4 , 0x08, 0x8E);
	idt_set_gate( 5, (addr)isr5 , 0x08, 0x8E);
	idt_set_gate( 6, (addr)isr6 , 0x08, 0x8E);
	idt_set_gate( 7, (addr)isr7 , 0x08, 0x8E);
	idt_set_gate( 8, (addr)isr8 , 0x08, 0x8E);
	idt_set_gate( 9, (addr)isr9 , 0x08, 0x8E);
	idt_set_gate(10, (addr)isr10, 0x08, 0x8E);
	idt_set_gate(11, (addr)isr11, 0x08, 0x8E);
	idt_set_gate(12, (addr)isr12, 0x08, 0x8E);
	idt_set_gate(13, (addr)isr13, 0x08, 0x8E);
	idt_set_gate(14, (addr)isr14, 0x08, 0x8E);
	idt_set_gate(15, (addr)isr15, 0x08, 0x8E);
	idt_set_gate(16, (addr)isr16, 0x08, 0x8E);
	idt_set_gate(17, (addr)isr17, 0x08, 0x8E);
	idt_set_gate(18, (addr)isr18, 0x08, 0x8E);
	idt_set_gate(19, (addr)isr19, 0x08, 0x8E);
	idt_set_gate(20, (addr)isr20, 0x08, 0x8E);
	idt_set_gate(21, (addr)isr21, 0x08, 0x8E);
	idt_set_gate(22, (addr)isr22, 0x08, 0x8E);
	idt_set_gate(23, (addr)isr23, 0x08, 0x8E);
	idt_set_gate(24, (addr)isr24, 0x08, 0x8E);
	idt_set_gate(25, (addr)isr25, 0x08, 0x8E);
	idt_set_gate(26, (addr)isr26, 0x08, 0x8E);
	idt_set_gate(27, (addr)isr27, 0x08, 0x8E);
	idt_set_gate(28, (addr)isr28, 0x08, 0x8E);
	idt_set_gate(29, (addr)isr29, 0x08, 0x8E);
	idt_set_gate(30, (addr)isr30, 0x08, 0x8E);
	idt_set_gate(31, (addr)isr31, 0x08, 0x8E);

	idt_set_gate(32, (addr)irq0, 0x08, 0x8E);
	idt_set_gate(33, (addr)irq1, 0x08, 0x8E);
	idt_set_gate(34, (addr)irq2, 0x08, 0x8E);
	idt_set_gate(35, (addr)irq3, 0x08, 0x8E);
	idt_set_gate(36, (addr)irq4, 0x08, 0x8E);
	idt_set_gate(37, (addr)irq5, 0x08, 0x8E);
	idt_set_gate(38, (addr)irq6, 0x08, 0x8E);
	idt_set_gate(39, (addr)irq7, 0x08, 0x8E);
	idt_set_gate(40, (addr)irq8, 0x08, 0x8E);
	idt_set_gate(41, (addr)irq9, 0x08, 0x8E);
	idt_set_gate(42, (addr)irq10, 0x08, 0x8E);
	idt_set_gate(43, (addr)irq11, 0x08, 0x8E);
	idt_set_gate(44, (addr)irq12, 0x08, 0x8E);
	idt_set_gate(45, (addr)irq13, 0x08, 0x8E);
	idt_set_gate(46, (addr)irq14, 0x08, 0x8E);
	idt_set_gate(47, (addr)irq15, 0x08, 0x8E);
	idt_flush((addr)&idt_ptr);

	asm volatile("sti");
}

static void idt_set_gate(u8int num, addr base, u16int sel, u8int flags)
{
	idt_entries[num].base_lo = base & 0xFFFF;
	idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

	idt_entries[num].sel     = sel;
	idt_entries[num].always0 = 0;
	// We must uncomment the OR below when we get to using user-mode.
	// It sets the interrupt gate's privilege level to 3.
	idt_entries[num].flags   = flags /* | 0x60 */;
}
