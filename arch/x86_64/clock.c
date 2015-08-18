/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/clock.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "common.h"
#include "mach_dep.h"

#define KVM_CPUID_SIGNATURE 0x40000000
#define KVM_CPUID_FEATURES 0x40000001

#define MSR_KVM_WALL_CLOCK_NEW  0x4b564d00
#define MSR_KVM_SYSTEM_TIME_NEW 0x4b564d01
struct pvclock_vcpu_time_info {
	uint32_t version;
	uint32_t pad0;
	uint64_t tsc_timestamp;
	uint64_t system_time;
	uint32_t tsc_to_system_mul;
	signed char tsc_shift;
	unsigned char flags;
	unsigned char pad[2];
}__attribute__((__packed__)) __attribute__ ((aligned (4096)));;
/* 32 bytes */

struct pvclock_wall_clock {
	uint32_t version;
	uint32_t sec;
	uint32_t nsec;
}__attribute__((__packed__));


static struct pvclock_wall_clock wall_clock;
static volatile struct pvclock_vcpu_time_info vcpu_time[MAX_CPUS];
static int kvm_clock_available = 0;
static uint64_t start_time, curr_system_time = 0; /* updated by the boot cpu */
static int test_count = 0;
static unsigned long stored_system_times[MAX_CPUS]; /* this is used if the old system is more then current*/
unsigned long get_kvm_time_fromboot(){ /* return 10ms units */
	unsigned long diff,ticks;

	if (kvm_clock_available == 0)
		return 0;
	if (getcpuid() != 0) {
		BUG();
	}
	ticks = vcpu_time[0].system_time / 10000000 ;

//	get_systemtime(); /* this function is called to store the time */
#if 1
	diff = vcpu_time[0].system_time - start_time;
	if (diff < 0) {
		curr_system_time = -diff;
	} else {
		curr_system_time = diff;
	}
	//return curr_system_time / 10000000;
#endif
	return ticks;
}
extern unsigned long  g_jiffie_errors;

void init_vcputime(int cpu){
	if (kvm_clock_available == 1){
		msr_write(MSR_KVM_SYSTEM_TIME_NEW, __pa(&vcpu_time[cpu]) | 1);
	}
	stored_system_times[cpu]=0;
}

static inline int kvm_para_available(int cpu_id) {

	uint32_t cpuid_ret[4], *p;
	char signature[13];

	do_cpuid(KVM_CPUID_SIGNATURE, &cpuid_ret[0]);
	p = &signature[0];
	*p = cpuid_ret[1];
	p = &signature[4];
	*p = cpuid_ret[2];
	p = &signature[8];
	*p = cpuid_ret[3];

	signature[12] = 0;
	if (ut_strcmp(signature, "KVMKVMKVM") != 0) {
		ut_log("	FAILED : KVM Clock Signature :%s: \n", signature);
		return JFAIL;
	}

	do_cpuid(KVM_CPUID_FEATURES, &cpuid_ret[0]);

	msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
	kvm_clock_available = 1;
	init_vcputime(cpu_id);

	start_time = wall_clock.sec;
	//g_jiffie_tick = get_kvm_time_fromboot();

	ut_log("	Succeded: KVM Clock Signature :%s: cpuid: %x \n", signature, cpuid_ret[0]);
	return JSUCCESS;

}
int g_conf_wall_clock=0;
int ut_get_wallclock(unsigned long *sec, unsigned long *usec) {
	unsigned long tmp_usec;

	if (kvm_clock_available == 1) {
		if (g_conf_wall_clock == 1) {
			uint32_t version = wall_clock.version;
			msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
			if (sec != 0) {
				tmp_usec = curr_system_time / 1000; /* so many usec from start */
				*sec = wall_clock.sec +( tmp_usec / 1000000);
			}
			if (usec != 0) {
				*usec = tmp_usec % 1000000;
			}
		} else {
			unsigned long sys_time = ut_get_systemtime_ns();
		//	unsigned long sys_time = stored_system_times[0];
			if (sec != 0) {
				*sec = (sys_time / 1000000000) + start_time;
			}
			if (usec != 0) {
				*usec = (sys_time / 1000) % 1000000;
			}
		}
	} else {
		if (sec != 0) {
			*sec = g_jiffies / 100;
		}
		if (usec != 0) {
			*usec = g_jiffies * 10000 ; /* actuall resoultion at the best is 10 ms*/
		}
	}
	return 1;
}

#define DECLARE_ARGS(val, low, high)    unsigned low, high
#define EAX_EDX_RET(val, low, high)     "=a" (low), "=d" (high)
#define EAX_EDX_VAL(val, low, high)     ((low) | ((uint64_t)(high) << 32))
static  unsigned long __native_read_tsc(void)
 {
         DECLARE_ARGS(val, low, high);
         asm volatile("rdtsc" : EAX_EDX_RET(val, low, high));
         return EAX_EDX_VAL(val, low, high);
}

unsigned long test_time_updates=0;
static spinlock_t	time_spinlock;

