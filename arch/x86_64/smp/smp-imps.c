/*
 *  <Insert copyright here : it must be BSD-like so anyone can use it>
 *
 *  Author:  Erich Boleyn  <erich@uruk.org>   http://www.uruk.org/~erich/
 *
 *  Source file implementing Intel MultiProcessor Specification (MPS)
 *  version 1.1 and 1.4 SMP hardware control for Intel Architecture CPUs,
 *  with hooks for running correctly on a standard PC without the hardware.
 *
 *  This file was created from information in the Intel MPS version 1.4
 *  document, order number 242016-004, which can be ordered from the
 *  Intel literature center.
 *
 *  General limitations of this code:
 *
 *   (1) : This code has never been tested on an MPS-compatible system with
 *           486 CPUs, but is expected to work.
 *   (2) : Presumes "int", "long", and "unsigned" are 32 bits in size, and
 *	     that 32-bit pointers and memory addressing is used uniformly.
 */

#define _SMP_IMPS_C

/*
 *  XXXXX  The following absolutely must be defined!!!
 *
 *  The "KERNEL_PRINT" could be made a null macro with no danger, of
 *  course, but pretty much nothing would work without the other
 *  ones defined.
 */

/*
 *  Includes here
 */

#define IMPS_DEBUG
#include "common.h"
#include "task.h"
#include "smp-apic.h"
#include "smp-imps.h"

/*
 *  Defines that are here so as not to be in the global header file.
 */
#define EBDA_SEG_ADDR			0x40E
#define BIOS_RESET_VECTOR		0x467
#define LAPIC_ADDR_DEFAULT		0xFEE00000uL
#define IOAPIC_ADDR_DEFAULT		0xFEC00000uL
#define CMOS_RESET_CODE			0xF
#define		CMOS_RESET_JUMP		0xa
#define CMOS_BASE_MEMORY		0x15

/*
 *  Static defines here for SMP use.
 */

#define DEF_ENTRIES	23

static int lapic_dummy = 0;
static struct {
	imps_processor proc[2];
	imps_bus bus[2];
	imps_ioapic ioapic;
	imps_interrupt intin[16];
	imps_interrupt lintin[2];
} defconfig = { { { IMPS_BCT_PROCESSOR, 0, 0, 0, 0, 0 }, { IMPS_BCT_PROCESSOR,
		1, 0, 0, 0, 0 } }, {
		{ IMPS_BCT_BUS, 0, { 'E', 'I', 'S', 'A', ' ', ' ' } }, { 255, 1, { 'P',
				'C', 'I', ' ', ' ', ' ' } } }, { IMPS_BCT_IOAPIC, 0, 0,
		IMPS_FLAG_ENABLED, IOAPIC_ADDR_DEFAULT }, { { IMPS_BCT_IO_INTERRUPT,
		IMPS_INT_EXTINT, 0, 0, 0, 0xFF, 0 }, { IMPS_BCT_IO_INTERRUPT,
		IMPS_INT_INT, 0, 0, 1, 0xFF, 1 }, { IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT,
		0, 0, 0, 0xFF, 2 }, { IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 3,
		0xFF, 3 }, { IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 4, 0xFF, 4 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 5, 0xFF, 5 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 6, 0xFF, 6 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 7, 0xFF, 7 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 8, 0xFF, 8 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 9, 0xFF, 9 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 10, 0xFF, 10 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 11, 0xFF, 11 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 12, 0xFF, 12 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 13, 0xFF, 13 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 14, 0xFF, 14 }, {
		IMPS_BCT_IO_INTERRUPT, IMPS_INT_INT, 0, 0, 15, 0xFF, 15 } }, { {
		IMPS_BCT_LOCAL_INTERRUPT, IMPS_INT_EXTINT, 0, 0, 15, 0xFF, 0 }, {
		IMPS_BCT_LOCAL_INTERRUPT, IMPS_INT_NMI, 0, 0, 15, 0xFF, 1 } } };

/*
 *  Exported globals here.
 */

