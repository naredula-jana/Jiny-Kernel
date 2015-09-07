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
static unsigned long stored_system_times[MAX_CPUS];
static  unsigned long __native_read_tsc(void);

extern unsigned long  g_jiffie_errors;
int g_conf_wall_clock=0;
int ut_get_wallclock(unsigned long *sec, unsigned long *usec) {
	unsigned long tmp_usec;
#if 0
	if (g_conf_wall_clock == 1) {
		uint32_t version = wall_clock.version;
		msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
		if (sec != 0) {
			tmp_usec = curr_system_time / 1000; /* so many usec from start */
			*sec = wall_clock.sec + (tmp_usec / 1000000);
		}
		if (usec != 0) {
			*usec = tmp_usec % 1000000;
		}
	} else {
#endif
		unsigned long sys_time = ut_get_systemtime_ns();
		if (sec != 0) {
			*sec = (sys_time / 1000000000) + start_time;
		}
		if (usec != 0) {
			*usec = (sys_time / 1000) % 1000000;
		}
//	}
	return 1;
}

#define DECLARE_ARGS(val, low, high)    unsigned low, high
#define EAX_EDX_RET(val, low, high)     "=a" (low), "=d" (high)
#define EAX_EDX_VAL(val, low, high)     ((low) | ((uint64_t)(high) << 32))
static  unsigned long __native_read_tsc(void){
         DECLARE_ARGS(val, low, high);
         asm volatile("rdtsc" : EAX_EDX_RET(val, low, high));
         return EAX_EDX_VAL(val, low, high);
}

static spinlock_t	time_spinlock;
int g_conf_hw_clock=1;  /* sometimes hw clock is very slow or not accurate , then jiffies can be used at the resoultion of 10ms */

static unsigned long last_jiffie_tsc=0;
static unsigned long total_tsc_per_jiffie;

int g_conf_ms_per_jiffie=6;/* instaed of 10ms, the apic timer interrupt generating faster , so it canged from 10 to 6*/
#define MS_PER_JIFF g_conf_ms_per_jiffie
#define NS_PER_MS 1000000
#define NS_PER_JIFFIE (NS_PER_MS*MS_PER_JIFF)
//static unsigned long stored_system_times_ns[MAX_CPUS];
static unsigned long stored_start_times[MAX_CPUS];
static unsigned long store_common_time_ns=0;
unsigned long g_stat_clock_errors=0;
static unsigned long get_percpu_ns() { /* get percpu nano seconds */
	unsigned long time_ns;
	unsigned long intr_flags;
	uint32_t version;
	int cpu = getcpuid();

	if (g_conf_hw_clock == 1) {
		do {
			version = vcpu_time[cpu].version;
			time_ns = (__native_read_tsc() - vcpu_time[cpu].tsc_timestamp);
			if (vcpu_time[cpu].tsc_shift >= 0) {
				time_ns <<= vcpu_time[cpu].tsc_shift;
			} else {
				time_ns >>= -vcpu_time[cpu].tsc_shift;
			}
			time_ns = (time_ns * vcpu_time[cpu].tsc_to_system_mul) >> 32;
			time_ns = time_ns + vcpu_time[cpu].system_time;
		} while ((vcpu_time[cpu].version & 0x1)
				|| (version != vcpu_time[cpu].version));
	} else {
		unsigned long tsc = __native_read_tsc();

		signed long delta_ns = (((tsc - (stored_start_times[cpu] - stored_start_times[0]) - last_jiffie_tsc)*NS_PER_JIFFIE)/total_tsc_per_jiffie);
		if (delta_ns < 0) {
			ut_log(" ERROR NEGAT:  cpu :%d  delta:%x(%d) tsc:%d last_jiffie:%d diff:%d per_jiffie:%d \n",cpu,delta_ns,delta_ns,tsc,last_jiffie_tsc,tsc-last_jiffie_tsc,total_tsc_per_jiffie);
			delta_ns =0;
		}
		if (delta_ns > (2*NS_PER_JIFFIE)){
			//ut_log(" ERROR..:  cpu :%d  delta:%x(%d) tsc:%d last_jiffie:%d diff:%d per_jiffie:%d \n",cpu,delta_ns,delta_ns,tsc,last_jiffie_tsc,tsc-last_jiffie_tsc,total_tsc_per_jiffie);
			delta_ns=0;
			g_stat_clock_errors++;
		}
		time_ns = (g_jiffies*NS_PER_JIFFIE) +  delta_ns;

	}
	//stored_system_times_ns[cpu] = time_ns;
	g_cpu_state[cpu].system_times_ns = time_ns;
	return time_ns;
}
extern int g_conf_hw_clock;
unsigned long ut_get_systemtime_ns(){ /* returns nano seconds */
	unsigned long ns;
	unsigned long intr_flags;
	uint32_t version;
	int i;

	ns = get_percpu_ns();
#if 0
  /* storing the time to make sure this function returns the increasing value of time from any cpu, always monotonic. */
	spin_lock_irqsave(&(time_spinlock), intr_flags);
	if (ns <= store_common_time_ns){
		ns = store_common_time_ns;
	}else{
		store_common_time_ns=ns;
	}
	spin_unlock_irqrestore(&(time_spinlock), intr_flags);
#else
	for (i=0; i<getmaxcpus(); i++){
		if (ns < g_cpu_state[i].system_times_ns){
			ns = g_cpu_state[i].system_times_ns;
		}
	}
#endif

	return ns;
}

void ar_update_jiffie() {
	static int count_for_tsc=0;
	if ( g_conf_hw_clock == 1) {
		unsigned long ms_10 = ut_get_systemtime_ns()/10000000;
		if (ms_10 > g_jiffies) {
			g_jiffies++;
		} else {
			g_jiffie_errors++;
		}
	}else{
		g_jiffies++;
	}
	unsigned long curr_tsc =__native_read_tsc();

	if (count_for_tsc < 300){  /* this is to make sure total_tsc_per_jiffie will be constant after the initial period */
		total_tsc_per_jiffie = curr_tsc - last_jiffie_tsc;
		count_for_tsc++;
	}
	last_jiffie_tsc = curr_tsc;

	return;
}

int init_clock(int cpu_id) {
	static int boot_cpu_clock_init=0;
	uint32_t cpuid_ret[4], *p;
	char signature[13];
	int i;

	if (cpu_id == 0 && boot_cpu_clock_init==0) {
		boot_cpu_clock_init = 1;
		arch_spinlock_init(&time_spinlock, (unsigned char *) "time_lock");

		vcpu_time[0].system_time = 0;

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

		if (kvm_clock_available == 1) {
			msr_write(MSR_KVM_SYSTEM_TIME_NEW, __pa(&vcpu_time[cpu_id]) | 1);
		}
		stored_system_times[cpu_id] = 0;
		start_time = wall_clock.sec;
		ut_log("	Succeded: KVM Clock Signature :%s: cpuid: %x \n", signature,
				cpuid_ret[0]);
		for (i=0; i< getmaxcpus(); i++){
			stored_start_times[i]=0;
		}
	} else {
		if (cpu_id == 0) {
			if (kvm_clock_available == 1) {
				msr_write(MSR_KVM_SYSTEM_TIME_NEW,
						__pa(&vcpu_time[cpu_id]) | 1);
			}
			stored_system_times[cpu_id] = 0;
		}
	}
	if (g_boot_completed == 1){
		static int cpus_started=0;
		int cpu_count = getmaxcpus();
		stored_start_times[cpu_id] = __native_read_tsc();
		cpus_started++;
		while(cpus_started < cpu_count){
			stored_start_times[cpu_id] = __native_read_tsc();
		}
	}
	return JSUCCESS;

}
/***************************************************************************************************/
int Jcmd_clock() {
	unsigned long sec, usec, nsec,msec;
	unsigned long cpu0, mycpu;
	int i;

	nsec= get_percpu_ns();
	usec = nsec/1000;
	sec = usec/1000000;
	ut_printf(" BOOTCPU tsc : %x  currcpu_ts: %x \n)",last_jiffie_tsc, __native_read_tsc());
	ut_printf("jiffies :%d errors:%d   ns:%d \n", g_jiffies,  g_jiffie_errors,get_percpu_ns());
	ut_printf("system time  ns:%d(%x) usec: %d(%x)  sec:%d\n",nsec,nsec,usec,usec,sec);
	nsec= ut_get_systemtime_ns();
	usec = nsec/1000;
	sec = usec/1000000;
	ut_printf("get system time  ns:%d(%x) usec: %d(%x)  sec:%d stored time:%d(%d)\n",nsec,nsec,usec,usec,sec,stored_system_times[0],stored_system_times[0]);

	uint32_t ver=vcpu_time[getcpuid()].version ;
	cpu0 = vcpu_time[0].tsc_timestamp;
	mycpu = vcpu_time[getcpuid()].tsc_timestamp;
	ut_printf(" timestamp cpu0: %d (%d) mycpu: %d (%d)  version:%d systemtiem:%d\n",cpu0,cpu0/1000000,mycpu,mycpu/1000000,ver,vcpu_time[getcpuid()].system_time);

	for (i=0; i<getmaxcpus(); i++){
		ut_printf(" %d : stored time: %x(%d) ns\n",i,g_cpu_state[i].system_times_ns,g_cpu_state[i].system_times_ns);
	}
	for (i=0; i<getmaxcpus(); i++){
		ut_printf(" %d : Start time: %x(%d) ns\n",i,stored_start_times[i],stored_start_times[i]);
	}
#if 0
	msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
	asm volatile("mfence":::"memory");
	asm volatile("sfence" ::: "memory");
	asm volatile("lfence" ::: "memory");
	ut_printf(" MFENCE new wall clock :  sec %d nsec:%d version:%d \n",wall_clock.sec,wall_clock.nsec,wall_clock.version);
	ut_printf(" tsc_per_jiffie: %d \n",total_tsc_per_jiffie);
#endif

	 return 1;
}
