#ifndef __TYPES_h
#define __TYPES_h

typedef long unsigned int  size_t;
typedef unsigned long  addr_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

# if __WORDSIZE == 64
typedef long int                int64_t;
# else
__extension__
typedef long long int           int64_t;
# endif

//typedef signed long long int64_t;
typedef unsigned char uint8_t; 
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;


#if __WORDSIZE == 64
typedef unsigned long int       uint64_t;
#else
__extension__
typedef unsigned long long int  uint64_t;
#endif

#define MAX_CPUS 10  /* TODO: defined the same value at multiple places */
extern int g_imps_num_cpus;
static inline int getcpuid()  __attribute__((always_inline));
static inline int getcpuid() {
	unsigned long cpuid;

	asm volatile("movq %%gs:0x48,%0" : "=r" (cpuid)); // TODO : Hardcoded 48 need to replace with define symbol
	if (cpuid >= MAX_CPUS || cpuid < 0 || cpuid >= g_imps_num_cpus){
		while(1);
		return 0;
	}
	return cpuid;
}
static inline int getmaxcpus()  __attribute__((always_inline));
static inline int getmaxcpus() {
	return g_imps_num_cpus;
}

#endif