volatile int imps_release_cpus = 0;
int imps_enabled = 0;
int imps_num_cpus = 1;
unsigned long imps_lapic_addr = ((unsigned long) (&lapic_dummy)) - LAPIC_ID;
unsigned char imps_cpu_apic_map[IMPS_MAX_CPUS];
unsigned char imps_apic_cpu_map[IMPS_MAX_CPUS];

static inline void io_delay(void) {
	const unsigned short DELAY_PORT = 0x80;
	asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}
void udelay(int loops) {
	while (loops--)
		io_delay(); /* Approximately 1 us */
}

/*
 *  MPS checksum function
 *
 *  Function finished.
 */

static int get_checksum(unsigned start, int length) {
	unsigned sum = 0;

	while (length-- > 0) {
		sum += *((unsigned char *) (start++));
	}

	return (sum & 0xFF);
}
/*
 *  APIC ICR write and status check function.
 */

static int send_ipi(unsigned int dst, unsigned int v) {
	int to, send_status;

	IMPS_LAPIC_WRITE(LAPIC_ICR+0x10, (dst << 24));
	IMPS_LAPIC_WRITE(LAPIC_ICR, v);

	/* Wait for send to finish */
	to = 0;
	do {
		UDELAY(100);
		send_status = IMPS_LAPIC_READ(LAPIC_ICR) & LAPIC_ICR_STATUS_PEND;
	} while (send_status && (to++ < 1000));

	return (to < 1000);
}

int getcpuid() {
	int id;
	if (imps_num_cpus == 1)
		return 0;
#if 1
	id = g_current_task->cpu;
#else
	id= APIC_ID(IMPS_LAPIC_READ(LAPIC_ID)); /* id is 0 based */
#endif
	if (id >= MAX_CPUS || id < 0 || id >= imps_num_cpus)
		return 0;

	return id;
}
int getmaxcpus() {
	return imps_num_cpus;
}
static inline unsigned long interrupts_enable(void) {
	unsigned long o;

	__asm__ volatile (
			"pushfq\n"
			"popq %0\n"
			"sti\n"
			: "=r" (o)
	);

	return o;
}

int wait_non_bootcpus = 1;
/*
 *  Primary function for booting individual CPUs.
 *
 *  This must be modified to perform whatever OS-specific initialization
 *  that is required.
 */

extern void init_smp_gdt(int cpu);
extern void idleTask_func();
void smp_main() {
	int i;
    int cpuid;
	while (wait_non_bootcpus == 1)
		; /* wait till boot cpu trigger */
	cli();
	/* disable interrupts */
	cpuid=getcpuid();
	init_smp_gdt(cpuid);
	local_ap_apic_init();
	__enable_apic();
	init_syscall(cpuid);
	interrupts_enable();

	local_ap_apic_init(); /* TODO : Need to call this second time to get APIC enabled */
    ut_printf("SMP cpu id:%d \n",cpuid);
	idleTask_func();
	return;
}

