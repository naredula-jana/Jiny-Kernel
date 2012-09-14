#include "common.h"
#include "task.h"
#include "descriptor_tables.h"
/* MSRs */
#define MSR_EFER         0xC0000080
#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_CSTAR        0xC0000083
#define MSR_SF_MASK      0xC0000084
#define MSR_FS_BASE      0xC0000100
#define MSR_GS_BASE      0xC0000101
#define MSR_KERN_GS_BASE 0xC0000102
#define MSR_SYSCFG       0xC0010010
#define MSR_APIC_BAR     0x0000001B /* APIC base address register */

#define EFER_SCE   0x00  /* System Call Extensions */


static inline uint64_t msr_read(uint32_t msr)
{
  uint32_t eax, edx;

  __asm__ volatile ("rdmsr\n"
                    : "=a" (eax), "=d" (edx)
                    : "c" (msr));

  return (((uint64_t)edx << 32) | eax);
}

static inline void msr_write(uint32_t msr, uint64_t val)
{
	__asm__ volatile ("wrmsr\n"
			:: "c" (msr), "a" (val & 0xffffffff),
			"d" (val >> 32));
}


static void init_fs_and_gs(int cpuid)
{

	/*msr_write(MSR_GS_BASE, 0); */
	msr_write(MSR_GS_BASE, &g_cpu_state[cpuid]);
	msr_write(MSR_KERN_GS_BASE,
			&g_cpu_state[cpuid]);
	msr_write(MSR_FS_BASE, 0);
	__asm__ volatile ("swapgs");
}

static inline void bit_set(void *bitmap, int bit)
{
	*(unsigned long *)bitmap |= (1 << bit);
}

static inline void efer_set_feature(int ftr_bit)
{
	volatile uint64_t efer = (volatile uint64_t)msr_read(MSR_EFER);

	bit_set(&efer, ftr_bit);
	msr_write(MSR_EFER, efer);
}
extern void syscall_entry(void);
void init_syscall(int cpuid)
{
	efer_set_feature(EFER_SCE);
	msr_write(MSR_STAR,
			((uint64_t)(GDT_SEL(KCODE_DESCR) | SEG_DPL_KERNEL) << 32) |
			((uint64_t)(GDT_SEL(KDATA_DESCR) | SEG_DPL_USER) << 48));
	msr_write(MSR_LSTAR, (uint64_t)syscall_entry);
	msr_write(MSR_SF_MASK, 0x200);
	init_fs_and_gs(cpuid);

}
