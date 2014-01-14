#ifndef __MACH_DEP_H
#define __MACH_DEP_H
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/mach_dep.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
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
#define MSR_DEBUGCTL     0x000001d9
#define MSR_LASTBRANCHFROMIP 0x000001db
#define MSR_LASTBRANCHTOIP 0x000001dc
#define MSR_LASTINTFROMIP 0x000001dd
#define MSR_LASTINTTOIP 0x000001de
/*
 * CPUID instruction features register
 */
#define CPUID_FPU   0x00000001
#define CPUID_VME   0x00000002
#define CPUID_DE    0x00000004
#define CPUID_PSE   0x00000008
#define CPUID_TSC   0x00000010
#define CPUID_MSR   0x00000020
#define CPUID_PAE   0x00000040
#define CPUID_MCE   0x00000080
#define CPUID_CX8   0x00000100
#define CPUID_APIC  0x00000200
#define CPUID_B10   0x00000400
#define CPUID_SEP   0x00000800
#define CPUID_MTRR  0x00001000
#define CPUID_PGE   0x00002000
#define CPUID_MCA   0x00004000
#define CPUID_CMOV  0x00008000
#define CPUID_PAT   0x00010000
#define CPUID_PSE36 0x00020000
#define CPUID_PSN   0x00040000
#define CPUID_CLFSH 0x00080000
#define CPUID_B20   0x00100000
#define CPUID_DS    0x00200000
#define CPUID_ACPI  0x00400000
#define CPUID_MMX   0x00800000
#define CPUID_FXSR  0x01000000
#define CPUID_SSE   0x02000000
#define CPUID_XMM   0x02000000
#define CPUID_SSE2  0x04000000
#define CPUID_SS    0x08000000
#define CPUID_HTT   0x10000000
#define CPUID_TM    0x20000000
#define CPUID_B30   0x40000000
#define CPUID_PBE   0x80000000
#if 0

#endif


#define EFER_SCE   0x00  /* System Call Extensions */

#define	EFLAG_TRACEBIT		0x00000100	/* trace enable bit */

/* DEBUGCTLMSR bits  */
#define DEBUGCTLMSR_LBR         (1UL <<  0) /* Last Branch recording */
#define DEBUGCTLMSR_BTF         (1UL <<  1) /* Single-Step on branches */

extern uint32_t g_cpu_features;
static inline void eflag_write(uint64_t value){
	__asm__ volatile ("pushq %0; popfq" : : "r" (value));
}

static inline uint64_t eflag_read(){
	uint64_t value;
	__asm __volatile("pushfq; popq %0" : "=r" (value));
	return value;
}

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

static inline void do_cpuid(uint32_t ax, uint32_t *ret)
{
    __asm __volatile("cpuid"
             : "=a" (ret[0]), "=b" (ret[1]), "=c" (ret[2]), "=d" (ret[3])
             :  "0" (ax));
}

static uint8_t __attribute__((always_inline,used))  inb(uint16_t port)
{
  uint8_t vl;
  __asm__ volatile ("inb %1, %0\n" : "=a" (vl) : "d" (port));
  return vl;
}

/* put byte value to io port */
static void __attribute__((always_inline,used)) outb(uint16_t port,uint8_t vl)
{
  __asm__ volatile ("outb %0, %1\n" : : "a" (vl), "Nd" (port));
}

/* read 16 bit value from io port */
static uint16_t __attribute__((always_inline,used))  inw(uint16_t port)
{
  uint16_t vl;
  __asm__ volatile ("inw %1, %0\n" : "=a" (vl) : "d" (port));
  return vl;
}

/* put 16 bit value to io port */
static void __attribute__((always_inline,used)) outw(uint16_t port,uint16_t vl)
{
  __asm__ volatile ("outw %0, %1\n" : : "a" (vl), "Nd" (port));
}

/* read 32 bit value from io port */
static uint32_t __attribute__((always_inline,used)) inl(uint16_t port)
{
  uint32_t vl;
  __asm__ volatile ("inl %1, %0\n" : "=a" (vl) : "d" (port));
  return vl;
}
/* put 32 bit value to io port */
static void __attribute__((always_inline,used)) outl(uint16_t port,uint32_t vl)
{
	__asm__ volatile ("outl %0, %1\n" : : "a" (vl), "Nd" (port));
}
#endif
