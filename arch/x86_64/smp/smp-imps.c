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
#include "interface.h"
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

extern int local_ap_apic_init(void);
extern int local_bsp_apic_init(void);
extern void local_apic_bsp_switch(void);

static int lapic_dummy = 0;

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
#if 0
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
extern void __enable_apic(void);
void smp_main() {
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
	unsigned bios_reset_vector = (unsigned )PHYS_TO_VIRTUAL(BIOS_RESET_VECTOR);

	/*
	 * Copy boot code for secondary CPUs here.  Find it in between
	 * "patch_code_start" and "patch_code_end" symbols.  The other CPUs
	 * will start there in 16-bit real mode under the 1MB boundary.
	 * "patch_code_start" should be placed at a 4K-aligned address
	 * under the 1MB boundary.
	 */
	extern void *trampoline_data;
	int *p;
	cpuid = proc->apic_id;
	bootaddr = (512 - 64) * 1024;
	memcpy((unsigned char *) __va(bootaddr),(unsigned char *) &trampoline_data, 0x512);

	stack = (unsigned long)g_idle_tasks[cpuid];/* TODO : currently stack is hardcoded for second cpu , need to make configurable */
	stack = (stack + TASK_SIZE - 0x64);
	p = (int *)((char *) __va(bootaddr) + 0x504);
	*p = (int) stack;
	/*
	 *  Generic CPU startup sequence starts here.
	 */

	/* set BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE,CMOS_RESET_JUMP);
	*((volatile unsigned *) bios_reset_vector) = (unsigned)((bootaddr & 0xFF000) << 12);

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
	KERNEL_PRINT("SMP: boot addr: %x  cpuid:%d \n", *p,cpuid);
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
		return ;
	}
	if (proc->flags & (IMPS_CPUFLAG_BOOT)) {
		KERNEL_PRINT("SMP: #0  BootStrap Processor (BSP)\n");
		return ;
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

int init_smp_force(int ncpus) {
	int apicid, i;
	imps_processor p;

	KERNEL_PRINT(("SMP: Intel MultiProcessor \"Force\" Support\n"));

	imps_lapic_addr = (READ_MSR_LO(0x1b) & 0xFFFFF000);
	imps_lapic_addr = (unsigned long)PHYS_TO_VIRTUAL(imps_lapic_addr);
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
ut_printf(" After adding the processor \n");
	local_bsp_apic_init(); /* TODO : Need to call this twice to get APIC enabled */

	unsigned long *page_table;
	page_table = __va(0x00102000); /* Refer the paging.c code this is work around for SMP */
	*page_table = 0;

	wait_non_bootcpus = 0; /* from this point onwards  all non-boot cpus starts */

	return imps_num_cpus;
}



