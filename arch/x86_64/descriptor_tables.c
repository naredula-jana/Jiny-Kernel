#include "common.h"
#include "task.h"
#include "mach_dep.h"

#include "descriptor_tables.h"

// Internal function prototypes.
static void init_gdt();
static void init_idt();
static void idt_set_gate(int slot, uint8_t type, uint8_t dpl,
		addr_t handler, int ist);


static gdt_entry_t gdt_entries[MAX_CPUS][9];
static gdt_ptr_t   gdt_ptr[MAX_CPUS];
idt_ptr_t   idt_ptr;
idt_entry_t idt_entries[MAX_IRQS];
struct cpu_state g_cpu_state[MAX_CPUS];
static tss_t tss[MAX_CPUS];

// Initialisation routine - zeroes all the interrupt service routines,
// initialises the GDT and IDT.
int init_descriptor_tables() {

	// Initialise the global descriptor table.
	init_gdt(0);
	// Initialise the interrupt descriptor table.
	init_idt();
	return JSUCCESS;
}
static inline void seg_descr_set_base(gdt_entry_t *descr, uint32_t base) {
	descr->base_address_low = base & 0xffffff;
	descr->base_address_high = (base >> 24) & 0xff;
}

static inline void seg_descr_set_limit(gdt_entry_t *descr, uint32_t lim) {
	descr->seg_limit_low = lim & 0xffff;
	descr->seg_limit_high = (lim >> 16) & 0xf;
}

static void seg_descr_setup(gdt_entry_t *seg_descr, uint8_t type, uint8_t dpl,
		uint32_t base, uint32_t limit, uint8_t flags) {
	seg_descr_set_limit(seg_descr, limit);
	seg_descr_set_base(seg_descr, base);

	/* set type and privilege level */
	seg_descr->flags_low = type | ((dpl & 0x03) << SEG_DPL_SHIFT);
	/* then set other flags */
	seg_descr->flags_low |= !!(flags & SEG_FLG_PRESENT) << 7;
	seg_descr->flags_high = (flags >> 1) & 0xf;
}
static inline void gdtr_load(gdt_ptr_t *gdtr_reg) {
	asm volatile("lgdtq %0\n" : : "m" (*gdtr_reg));
}
static inline void tr_load(uint16_t s) {
#ifdef SMP
	//if (getcpuid() != 0 )  return;  //TODO : later need find the solution for SMP
#endif
	asm volatile("ltr %0" : : "r" (s));
}
#define gdt_install_tss(tss_descr, dpl, base, limit, flags) \
	__install_tss(tss_descr, SEG_TYPE_TSS, dpl, base, limit, flags)

static void __install_tss(struct tss_descr *descr, int type, uint8_t dpl,
		uint64_t base, uint32_t limit, uint8_t flags) {
	seg_descr_setup(&descr->seg_low, type, dpl, base & 0xffffffffU, limit,
			flags);
	descr->seg_high.base_rest = (base >> 32) & 0xffffffffU;
	descr->seg_high.ignored = 0;
}
void ar_setupTssStack(unsigned long stack) {
	int cpu = getcpuid();

	tss[cpu].rsp0 = stack;
	gdt_install_tss((tss_descr_t *)&gdt_entries[cpu][TSS_DESCR], SEG_DPL_KERNEL,
			(uint64_t)&tss[cpu], TSS_DEFAULT_LIMIT, SEG_FLG_PRESENT);
	tr_load(GDT_SEL(TSS_DESCR));
	return;
}

static void tss_init(tss_t *tssp) {
	int i;

	ut_memset((unsigned char *) tssp, 0, sizeof(*tssp));
	for (i = 0; i < TSS_NUM_ISTS; i++) {
		tssp->ists[i] = 0;
	}
	tssp->rsp0 = (uint64_t) &i; /* At this point in time just need a stack , proper stack will setup before the task is moved to ring-3 */

	tssp->iomap_base = TSS_BASIC_SIZE;
}

