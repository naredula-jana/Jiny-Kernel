/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * (c) Copyright 2006,2007,2008 MString Core Team <http://mstring.jarios.org>
 * (c) Copyright 2008 Tirra <tirra.newly@gmail.com>
 * (c) Copyright 2008 Dmitry Gromada <gromada82@gmail.com>
 *
 * include/eza/amd64/apic.h: implements local APIC support driver.
 *
 */

#ifndef __ARCH_APIC_H__
#define __ARCH_APIC_H__

#if 0
#include <config.h>
#include <eza/arch/types.h>
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef int int32_t;
typedef unsigned long ulong_t;
typedef unsigned long uintptr_t;
#define kprintf ut_printf
//#define kprintf dummy
#define true 1
#define false 0
#define HZ  1000 /* Timer frequency. */
#define IRQ_BASE 32 /* First vector in IDT for IRQ #0. */
#define RESERVED_IRQS 8 /* Reserved IRQ for SMP use. */
/* Maximum number of hardware IRQs in the system. */
#define NUM_IRQS  256 - IRQ_BASE - RESERVED_IRQS
#define CPU_SMP_BASE_IRQ (256 - RESERVED_IRQS)
#define LOCAL_TIMER_CPU_IRQ_VEC CPU_SMP_BASE_IRQ
#define SCHEDULER_IPI_IRQ_VEC (CPU_SMP_BASE_IRQ+1)





#define DEFAULT_APIC_BASE    0xfee00000
#define APIC_INT_EOI  0x0

/* delivery modes (TX) */
#define TXMODE_FIXED	0x0
#define TXMODE_LOWPRI	0x1
#define TXMODE_SMI	0x2
#define TXMODE_NMI	0x4
#define TXMODE_INIT	0x5
#define TXMODE_STARTUP	0x6
#define TXMODE_EXTINT	0x7

/* destination modes */
#define DMODE_LOGIC  0x1
#define DMODE_PHY    0x0

/* short hand modes */
#define SHORTHAND_NIL       0x0 /* none nobody */
#define SHORTHAND_SELF      0x1 /* self only */
#define SHORTHAND_ALLABS    0x2 /* absolutely all */
#define SHORTHAND_ALLEXS    0x3 /* all exclude ipi */

/* level modes*/
#define LEVEL_ASSERT  0x0
#define LEVEL_DEASSERT  0x1

/* trigger modes */
#define TRIG_EDGE   0x0
#define TRIG_LEVEL  0x1

struct __local_apic_timerst_t { /* LVT timers stuff */
  uint32_t count;
  uint32_t __reserved[3];
} __attribute__ ((packed));

struct __local_apic_timer_dcr_t {
  uint32_t divisor : 4, 
    __res0 : 28;
  uint32_t __reserved[3];
} __attribute__ ((packed));

typedef union {
    uint32_t reg;
    struct {
    uint32_t __reserved0 : 24,
      phy_apic_id : 4,
      __reserved1 : 4;
    } __attribute__ ((packed));
} apic_id_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t version : 8,
      __reserved0 : 8,
      max_lvt : 8,
      __reserved1 : 8;
  } __attribute__ ((packed));
} apic_version_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t priority : 8,
      __reserved0 : 24;
  } __attribute__ ((packed));
} apic_apr_t; /*arbitration priority register*/

typedef union {
  uint32_t reg;
  struct {
    uint32_t priority : 8,
      __reserved0 : 24;
  } __attribute__ ((packed));
} apic_tpr_t; /*task priority register*/

typedef union {
  uint32_t reg;
  struct {
    uint32_t priority : 8,
      __reserved0 : 24;
  } __attribute__ ((packed));
} apic_ppr_t; /*processor priority register*/

typedef union {
  uint32_t reg;
  struct {
    uint32_t __reserved0 : 28,
      mode : 4;
  } __attribute__ ((packed));
} apic_dfr_t; /*destination format register*/

typedef union {
  uint32_t reg;
  struct {
    uint32_t __reserved0 : 24,
      log_dest : 8;
  } __attribute__ ((packed));
} apic_ldr_t; /*logical destination register*/

typedef union {
  uint32_t reg;
  struct {
    uint32_t spurious_vector : 8,
      apic_enabled : 1,
      cpu_focus : 1,
      __reserved0 : 22;
  } __attribute__ ((packed));
} apic_svr_t;

