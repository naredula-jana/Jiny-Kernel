#define XCR_XFEATURE_ENABLED_MASK	0x00000000
/*
 * List of XSAVE features Linux knows about:
 */
enum xfeature {
        XFEATURE_FP,
        XFEATURE_SSE,
        /*
         * Values above here are "legacy states".
         * Those below are "extended states".
         */
        XFEATURE_YMM,
        XFEATURE_BNDREGS,
        XFEATURE_BNDCSR,
        XFEATURE_OPMASK,
        XFEATURE_ZMM_Hi256,
        XFEATURE_Hi16_ZMM,
        XFEATURE_PT_UNIMPLEMENTED_SO_FAR,
        XFEATURE_PKRU,

        XFEATURE_MAX,
};

#define XFEATURE_MASK_FP                (1 << XFEATURE_FP)
#define XFEATURE_MASK_SSE               (1 << XFEATURE_SSE)
#define XFEATURE_MASK_YMM               (1 << XFEATURE_YMM)
#define XFEATURE_MASK_BNDREGS           (1 << XFEATURE_BNDREGS)
#define XFEATURE_MASK_BNDCSR            (1 << XFEATURE_BNDCSR)
#define XFEATURE_MASK_OPMASK            (1 << XFEATURE_OPMASK)
#define XFEATURE_MASK_ZMM_Hi256         (1 << XFEATURE_ZMM_Hi256)
#define XFEATURE_MASK_Hi16_ZMM          (1 << XFEATURE_Hi16_ZMM)
#define XFEATURE_MASK_PT                (1 << XFEATURE_PT_UNIMPLEMENTED_SO_FAR)
#define XFEATURE_MASK_PKRU              (1 << XFEATURE_PKRU)

#define XFEATURE_MASK_FPSSE             (XFEATURE_MASK_FP | XFEATURE_MASK_SSE)
#define XFEATURE_MASK_AVX512            (XFEATURE_MASK_OPMASK \
                                         | XFEATURE_MASK_ZMM_Hi256 \
                                         | XFEATURE_MASK_Hi16_ZMM)

static inline void xsetbv(uint32_t index, uint64_t value)
{
        uint32_t eax = value;
        uint32_t edx = value >> 32;

        asm volatile(".byte 0x0f,0x01,0xd1" /* xsetbv */
                     : : "a" (eax), "d" (edx), "c" (index));
}



#define _AC(X,Y)        X
#define _BITUL(x)       (_AC(1,UL) << (x))
//#define X86_CR4_OSXSAVE_BIT     18 /* enable xsave and xrestore */
//#define X86_CR4_OSXSAVE         _BITUL(X86_CR4_OSXSAVE_BIT)


/*
 * Intel CPU features in CR4
 */
