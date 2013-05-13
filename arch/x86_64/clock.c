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
static struct pvclock_vcpu_time_info vpcu_time;

unsigned long get_kvm_clock() /* return 10ms units */
{
	return vpcu_time.system_time/10000000 ;
}
extern unsigned long g_jiffie_tick,g_jiffie_errors;
static inline int kvm_para_available(void) {

	uint32_t cpuid_ret[4], *p;
	char signature[13];

	if (1) {
		do_cpuid(KVM_CPUID_SIGNATURE, &cpuid_ret[0]);

		p = &signature[0];
		*p = cpuid_ret[1];
		p = &signature[4];
		*p = cpuid_ret[2];
		p = &signature[8];
		*p = cpuid_ret[3];

		signature[12] = 0;
		ut_printf(" KVM Clock Signature :%s: \n", signature);
		if (ut_strcmp(signature, "KVMKVMKVM") != 0)
			return 0;

		do_cpuid(KVM_CPUID_FEATURES, &cpuid_ret[0]);

		msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));

		msr_write(MSR_KVM_SYSTEM_TIME_NEW, __pa(&vpcu_time) | 1);
		g_jiffie_tick = get_kvm_clock();
		ut_printf("flags :%x\n", cpuid_ret[0]);
		return 1;
	}

	return 0;
}
int get_wallclock(unsigned long *time){
	msr_write(MSR_KVM_WALL_CLOCK_NEW, __pa(&wall_clock));
	*time = wall_clock.sec + vpcu_time.system_time/1000000000;
	return 1;
}
int Jcmd_clock() {

	ut_printf("system time  ts :%x system time :%x:%d  jiffies :%d sec version:%x errors:%d \n",
			vpcu_time.tsc_timestamp, vpcu_time.system_time,vpcu_time.system_time/1000000000, g_jiffies/100,vpcu_time.version);
	ut_printf(" jiifies :%d  clock:%d errors:%d \n",g_jiffie_tick,get_kvm_clock(),g_jiffie_errors);
	return 1;
}

int init_clock() {

	vpcu_time.system_time=0;
	if (kvm_para_available() == 0)
		return 0;

	return 0;
}