static unsigned long get_percpu_ns() {  /* get percpu nano seconds */
	unsigned long time;
	unsigned long intr_flags;
	uint32_t version;
	int cpu = getcpuid();

	do {
		version = vcpu_time[cpu].version;
		time = (__native_read_tsc() - vcpu_time[cpu].tsc_timestamp);
		if (vcpu_time[cpu].tsc_shift >= 0) {
			time <<= vcpu_time[cpu].tsc_shift;
		} else {
			time >>= -vcpu_time[cpu].tsc_shift;
		}
		time = (time * vcpu_time[cpu].tsc_to_system_mul) >> 32;
		time = time + vcpu_time[cpu].system_time;
	} while ((vcpu_time[cpu].version & 0x1) || (version != vcpu_time[cpu].version));

	return time;
}
extern int g_conf_hw_clock;
unsigned long ut_get_systemtime_ns(){ /* returns nano seconds */
	unsigned long time;
	unsigned long intr_flags;
	uint32_t version;

	if (g_conf_hw_clock == 1){
		time = get_percpu_ns();
	}else{
		time = g_jiffies * 10 * 1000 * 1000 ;
	}
  /* storing the time to make sure this function returns the increasing value of time from any cpu, always monotonic. */
	spin_lock_irqsave(&(time_spinlock), intr_flags);
	if (time <= stored_system_times[0]){
		time = stored_system_times[0];
	}else{
		stored_system_times[0]=time;
		test_time_updates++;
		//asm volatile("sfence":::"memory");  /* make sure all other cpu see the only one time */
	}
	spin_unlock_irqrestore(&(time_spinlock), intr_flags);

	return time;
}
unsigned long get_100usec(){ /*  return in units of 100 useconds */
	return ut_get_systemtime_ns()/100000;
}
void clock_test(){
	unsigned long curr_u,start_j,start_t,end_t;
	unsigned long incrs=0;
	unsigned long err=0;
	unsigned long sec,usec;

	ut_printf(" first elme: %x  second elem:%x \n",&vcpu_time[0],&vcpu_time[1]);
	curr_u=get_100usec();
	start_j=g_jiffies;
	start_t=curr_u;
	while ((start_j+3000)>g_jiffies){
		unsigned long t = get_100usec();
		if (curr_u < t){
			incrs++;
			curr_u = t;
		}else if (curr_u > t){
			err++;
			curr_u = t;
		}
	}
	end_t=curr_u;
	ut_printf("TEST with new clock result  start: %d end:%d  incrs:%d err:%d  cpuid:%d version:%d time:%d newtime:%d elapsed time:%d\n",start_j,g_jiffies,incrs,err,getcpuid(),vcpu_time[0].version,vcpu_time[0].system_time,ut_get_systemtime_ns(),(end_t-start_t));

	ut_get_wallclock(&sec, &usec);
	curr_u=(sec*30000) + (usec/100);
	start_j=g_jiffies;
	start_t=curr_u;
	err =0;
	incrs=0;
	while ((start_j+1000)>g_jiffies){
		unsigned long t = 	ut_get_wallclock(&sec, &usec);
		t=(sec*10000) + (usec/100);
		if (curr_u < t){
			incrs++;
			curr_u = t;
		}else if (curr_u > t){
			err++;
			curr_u = t;
		}
	}
	end_t=curr_u;
	ut_printf("TEST using gettime of day result  start: %d end:%d  incrs:%d err:%d  cpuid:%d version:%d time:%d newtime:%d elapsed time:%d\n",start_j,g_jiffies,incrs,err,getcpuid(),vcpu_time[0].version,vcpu_time[0].system_time,ut_get_systemtime_ns(),(end_t-start_t));
}
int Jcmd_clock() {
	unsigned long sec, usec, msec;
//	ut_printf("NEW System clock: version:%x(%d) start:%d sec sec: %d(%x) msec:%d(%x) usec:%d(%x) \n",vcpu_time[0].version,vcpu_time[0].version,start_time, sec,sec, msec,msec, usec,usec);
	ut_printf("jiffies :%d errors:%d    get100Usec:%d\n", g_jiffies,  g_jiffie_errors,get_100usec());
#if 0
	unsigned long ntjif,tjif = g_jiffies;
	unsigned long tc[5];
	tc[0]=0;
	tc[1]=0;
	tc[2]=0;
	ntjif=tjif;
	test_time_updates=0;
	while((tjif+3) > g_jiffies ){ /* run for 30ms */
		ut_get_systemtime_ns();
		if (g_jiffies > ntjif){
			tc[ntjif-tjif]=test_time_updates;
			test_time_updates=0;
			ntjif = g_jiffies;
		}
	}
	ut_printf("New updates in 3 slots 0: %d  1: %d  2: %d \n",tc[0],tc[1],tc[2]);
	return 1;

	msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
	ut_printf("msr write  clock: %d nsec:%d \n",wall_clock.sec,wall_clock.nsec);
	ut_get_wallclock(&sec, &usec);
	msec = sec * 1000 + (usec / 1000);
	ut_printf("Wall clock:  sec: %d(%x) msec:%d(%x) usec:%d(%x)\n", sec,sec, msec,msec, usec,usec);

	sec = (vcpu_time[0].system_time / 1000000000) ;
	msec =  (vcpu_time[0].system_time % 1000000000)/1000000 ;
	usec =  (vcpu_time[0].system_time % 1000000) / 1000 ;
	ut_printf("System clock: version:%x(%d) start:%d sec sec: %d(%x) msec:%d(%x) usec:%d(%x) \n",vcpu_time[0].version,vcpu_time[0].version,start_time, sec,sec, msec,msec, usec,usec);
	ut_printf("jiffies :%d errors:%d  sec:%d get100Usec:%d\n", g_jiffies,  g_jiffie_errors,sec,get_100usec());
   clock_test();
#endif
	return 1;
}
int g_conf_kvmclock_enable = 1;
int init_clock(int cpu_id) {
	if (cpu_id ==0){
		arch_spinlock_init(&time_spinlock, (unsigned char *)"time_lock");
	}
	vcpu_time[0].system_time = 0;
	if (kvm_para_available(cpu_id) == JFAIL) {
		ut_log("ERROR:  Kvm clock is diabled by Jiny config\n");
		return JFAIL;
	}
	if (g_conf_kvmclock_enable == 0) {
		ut_log(" ERROR : Kvm clock is diabled by Jiny config\n");
		return JFAIL;
	}

	return JSUCCESS;
}
