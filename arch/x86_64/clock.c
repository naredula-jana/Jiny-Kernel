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
}__attribute__((__packed__));
/* 32 bytes */

struct pvclock_wall_clock {
	uint32_t version;
	uint32_t sec;
	uint32_t nsec;
}__attribute__((__packed__));

static struct pvclock_wall_clock wall_clock;
static struct pvclock_vcpu_time_info vcpu_time[MAX_CPUS];
static int kvm_clock_available = 0;
static uint64_t start_time, curr_system_time = 0; /* updated by the boot cpu */
static int test_count = 0;
unsigned long get_kvm_time_fromboot(){ /* return 10ms units */
	unsigned long diff,ticks;

	if (kvm_clock_available == 0)
		return 0;
	if (getcpuid() != 0) {
		BUG();
	}
	ticks = vcpu_time[0].system_time / 10000000 ;

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

static void init_vpcutime(){
	msr_write(MSR_KVM_SYSTEM_TIME_NEW, __pa(&vcpu_time[0]) | 1);
}
static inline int kvm_para_available(void) {

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
	init_vpcutime();

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
			uint32_t version;
			do {
				version = vcpu_time[0].version;
				if (sec != 0) {
					*sec = (vcpu_time[0].system_time / 1000000000) + start_time;
				}
				if (usec != 0) {
					*usec = (vcpu_time[0].system_time % 1000000) / 1000;
				}
			} while ((vcpu_time[0].version & 0x1)
					|| (version != vcpu_time[0].version));
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
int Jcmd_clock() {
	unsigned long sec, usec, msec;

	msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
	ut_printf("msr write  clock: %d nsec:%d \n",wall_clock.sec,wall_clock.nsec);
	ut_get_wallclock(&sec, &usec);
	msec = sec * 1000 + (usec / 1000);
	ut_printf("Wall clock:  sec: %d(%x) msec:%d(%x) usec:%d(%x)\n", sec,sec, msec,msec, usec,usec);

	sec = (vcpu_time[0].system_time / 1000000000) ;
	msec =  (vcpu_time[0].system_time % 1000000000)/1000000;
	usec =  (vcpu_time[0].system_time % 1000000) / 1000 ;
	ut_printf("System clock: version:%x(%d) start:%d sec sec: %d(%x) msec:%d(%x) usec:%d(%x) \n",vcpu_time[0].version,vcpu_time[0].version,start_time, sec,sec, msec,msec, usec,usec);
	ut_printf("jiffies :%d errors:%d \n", g_jiffies/100,  g_jiffie_errors);
//
	return 1;
}
int g_conf_kvmclock_enable = 1;
int init_clock() {

	vcpu_time[0].system_time = 0;
	if (kvm_para_available() == JFAIL) {
		ut_log("ERROR:  Kvm clock is diabled by Jiny config\n");
		return JFAIL;
	}
	if (g_conf_kvmclock_enable == 0) {
		ut_log(" ERROR : Kvm clock is diabled by Jiny config\n");
		return JFAIL;
	}
	kvm_clock_available = 1;
	return JSUCCESS;
}