typedef union {  
  uint32_t reg;
  struct {
      unsigned tx_cs_err : 1,
       rx_cs_err : 1,
       tx_accept_err : 1,
       rx_accept_err : 1,
       __res0 : 1,
       tx_illegal_vector : 1,
       rx_illegal_vector : 1,
      reg_illegal_addr : 1,
      __res1 : 24;
  } __attribute__ ((packed));
} apic_esr_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t vector : 8,
      tx_mode : 3,
      rx_mode: 1,
      tx_status: 1,
      __res0: 1,
      level: 1,
      trigger: 1,
      __res1: 2,
      shorthand: 2,
      __res2: 12;
  } __attribute__ ((packed));
} apic_icr1_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t __res0 : 24,
      dest : 8;
  } __attribute__ ((packed));
} apic_icr2_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t vector : 8,
      __res0 : 4,
      tx_status : 1,
      __res1 : 3,
      mask : 1,
      timer_mode : 1,
      __res2 : 14;
  } __attribute__ ((packed));
} apic_lvt_timer_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t vector : 8,
      tx_mode : 3,
      __res0 : 1,
      tx_status : 1,
      __res1 : 3,
      mask : 1,
      __res2 : 15;
  } __attribute__ ((packed));
} apic_thermal_sensor_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t vector : 8,
      tx_mode : 3,
      __res0 : 1,
      tx_status : 1,
      __res1 : 3,
      mask : 1,
      __res2 : 15;
  } __attribute__ ((packed));
} apic_lvt_pc_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t vector : 8,
      tx_mode : 3,
      __res0 : 1,
      tx_status : 1,
      polarity : 1,
      remote_irr : 1,
      trigger : 1,
      mask : 1,
      __res1 : 15;
  } __attribute__ ((packed));
} apic_lvt_lint_t;

typedef union {
	uint32_t reg;
	struct {
		uint32_t vector : 8,
			tx_mode : 3,
			__res0 : 1,
			tx_status : 1,
			__res1 : 3,
			mask : 1,
			__res2 : 15;
	} __attribute__ ((packed));
} apic_cmci_lvt_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t vector : 8,
      tx_mode: 3,
      __res0 : 1,
      tx_status : 1,
      __res1 : 3,
      mask : 1,
      __res2 : 15;
  } __attribute__ ((packed));
} apic_lvt_error_t;

typedef union {
  uint32_t reg;
  struct {
    uint32_t divisor : 4, 
      __res0 : 28;
  } __attribute__ ((packed));
} apic_timer_dcr_t;