#define X86_CR4_VME_BIT		0 /* enable vm86 extensions */
#define X86_CR4_VME		_BITUL(X86_CR4_VME_BIT)
#define X86_CR4_PVI_BIT		1 /* virtual interrupts flag enable */
#define X86_CR4_PVI		_BITUL(X86_CR4_PVI_BIT)
#define X86_CR4_TSD_BIT		2 /* disable time stamp at ipl 3 */
#define X86_CR4_TSD		_BITUL(X86_CR4_TSD_BIT)
#define X86_CR4_DE_BIT		3 /* enable debugging extensions */
#define X86_CR4_DE		_BITUL(X86_CR4_DE_BIT)
#define X86_CR4_PSE_BIT		4 /* enable page size extensions */
#define X86_CR4_PSE		_BITUL(X86_CR4_PSE_BIT)
#define X86_CR4_PAE_BIT		5 /* enable physical address extensions */
#define X86_CR4_PAE		_BITUL(X86_CR4_PAE_BIT)
#define X86_CR4_MCE_BIT		6 /* Machine check enable */
#define X86_CR4_MCE		_BITUL(X86_CR4_MCE_BIT)
#define X86_CR4_PGE_BIT		7 /* enable global pages */
#define X86_CR4_PGE		_BITUL(X86_CR4_PGE_BIT)
#define X86_CR4_PCE_BIT		8 /* enable performance counters at ipl 3 */
#define X86_CR4_PCE		_BITUL(X86_CR4_PCE_BIT)
#define X86_CR4_OSFXSR_BIT	9 /* enable fast FPU save and restore */
#define X86_CR4_OSFXSR		_BITUL(X86_CR4_OSFXSR_BIT)
#define X86_CR4_OSXMMEXCPT_BIT	10 /* enable unmasked SSE exceptions */
#define X86_CR4_OSXMMEXCPT	_BITUL(X86_CR4_OSXMMEXCPT_BIT)
#define X86_CR4_VMXE_BIT	13 /* enable VMX virtualization */
#define X86_CR4_VMXE		_BITUL(X86_CR4_VMXE_BIT)
#define X86_CR4_SMXE_BIT	14 /* enable safer mode (TXT) */
#define X86_CR4_SMXE		_BITUL(X86_CR4_SMXE_BIT)
#define X86_CR4_FSGSBASE_BIT	16 /* enable RDWRFSGS support */
#define X86_CR4_FSGSBASE	_BITUL(X86_CR4_FSGSBASE_BIT)
#define X86_CR4_PCIDE_BIT	17 /* enable PCID support */
#define X86_CR4_PCIDE		_BITUL(X86_CR4_PCIDE_BIT)
#define X86_CR4_OSXSAVE_BIT	18 /* enable xsave and xrestore */
#define X86_CR4_OSXSAVE		_BITUL(X86_CR4_OSXSAVE_BIT)
#define X86_CR4_SMEP_BIT	20 /* enable SMEP support */
#define X86_CR4_SMEP		_BITUL(X86_CR4_SMEP_BIT)
#define X86_CR4_SMAP_BIT	21 /* enable SMAP support */
#define X86_CR4_SMAP		_BITUL(X86_CR4_SMAP_BIT)
#define X86_CR4_PKE_BIT		22 /* enable Protection Keys support */
#define X86_CR4_PKE		_BITUL(X86_CR4_PKE_BIT)



unsigned long __force_order;
static inline unsigned long native_read_cr2(void)
{
        unsigned long val;

        /* CR2 always exists on x86_64. */
        asm volatile("mov %%cr2,%0\n\t" : "=r" (val), "=m" (__force_order));
        return val;
}
static inline void native_write_cr2(unsigned long val)
{
        asm volatile("mov %0,%%cr2": : "r" (val), "m" (__force_order));
}

static inline unsigned long native_read_cr4(void)
{
        unsigned long val;

        /* CR4 always exists on x86_64. */
        asm volatile("mov %%cr4,%0\n\t" : "=r" (val), "=m" (__force_order));
        return val;
}
static inline void native_write_cr4(unsigned long val)
{
        asm volatile("mov %0,%%cr4": : "r" (val), "m" (__force_order));
}

static inline void native_cpuid(unsigned int *eax, unsigned int *ebx,
				unsigned int *ecx, unsigned int *edx)
{
	/* ecx is often an input as well as an output. */
	asm volatile("cpuid"
	    : "=a" (*eax),
	      "=b" (*ebx),
	      "=c" (*ecx),
	      "=d" (*edx)
	    : "0" (*eax), "2" (*ecx)
	    : "memory");
}
/*
 * Generic CPUID function
 * clear %ecx since some cpus (Cyrix MII) do not set or clear %ecx
 * resulting in stale register contents being returned.
 */
static inline void __cpuid(unsigned int op,
			 unsigned int *eax, unsigned int *ebx,
			 unsigned int *ecx, unsigned int *edx)
{
	*eax = op;
	*ecx = 0;
	native_cpuid(eax, ebx, ecx, edx);
}
#define XSTATE_CPUID		0x0000000d
/* Some CPUID calls want 'count' to be placed in ecx */
static inline void cpuid_count(unsigned int op, unsigned int count,
			       unsigned int *eax, unsigned int *ebx,
			       unsigned int *ecx, unsigned int *edx)
{
	*eax = op;
	*ecx = count;
	native_cpuid(eax, ebx, ecx, edx);
}