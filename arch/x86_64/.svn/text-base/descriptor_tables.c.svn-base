

#include "descriptor_tables.h"
// Lets us access our ASM functions from our C code.
extern void gdt_flush(addr);
extern void idt_flush(addr);

// Internal function prototypes.
static void init_gdt();
static void init_idt();
static void gdt_set_gate(int32_t,addr_t,addr_t,uint8_t,uint8_t);
static void idt_set_gate(int slot, uint8_t type, uint8_t dpl,
                               addr_t handler, int ist);

#define CONFIG_NRCPUS 1
gdt_entry_t gdt_entries[CONFIG_NRCPUS][6];
gdt_ptr_t   gdt_ptr;
idt_ptr_t   idt_ptr;
idt_entry_t idt_entries[256];


// Initialisation routine - zeroes all the interrupt service routines,
// initialises the GDT and IDT.
void init_descriptor_tables()
{

    // Initialise the global descriptor table.
   init_gdt(0);
    // Initialise the interrupt descriptor table.
   init_idt();
}
static inline void seg_descr_set_base(gdt_entry_t *descr, uint32_t base)
{
  descr->base_address_low = base & 0xffffff;
  descr->base_address_high = (base >> 24) & 0xff;
}

static inline void seg_descr_set_limit(gdt_entry_t *descr, uint32_t lim)
{
  descr->seg_limit_low = lim & 0xffff;
  descr->seg_limit_high = (lim >> 16) & 0xf;
}

static void seg_descr_setup(gdt_entry_t *seg_descr, uint8_t type,
                     uint8_t dpl, uint32_t base,
                     uint32_t limit, uint8_t flags)
{
  seg_descr_set_limit(seg_descr, limit);
  seg_descr_set_base(seg_descr, base);

  /* set type and privilege level */
  seg_descr->flags_low = type | ((dpl & 0x03) << SEG_DPL_SHIFT);
  /* then set other flags */
  seg_descr->flags_low |= !!(flags & SEG_FLG_PRESENT) << 7;
  seg_descr->flags_high = (flags >> 1) & 0xf;
}
static inline void gdtr_load(gdt_ptr_t *gdtr_reg)
{
  asm volatile("lgdtq %0\n" : : "m" (*gdtr_reg));
}
void init_gdt(int cpu)
{
  ut_memset(&gdt_entries[cpu][0], 0, sizeof(gdt_entries[cpu]));

  /* Null segment */
  seg_descr_setup(&gdt_entries[cpu][NULL_DESCR], 0, 0, 0, 0, 0);

  /* Kernel code segment */
  seg_descr_setup(&gdt_entries[cpu][KCODE_DESCR], SEG_TYPE_CODE, SEG_DPL_KERNEL,0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

  /* Kernel data segment */
  seg_descr_setup(&gdt_entries[cpu][KDATA_DESCR], SEG_TYPE_DATA, SEG_DPL_KERNEL,0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

  /* User code segment */
  seg_descr_setup(&gdt_entries[cpu][UCODE_DESCR], SEG_TYPE_CODE, SEG_DPL_USER,0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

  /* User data segment */
  seg_descr_setup(&gdt_entries[cpu][UDATA_DESCR], SEG_TYPE_DATA, SEG_DPL_USER,0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

  /* Kerne 32bit code segment */
  seg_descr_setup(&gdt_entries[cpu][KCODE32_DESCR], SEG_TYPE_CODE, SEG_DPL_KERNEL,0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_OPSIZE | SEG_FLG_GRAN);

  /* TSS initialization */
//  tss_init(&tss[cpu]);
//  gdt_install_tss((tss_descr_t *)&gdt[cpu][TSS_DESCR], SEG_DPL_KERNEL,
  //                (uint64_t)&tss[cpu], TSS_DEFAULT_LIMIT, SEG_FLG_PRESENT);

  gdt_ptr.limit = (sizeof(gdt_entries) / CONFIG_NRCPUS)-1;
  gdt_ptr.base = (uint64_t)&gdt_entries[cpu][0];

  gdtr_load(&gdt_ptr);
  //tr_load(GDT_SEL(TSS_DESCR));
}


extern uint8_t ar_irqsTable[];
extern uint8_t ar_faultsTable[];

#define SET_FAULT_GATE(irqnum)  \
    idt_set_gate(irqnum, SEG_TYPE_INTR, SEG_DPL_KERNEL, \
                     (addr_t)&ar_faultsTable + (irqnum) * 0x10, 0);
#define SET_IRQ_GATE(irqnum)                                        \
    idt_set_gate((irqnum) + 32, SEG_TYPE_INTR,            \
                     SEG_DPL_KERNEL,                                \
                     (addr_t)&ar_irqsTable + (irqnum) * 0x10, 0)
static void idt_set_gate(int slot, uint8_t type, uint8_t dpl,
                               addr_t handler, int ist)
{
  idt_entry_t *idt_descr;

  idt_descr = &idt_entries[slot];

  idt_descr->offset_low = handler & 0xffff;
  idt_descr->offset_midd = (handler >> 16) & 0xffff;
  idt_descr->offset_high = (handler >> 32) & 0xffffffffU;
  idt_descr->selector = GDT_SEL(KCODE_DESCR);
  idt_descr->ist = ist & 0x03;
  idt_descr->flags = type | ((dpl & 0x03) << SEG_DPL_SHIFT)
      | (SEG_FLG_PRESENT << 7);
}
static inline void idtr_load(idt_ptr_t *idtr_ptr)
{
  asm volatile("lidtq %0\n" : : "m" (*idtr_ptr));
}
extern void init_handlers();
static void init_idt()
{
	int i;
  
    idt_ptr.limit = sizeof(idt_entry_t) * 256 -1;
    idt_ptr.base  = (addr_t)&idt_entries;
	
	ut_memset(&idt_entries[0], 0, 255*sizeof(idt_entry_t));
	for (i = 0; i < 32; i++) {
		SET_FAULT_GATE(i);
	}
	for (i = 0; i < (255-32); i++) {
		SET_IRQ_GATE(i);
	}
	
	// Remap the irq table: programming 8259a
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
	
	init_handlers();

    idtr_load(&idt_ptr);
	asm volatile("sti");
}