static void init_gdt(int cpu) {
	ut_memset((unsigned char *) &gdt_entries[cpu][0], 0,
			sizeof(gdt_entries[cpu]));

	/* Null segment */
	seg_descr_setup(&gdt_entries[cpu][NULL_DESCR], 0, 0, 0, 0, 0);

	/* Kernel code segment */
	seg_descr_setup(&gdt_entries[cpu][KCODE_DESCR], SEG_TYPE_CODE,
			SEG_DPL_KERNEL, 0, 0xfffff,
			SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

	/* Kernel data segment */
	seg_descr_setup(&gdt_entries[cpu][KDATA_DESCR], SEG_TYPE_DATA,
			SEG_DPL_KERNEL, 0, 0xfffff,
			SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

	/* User code segment */
	seg_descr_setup(&gdt_entries[cpu][UCODE_DESCR], SEG_TYPE_CODE, SEG_DPL_USER,
			0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

	/* User data segment */
	seg_descr_setup(&gdt_entries[cpu][UDATA_DESCR], SEG_TYPE_DATA, SEG_DPL_USER,
			0, 0xfffff, SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

	/* Kerne 32bit code segment */
	seg_descr_setup(&gdt_entries[cpu][KCODE32_DESCR], SEG_TYPE_CODE,
			SEG_DPL_KERNEL, 0, 0xfffff,
			SEG_FLG_PRESENT | SEG_FLG_OPSIZE | SEG_FLG_GRAN);

	/* TSS initialization */
	tss_init(&tss[cpu]);
	gdt_install_tss((tss_descr_t *)&gdt_entries[cpu][TSS_DESCR], SEG_DPL_KERNEL,
			(uint64_t)&tss[cpu], TSS_DEFAULT_LIMIT, SEG_FLG_PRESENT);

	gdt_ptr[cpu].limit = (sizeof(gdt_entries) / MAX_CPUS) - 1;
	gdt_ptr[cpu].base = (uint64_t) &gdt_entries[cpu][0];

	gdtr_load(&gdt_ptr[cpu]);
	tr_load(GDT_SEL(TSS_DESCR));
}

int ar_archSetUserFS(unsigned long addr) /* TODO need to reimplement using LDT */
{
	if (addr == 0) {
		g_current_task->thread.userland.user_fs = 0;
		g_current_task->thread.userland.user_fs_base = 0;
	} else {
		g_current_task->thread.userland.user_fs = GDT_SEL(FS_UDATA_DESCR)
				| SEG_DPL_USER; /* 8th location in gdt table */
		g_current_task->thread.userland.user_fs_base = addr;
	}
	ar_updateCpuState(g_current_task,0);
	return 1;
}
int ar_updateCpuState(struct task_struct *next, struct task_struct *prev) {
	int cpuid = next->current_cpu;
	if (cpuid != getcpuid()) {
		BUG();
	}
	if (prev){
		prev->thread.userland.user_stack = g_cpu_state[cpuid].md_state.user_stack;
		prev->thread.userland.user_ds = g_cpu_state[cpuid].md_state.user_ds;
		prev->thread.userland.user_es = g_cpu_state[cpuid].md_state.user_es;
		prev->thread.userland.user_fs = g_cpu_state[cpuid].md_state.user_fs;
		prev->thread.userland.user_gs = g_cpu_state[cpuid].md_state.user_gs;
		prev->thread.userland.user_fs_base =  g_cpu_state[cpuid].md_state.user_fs_base;
	}
	g_cpu_state[cpuid].md_state.user_stack = next->thread.userland.user_stack;
	g_cpu_state[cpuid].md_state.user_ds = next->thread.userland.user_ds;
	g_cpu_state[cpuid].md_state.user_es = next->thread.userland.user_es;
	g_cpu_state[cpuid].md_state.user_fs = next->thread.userland.user_fs;
	g_cpu_state[cpuid].md_state.user_gs = next->thread.userland.user_gs;
	g_cpu_state[cpuid].md_state.user_fs_base = next->thread.userland.user_fs_base;
	g_cpu_state[cpuid].md_state.kernel_stack = (unsigned long) next + TASK_SIZE;

	seg_descr_setup(&gdt_entries[cpuid][FS_UDATA_DESCR], SEG_TYPE_DATA,
			SEG_DPL_USER, g_cpu_state[cpuid].md_state.user_fs_base, 0xfffff,
			SEG_FLG_PRESENT | SEG_FLG_64BIT | SEG_FLG_GRAN);

	gdtr_load(&gdt_ptr[cpuid]);
	asm volatile("mov %0, %%fs":: "r"(g_cpu_state[cpuid].md_state.user_fs));
#if 1 /* fs update using MSR instead of gdt table, in gdt table the value can have only 32 bit whereas in msr it is 64 bit */
	msr_write(MSR_FS_BASE, g_cpu_state[cpuid].md_state.user_fs_base);
#endif
	return 1;
}

extern uint8_t ar_irqsTable[];
extern uint8_t ar_faultsTable[];

#define SET_FAULT_GATE(irqnum)  \
	idt_set_gate(irqnum, SEG_TYPE_INTR, SEG_DPL_KERNEL, \
			(addr_t)&ar_faultsTable + (irqnum) * 0x10, 0);
#define SET_IRQ_GATE(irqnum)                \
	idt_set_gate((irqnum) + 32, SEG_TYPE_INTR,            \
			SEG_DPL_KERNEL,                                \
			(addr_t)&ar_irqsTable + (irqnum) * 0x10, 0)
static void idt_set_gate(int slot, uint8_t type, uint8_t dpl, addr_t handler,
		int ist) {
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
static inline void idtr_load(idt_ptr_t *idtr_ptr) {
	asm volatile("lidtq %0\n" : : "m" (*idtr_ptr));
}
extern void init_handlers();
static void init_idt() {
	int i;

	idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
	idt_ptr.base = (addr_t) &idt_entries;

	ut_memset((unsigned char *) &idt_entries[0], 0, 255 * sizeof(idt_entry_t));
	for (i = 0; i < 32; i++) {
		SET_FAULT_GATE(i);
	}
	for (i = 0; i < (256 - 32); i++) {
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
	//asm volatile("sti");
}
#ifdef SMP
void init_smp_gdt(int current_cpu) {
#if 0
	ut_memcpy(&gdt_entries[current_cpu][0], &gdt_entries[0][0], sizeof(gdt_entries[current_cpu]));
	gdt_ptr[current_cpu].limit = (sizeof(gdt_entries) / MAX_CPUS)-1;
	gdt_ptr[current_cpu].base = (uint64_t)&gdt_entries[current_cpu][0];
	gdtr_load(&gdt_ptr[current_cpu]);
	tr_load(GDT_SEL(TSS_DESCR));
#else
	init_gdt(current_cpu);
#endif

	idtr_load(&idt_ptr);
}
#endif