static int boot_cpu(imps_processor *proc) {
	int apicid = proc->apic_id, success = 1, to;
	int cpuid;
	unsigned bootaddr, accept_status;
	unsigned long stack;
	unsigned bios_reset_vector = PHYS_TO_VIRTUAL(BIOS_RESET_VECTOR);

	/*
	 * Copy boot code for secondary CPUs here.  Find it in between
	 * "patch_code_start" and "patch_code_end" symbols.  The other CPUs
	 * will start there in 16-bit real mode under the 1MB boundary.
	 * "patch_code_start" should be placed at a 4K-aligned address
	 * under the 1MB boundary.
	 */
	extern void *trampoline_data;
	extern char *g_idle_stack;
	int *p;
	cpuid = proc->apic_id;
	bootaddr = (512 - 64) * 1024;
	memcpy((char *) __va(bootaddr), &trampoline_data, 0x512);

	stack = g_idle_tasks[cpuid];/* TODO : currently stack is hardcoded for second cpu , need to make configurable */
	stack = (stack + TASK_SIZE - 0x64);
	p = (char *) __va(bootaddr) + 0x504;
	*p = (int) stack;
	/*
	 *  Generic CPU startup sequence starts here.
	 */

	/* set BIOS reset vector */CMOS_WRITE_BYTE(CMOS_RESET_CODE,
			CMOS_RESET_JUMP);
	*((volatile unsigned *) bios_reset_vector) = ((bootaddr & 0xFF000) << 12);

	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* assert INIT IPI */
	send_ipi(apicid,
			LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

	UDELAY(10000);

	/* de-assert INIT IPI */
	send_ipi(apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);

	UDELAY(10000);

	/*
	 *  Send Startup IPIs if not an old pre-integrated APIC.
	 */
	if (proc->apic_ver >= APIC_VER_NEW) {
		int i;
		for (i = 1; i <= 2; i++) {
			send_ipi(apicid, LAPIC_ICR_DM_SIPI | ((bootaddr >> 12) & 0xFF));
			UDELAY(1000);
		}
	}

	/*
	 *  Check to see if other processor has started.
	 */
	to = 0;
	p = __va(bootaddr);
	while (*p != 0xA5A5A5A5 && to++ < 100)
		UDELAY(10000);
	KERNEL_PRINT("SMP: boot addr: %x \n", *p);
	if (to >= 100) {
		KERNEL_PRINT("SMP: CPU Not Responding, DISABLED");
		success = 0;
	} else {
		KERNEL_PRINT("SMP: #%d  Application Processor (AP)", imps_num_cpus);
	}

	/*
	 *  Generic CPU startup sequence ends here, the rest is cleanup.
	 */

	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;

	KERNEL_PRINT("\n");

	return success;
}

/*
 *  read bios stuff and fill tables
 */

static void add_processor(imps_processor *proc) {
	int apicid = proc->apic_id;

	KERNEL_PRINT("SMP: Processor [APIC id %d ver %d]:  ", apicid, proc->apic_ver);
	if (!(proc->flags & IMPS_FLAG_ENABLED)) {
		KERNEL_PRINT(("DISABLED\n"));
		return 0;
	}
	if (proc->flags & (IMPS_CPUFLAG_BOOT)) {
		KERNEL_PRINT("SMP: #0  BootStrap Processor (BSP)\n");
		return 0;
	}
	if (boot_cpu(proc)) {
		/*  XXXXX  add OS-specific setup for secondary CPUs here */
		imps_cpu_apic_map[imps_num_cpus] = apicid;
		imps_apic_cpu_map[apicid] = imps_num_cpus;
		imps_num_cpus++;
	}
}

/*
 *  This is the primary function to "force" SMP support, with
 *  the assumption that you have consecutively numbered APIC ids.
 */

int imps_force(int ncpus) {
	int apicid, i;
	imps_processor p;

	KERNEL_PRINT(("SMP: Intel MultiProcessor \"Force\" Support\n"));

	imps_lapic_addr = (READ_MSR_LO(0x1b) & 0xFFFFF000);
	imps_lapic_addr = PHYS_TO_VIRTUAL(imps_lapic_addr);
	/*
	 *  Setup primary CPU.
	 */
	apicid = IMPS_LAPIC_READ(LAPIC_SPIV);
	IMPS_LAPIC_WRITE(LAPIC_SPIV, apicid|LAPIC_SPIV_ENABLE_APIC);
	apicid = APIC_ID(IMPS_LAPIC_READ(LAPIC_ID));

	imps_cpu_apic_map[0] = apicid;
	imps_apic_cpu_map[apicid] = 0;

	p.type = 0;
	p.apic_ver = 0x10;
	p.signature = p.features = 0;

	local_apic_bsp_switch();
	local_bsp_apic_init();
	if (ncpus > MAX_CPUS)
		ncpus = MAX_CPUS;

	for (i = 0; i < ncpus; i++) {
		if (apicid == i) {
			p.flags = IMPS_FLAG_ENABLED | IMPS_CPUFLAG_BOOT;
		} else {
			p.flags = IMPS_FLAG_ENABLED;
		}
		p.apic_id = i;
		add_processor(&p);
	}

	local_bsp_apic_init(); /* TODO : Need to call this twice to get APIC enabled */

	unsigned long *page_table;
	page_table = __va(0x00102000); /* Refer the paging.c code this is work around for SMP */
	*page_table = 0;

	wait_non_bootcpus = 0; /* from this point onwards  all non-boot cpus starts */

	return imps_num_cpus;
}