struct __local_apic_t {
  struct { uint32_t __reserved0[4];} __fuck01; /* 16 bytes */  /* |000->|010 */
  struct { uint32_t __reserved1[4];} __34r34f;
  apic_id_t id;   /* |020 */
  struct {uint32_t __reserved[3];} __f34fff0h;
  const apic_version_t version; /* |030 apic version register*/
  struct {uint32_t __reserved[3];} __f34fff000h;
  struct { uint32_t __reserved2[4];} __7263er; /* 16 bytes */  /* |040->|070 */
  struct { uint32_t __reserved2[4];} __7263er345; /* 16 bytes */
  struct { uint32_t __reserved2[4];} __7263er23; /* 16 bytes */
  struct { uint32_t __reserved2[4];} __7263er870; /* 16 bytes */
  apic_tpr_t tpr;
  struct {uint32_t __reserved[3];} __f34fff00h;   /* |080 task priority register */
  const apic_apr_t apr;
  struct {uint32_t __reserved[3];} __f34fffa0h;  /* |090 */
  apic_ppr_t ppr;
  struct {uint32_t __reserved[3];} __f34fffa1h;  /* |0A0 processor priority register */
  struct  { /* end of interrupt */  /* |0B0 */
    uint32_t eoi;    uint32_t __reserved[3];
  } eoi;
  struct { uint32_t __reserved6[4];} __d34dggf87er; /* 16 bytes */  /* |0C0 */
  apic_ldr_t ldr; /* |0D0 logical destination register */
  struct {uint32_t __reserved[3];} __f34fffa4h;
  apic_dfr_t dfr; /* |0E0 destination format register */
  struct {uint32_t __reserved[3];} __f34fffa2h;
  apic_svr_t svr; /* |0F0 spurious interrupt vector register */
  struct {uint32_t __reserved[3];} __f34fffa3h;
  struct  { /* in service register */  /* |100->|170 (8 items) */
    uint32_t bits;    uint32_t __reserved[3];
  } isr[8];
  struct  { /* trigger mode register */  /* |180->|1F0 (8 items) */
    uint32_t bits;    uint32_t __reserved[3];
  } tmr[8];
  struct  { /* interrupt request register */  /* |200->|270 (8 items) */
    uint32_t bits;    uint32_t __reserved[3];
  } irr[8];
  /* |280 error status register */
  apic_esr_t esr;  /* |280 error status register */ struct {uint32_t __reserved[3];} __f34fffa6h;
  struct { uint32_t __reserved7[4];} __d34d34d34; /* |290->|2F0 */ /* 16 bytes */
  struct { uint32_t __reserved7[4];} __d34d34d3423; /* 16 bytes */
  struct { uint32_t __reserved7[4];} __d34d34d345; /* 16 bytes */
  struct { uint32_t __reserved7[4];} __d34d34d34234; /* 16 bytes */
  struct { uint32_t __reserved7[4];} __d34d34d34f; /* 16 bytes */
  struct { uint32_t __reserved7[4];} __d34d34d34df; /* 16 bytes */
  /* |2F0 LFT for overflow condition of corrected machine check error; intel specific  */ 
  apic_cmci_lvt_t cmci_lvt; 
  struct { uint32_t __reserved7[3];} __d34d34d34ef; /* 12 bytes */
  apic_icr1_t icr1; /* |300 interrupt command register */
  struct {uint32_t __reserved[3];} __f34fffafh;
  apic_icr2_t icr2;  /*310 interrupt command register - destination */
  struct {uint32_t __reserved[3];} __f34fffad33h;
  apic_lvt_timer_t lvt_timer;   /* |320 lvt timer parameters */ 
  struct {uint32_t __reserved[3];} __f34fffad31h;
  apic_thermal_sensor_t lvt_thermal_sensor; /* |330 */ 
  struct {uint32_t __reserved[3];} __f34fffad35h;
  apic_lvt_pc_t lvt_pc;   /* |340 LVT performance counter */ 
  struct {uint32_t __reserved[3];} __f34fffad32h;
  apic_lvt_lint_t lvt_lint0;   /* |350 LVT logical int0 */ 
  struct {uint32_t __reserved[3];} __f34fffad36h;
  apic_lvt_lint_t lvt_lint1;   /* |360 LVT logical int1 */ 
  struct {uint32_t __reserved[3];} __f34fffad37h;
  apic_lvt_error_t lvt_error;  /* |370 LVT error register */ 
  struct {uint32_t __reserved[3];} __f34fffad38h;
  struct  { /* |380 LVT timers stuff */
    uint32_t count;
    uint32_t __reserved[3];
  } timer_icr;
  const struct  { /* |390 LVT timers stuff */
    uint32_t count;
    uint32_t __reserved[3];
  } timer_ccr;
  /* |3A0->|3D0 */
  struct {uint32_t __reserved14[4];} __erferf45f; /* 16 bytes */  /* |3A0->|3D0 */  
  struct {uint32_t __reserved14[4];} __erferf45werf; /* 16 bytes */
  struct {uint32_t __reserved14[4];} __erferf4wer5f; /* 16 bytes */  
  struct {uint32_t __reserved14[4];} __erferwerf45f; /* 16 bytes */
  apic_timer_dcr_t timer_dcr;  /* |3E0 timer divide configuration */ 
  struct {uint32_t __reserved[3];} __f34fffad39h;
  /* |3F0 */
  struct {uint32_t __reserved18[4];} __erferferf45654fedf; /* 16 bytes */

} __attribute__ ((packed));

/*uffh, functions*/
int local_bsp_apic_init(void);
void local_apic_bsp_switch(void);
uint32_t get_local_apic_id(void);
void local_apic_timer_init(uint8_t vector);
extern void fake_apic_init(void);
void local_apic_send_eoi(void);
void apic_timer_hack(void);

/*
 * Disable handling of some interrupts by setting the task priority
 * if 'prio' is not null the vectors from 0 upto prio*16 will be masked
 */
void apic_set_task_priority(uint8_t prio);

#ifdef CONFIG_SMP

int apic_send_ipi_init(int cpu);
int apic_send_ipi_vector(int cpu, uint8_t vector);
int apic_broadcast_ipi_vector(uint8_t vector);
int local_ap_apic_init(void);

#endif /* CONFIG_SMP */
static void inline outb(uint16_t port,uint8_t vl)
{
  __asm__ volatile ("outb %0, %1\n" : : "a" (vl), "Nd" (port));
}
#endif
