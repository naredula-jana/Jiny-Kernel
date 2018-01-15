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
static void init_ioapic();

static int lapic_dummy = 0;

/*
 *  Exported globals here.
 */

volatile int imps_release_cpus = 0;
int imps_enabled = 0;
int g_imps_num_cpus __attribute__ ((section ("confdata"))) = 1;  /* all read only data is kept in this section to have better cache friendly */
unsigned long imps_lapic_addr = ((unsigned long) (&lapic_dummy)) - LAPIC_ID;
unsigned long imps_ioapic_addr = 0;
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
static int apic_getcpuid() {
	int id;
	if (g_imps_num_cpus == 1)
		return 0;

	id= APIC_ID(IMPS_LAPIC_READ(LAPIC_ID)); /* id is 0 based */

	if (id >= MAX_CPUS || id < 0 || id >= g_imps_num_cpus)
		return 0;

	return id;
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
#include "fp.h"
void smp_main() {
    int cpuid;
    unsigned long rsp,rbp;

	while (wait_non_bootcpus == 1)
		; /* wait till boot cpu trigger */
	cli();
	/* disable interrupts */
	cpuid=apic_getcpuid();
	init_smp_gdt(cpuid);
	local_ap_apic_init();
	__enable_apic();
	init_syscall(cpuid);
//	interrupts_enable();

	local_ap_apic_init(); /* TODO : Need to call this second time to get APIC enabled */
    ut_log("	SMP From the New processor cpu id:%d stack:%x -----------------\n",cpuid,&cpuid);

#if 1
    /* adjust the stack pointer */
    asm volatile("movq %%rbp,%[prev_rbp]\n\t" \
    		"movq %%rsp,%[prev_rsp]\n\t"  \
    		/* output parameters */                            \
    		: [prev_rsp] "=m" (rsp),                \
    		  [prev_rbp] "=m" (rbp)                \
    	   /* input parameters: */                          \
    		:                 \
    		: /* reloaded segment registers */                 \
    		          		"memory");
   ut_log("   SMP  rsp:%x rbp:%x \n",rsp,rbp);
   rsp = KERNEL_CODE_START + rsp -0x40000000;
   rbp = KERNEL_CODE_START + rbp -0x40000000;

   asm volatile("movq %[new_rbp],%%rbp\n\t" \
   		"movq %[new_rsp],%%rsp\n\t"  \
   		/* output parameters */
   		:                 \
   		           	   /* input parameters: */                  \
   		: [new_rsp] "m" (rsp),                \
   		  [new_rbp] "m" (rbp)                \
                         \
   		: /* reloaded segment registers */                 \
   		          		"memory");
#endif

  // interrupts_enable();
 // xsetbv(XCR_XFEATURE_ENABLED_MASK, XFEATURE_MASK_FPSSE);
	idleTask_func();
	return;
}
//#define __old_va(x)                 ((void *)((unsigned long)(x)+0x40000000))
#define __old_va(x)                 ((void *)((unsigned long)(x)+0))
static int boot_cpu(imps_processor *proc) {
	int apicid = proc->apic_id, success = 1, to;
	int cpuid;
	unsigned bootaddr, accept_status;
	unsigned long stack;
	unsigned long bios_reset_vector = (unsigned long)PHYS_TO_VIRTUAL(BIOS_RESET_VECTOR);

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

	stack = (unsigned long)g_cpu_state[cpuid].idle_task;/* TODO : currently stack is hardcoded for second cpu , need to make configurable */
	stack= __pa(stack) + 0x40000000;


	stack = (stack + TASK_SIZE - 0x64);
	p = (int *)((char *) __old_va(bootaddr) + 0x504);
	*p = (int) stack;
	INIT_LOG("		SMP cpuid:%x stack:%x bootaddr:%x p:%x\n",cpuid,stack,bootaddr,p);
	//while(1);
	/*
	 *  Generic CPU startup sequence starts here.
	 */

	/* set BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE,CMOS_RESET_JUMP);
	 *((volatile unsigned long*) bios_reset_vector) = (unsigned long )((bootaddr & 0xFF000) << 12);

	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* assert INIT IPI */
	send_ipi(apicid,
			LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

	UDELAY(1000);

	/* de-assert INIT IPI */
	send_ipi(apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);

	UDELAY(1000);

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
//	UDELAY(1000000);  // This is to avoid proper printing by the new processor
	/*
	 *  Check to see if other processor has started.
	 */
	to = 0;
	p = __va(bootaddr);
	while (*p != 0xA5A5A5A5 && to++ < 100)
		UDELAY(1000);
	INIT_LOG("		SMP: boot addr: %x  cpuid:%d accept_status current_task:%x\n", *p,cpuid,accept_status,g_cpu_state[cpuid].md_state.kernel_stack);
	if (to >= 100) {
		INIT_LOG("		SMP: CPU Not Responding, DISABLED");
		success = 0;
	} else {
		INIT_LOG("		SMP: #%d  Application Processor (AP)", g_imps_num_cpus);
	}

	/*
	 *  Generic CPU startup sequence ends here, the rest is cleanup.
	 */

	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;

	INIT_LOG("\n");

	return success;
}

/*
 *  read bios stuff and fill tables
 */

static void add_processor(imps_processor *proc) {
	int apicid = proc->apic_id;

	INIT_LOG("	SMP: Processor [APIC id %d ver %d]:  ", apicid, proc->apic_ver);
	if (!(proc->flags & IMPS_FLAG_ENABLED)) {
		INIT_LOG(("ERROR: DISABLED\n"));
		return ;
	}
	if (proc->flags & (IMPS_CPUFLAG_BOOT)) {
		INIT_LOG("SMP: #0  BootStrap Processor (BSP)\n");
		return ;
	}
	if (boot_cpu(proc)) {
		/*  XXXXX  add OS-specific setup for secondary CPUs here */
		imps_cpu_apic_map[g_imps_num_cpus] = apicid;
		imps_apic_cpu_map[apicid] = g_imps_num_cpus;
		g_imps_num_cpus++;
	}
}

static int enable_ssx_avx(){
    unsigned long cr4= native_read_cr4();
    unsigned long cr2= native_read_cr4();
    unsigned int eax, ebx, ecx, edx;
    unsigned long xfeatures_mask;
    ecx=0;
    edx=0;
    eax=0;
    ebx=0;
	cpuid_count(XSTATE_CPUID, 0, &eax, &ebx, &ecx, &edx);
	xfeatures_mask = eax + ((unsigned long)edx << 32);
	ut_log("CPUID XSTATE_CPUID eax:%x ebx:%x ecx:%x edx: %x\n",eax,ebx,ecx,edx);

    ecx=0;
    edx=0;
    eax=0;
    ebx=0;
	cpuid_count(0x1, 0, &eax, &ebx, &ecx, &edx);  /* get feature information type==1 */
	ut_log("CPUID Feature INFO  eax:%x ebx:%x ecx:%x edx: %x\n",eax,ebx,ecx,edx);

	native_write_cr2(cr2 | 0x2);
	//native_write_cr4(cr4 |X86_CR4_OSXSAVE | X86_CR4_OSFXSR | X86_CR4_OSXMMEXCPT);
	native_write_cr4(cr4 | X86_CR4_OSXMMEXCPT);
	//native_write_cr4(0);
	if (xfeatures_mask != 0){
	    xsetbv(XCR_XFEATURE_ENABLED_MASK, XFEATURE_MASK_FPSSE | XFEATURE_MASK_YMM);
	   // xsetbv(XCR_XFEATURE_ENABLED_MASK, XFEATURE_MASK_FP);
	    //xsetbv(XCR_XFEATURE_ENABLED_MASK,  XFEATURE_MASK_ZMM_Hi256);
	}
	ut_log("	YMM NEW INITIALISED FP ++++++AVX +++++ enabled: %x : %x  cr4:%x\n",xfeatures_mask,XFEATURE_MASK_FPSSE,cr4);
}
/*
 *  This is the primary function to "force" SMP support, with
 *  the assumption that you have consecutively numbered APIC ids.
 */

int init_smp_force(unsigned long ncpus) {
	int apicid, i,ret;
	imps_processor p;
//while(1);




	ut_log(("		SMP: Intel MultiProcessor \"Force\" Support\n"));

	imps_lapic_addr  = vm_create_kmap("smp_apic",0x100000,PROT_WRITE,MAP_FIXED,0xFee00000);
	imps_ioapic_addr  = vm_create_kmap("io_apic",4096,PROT_WRITE,MAP_FIXED,0xFec00000);
	ut_log("		APIC  virt addr :%x phy addr:%x \n",imps_lapic_addr,__pa(imps_lapic_addr));
	/*
	 *  Setup primary CPU.
	 */
	apicid = IMPS_LAPIC_READ(LAPIC_SPIV);
	IMPS_LAPIC_WRITE(LAPIC_SPIV, apicid|LAPIC_SPIV_ENABLE_APIC);
	apicid = APIC_ID(IMPS_LAPIC_READ(LAPIC_ID));

	imps_cpu_apic_map[0] = apicid;
	imps_apic_cpu_map[apicid] = 0;
//while(1);
	p.type = 0;
	p.apic_ver = 0x10;
	p.signature = p.features = 0;
ut_log("		imps:smp : before the bsp_switch\n");  // TODO : uncommeting this line  looks like stuck with the interrupt-32
	local_apic_bsp_switch();
	local_bsp_apic_init();
	if (ncpus > MAX_CPUS)
		ncpus = MAX_CPUS;

	INIT_LOG("		imps_smp:  stack vert addr:%x  phy:%x \n",g_cpu_state[0].idle_task,__pa(g_cpu_state[0].idle_task));
	//return 0;
//while(1);
	init_ioapic();
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

	enable_ssx_avx();

	wait_non_bootcpus = 0; /* from this point onwards  all non-boot cpus starts */
//while(1);
	ut_log("	SMP: completed, ret:%d maxcpus: %d \n",g_imps_num_cpus,getmaxcpus());
	cli();


	return JSUCCESS;
}
/******************************   IO APIC code ********************/
static void write_ioapic_register( unsigned long apic_base, const uint8_t offset, const uint32_t val) {
	/* tell IOREGSEL where we want to write to */
	*(uint32_t*) (apic_base) = offset;
	/* write the value to IOWIN */
	*(uint32_t*) (apic_base + 0x10) = val;
}

static uint32_t read_ioapic_register( unsigned long apic_base, const uint8_t offset) {
	/* tell IOREGSEL where we want to read from */
	*(uint32_t*) (apic_base) = offset;
	/* return the data from IOWIN */
	return *(uint32_t*) (apic_base + 0x10);
}
#define IOAPIC_ID   0x00
typedef union  {
    uint32_t value;
    struct {
        uint32_t reserved0:24, id:8;
    }  __attribute__ ((packed));;
}ioapic_id_t;

typedef union  {
    uint32_t value;
    struct {
        uint32_t version:8, reserved0:8,
            max_irq:8, reserved1:8;
    } __attribute__ ((packed));;
}ioapic_ver_t;
/*
static struct ioapic_pin ioapic_get_8259A_pin(void)
{
    union ioapic_irqentry entry;
    struct ioapic_pin pic = { .apic = -1, .pin = -1 };
    int max_irq;

    for (int apic = 0; apic < nr_ioapics; apic++) {
        max_irq = ioapic_descs[apic].max_irq;
        for (int irq = 0; irq <= max_irq; irq++) {
            entry = ioapic_read_irqentry(apic, irq);

            if (entry.delivery_mode != IOAPIC_DELMOD_EXTINT)
                continue;

            if (entry.mask != IOAPIC_UNMASK)
                continue;

            pic.apic = apic;
            pic.pin = irq;
            return pic;
        }
    }

    return pic;
}
*/
static void init_ioapic(){
	int i;

	ioapic_ver_t version = { .value = 0 };
	ioapic_id_t id = { .value = 0 };
	id.value = read_ioapic_register(imps_ioapic_addr, IOAPIC_ID);
	INIT_LOG("		IOAPIC  id :%d \n",id.value);
	if (id.value != 0){

	}
}
void Jcmd_ioapic() {
	int i;
	ioapic_ver_t version = { .value = 0 };
	ioapic_id_t id = { .value = 0 };
	id.value = read_ioapic_register(imps_ioapic_addr, IOAPIC_ID);
	ut_printf(" apic :%x \n",id.id);


	 version.value = read_ioapic_register(imps_ioapic_addr, 1);
	 ut_printf(" version = %d  max_irq:%d \n",version.version,version.max_irq);

	for (i = 16; i < 30; i++) {
		ut_printf(" ioapic :%d -> %x \n", i, read_ioapic_register(imps_ioapic_addr, i));
	}
}



