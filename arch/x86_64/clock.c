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
unsigned long get_kvm_clock() /* return 10ms units */
{
	long diff;

	if (kvm_clock_available == 0)
		return 0;
	if (getcpuid() != 0) {
		BUG();
	}
	test_count++;
	//curr_system_time = vpcu_time.system_time;
	diff = vcpu_time[0].system_time - start_time;
	if (diff < 0) {
		curr_system_time = -diff;
	} else {
		curr_system_time = diff;
	}
	return curr_system_time / 10000000;
}
extern unsigned long g_jiffie_tick, g_jiffie_errors;

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

	start_time = vcpu_time[getcpuid()].system_time;
	g_jiffie_tick = get_kvm_clock();

	ut_log("	Succeded: KVM Clock Signature :%s: cpuid: %x \n", signature, cpuid_ret[0]);
	return JSUCCESS;

}
int ut_get_wallclock(unsigned long *sec, unsigned long *usec) {
	unsigned long tmp_usec;

	if (kvm_clock_available == 1) {
		uint32_t version = wall_clock.version;
		msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
		if (sec != 0) {
			tmp_usec = curr_system_time / 1000; /* so many usec from start */
			*sec = wall_clock.sec + tmp_usec / 1000000;

//			ut_printf("%d: %x :%d \n",getcpuid(),vcpu_time[getcpuid()].system_time,vcpu_time[getcpuid()].system_time);
//			ut_printf("%d: sec :%d    :%x   %d:%x pa:%x oldver:%x :%x curr:%x:%d\n", test_count, *sec, *sec, wall_clock.sec,
//					wall_clock.sec, __pa(&wall_clock), version, wall_clock.version, curr_system_time, curr_system_time);
		}
		if (usec != 0)
			*usec = tmp_usec % 1000000;
	} else {
		if (sec != 0) {
			*sec = g_jiffies / 100;
		}
		if (usec != 0) {
			usec = (g_jiffies % 100) * 1000; /* actuall resoultion at the best is 10 ms*/
		}
	}
	return 1;
}
int Jcmd_clock() {
	unsigned long sec, usec, msec;

	msr_write(MSR_KVM_SYSTEM_TIME_NEW, __pa(&vcpu_time[getcpuid()]) | 1);
	ut_get_wallclock(&sec, &usec);
	msec = sec * 1000 + (usec / 1000);
	ut_printf(" msec: %d sec:%d usec:%d\n", msec, sec, usec);

	ut_printf("system time  ts :%x system time :%x:%d  jiffies :%d sec version:%x errors:%d \n", vcpu_time[getcpuid()].tsc_timestamp,
			vcpu_time[getcpuid()].system_time, vcpu_time[getcpuid()].system_time / 1000000000, g_jiffies / 100, vcpu_time[getcpuid()].version);
	ut_printf(" jiifies :%d  clock:%d errors:%d \n", g_jiffie_tick, get_kvm_clock(), g_jiffie_errors);
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
