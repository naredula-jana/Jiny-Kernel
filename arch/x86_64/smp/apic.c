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
 * eza/amd64/apic.c: implements local APIC support driver.
 *
 * Modified by Naredula Janardhana Reddy while porting to Jiny
 *
 */

#include "apic.h"

void dummy(const char *format, ...){

}

static volatile struct __local_apic_t *local_apic=0;
volatile uint32_t local_apic_base = DEFAULT_APIC_BASE;
volatile uint8_t local_apic_ids[100];
static int __apics_number;

extern unsigned long imps_lapic_addr;
static int __map_apic_page(void)
{
  local_apic = (struct __local_apic_t *)imps_lapic_addr;
  return 0;
}


/*
 * default functions to access APIC (local APIC)
 * I think that gcc can try to make optimization on it
 * to avoid I'm stay `volatile` flag here.
 */
static inline uint32_t __apic_read(ulong_t rv)
{
    return *((volatile uint32_t *)((ulong_t)local_apic+rv));
}

static inline void __apic_write(ulong_t rv,uint32_t val)
{
    *((volatile uint32_t *)((ulong_t)local_apic+rv))=val;
}

static uint32_t __get_maxlvt(void)
{
  apic_version_t version=local_apic->version;

  return version.max_lvt;
}

static void __set_lvt_lint_vector(uint32_t lint_num,uint32_t vector)
{
  apic_lvt_lint_t lvt_lint;

  if(!lint_num) {
    lvt_lint=local_apic->lvt_lint0;
    lvt_lint.vector=vector;
    local_apic->lvt_lint0.reg=lvt_lint.reg;
  }  else {
    lvt_lint=local_apic->lvt_lint1;
    lvt_lint.vector=vector;
    local_apic->lvt_lint1.reg=lvt_lint.reg;
  }
}
int read_apic_isr(int isr){
	int i=isr/32;
	if (local_apic==0){
		if (isr >= 40)
		{
			// Send reset signal to slave.
			outb(0xA0, 0x20);
		}
		// Send reset signal to master. (As well as slave, if necessary).
		outb(0x20, 0x20);
		return 0;
	}
	return local_apic->isr[i].bits;
}
 void __enable_apic(void)
{
  apic_svr_t svr=local_apic->svr;

  svr.apic_enabled=0x1;
  svr.cpu_focus=0x1;
  local_apic->svr.reg=svr.reg;
}

static void __disable_apic(void)
{
  apic_svr_t svr=local_apic->svr;

  svr.apic_enabled=0x0;
  svr.cpu_focus=0x0;
  local_apic->svr.reg=svr.reg;
}

void apic_shootout(void)
{
  __disable_apic();
}

void apic_set_task_priority(uint8_t prio)
{
	local_apic->tpr.reg = prio; 
}



static void __local_apic_clear(void)
{
  uint32_t max_lvt;
  uint32_t v;
  apic_lvt_error_t lvt_error=local_apic->lvt_error;
  apic_lvt_timer_t lvt_timer=local_apic->lvt_timer;
  apic_lvt_lint_t lvt_lint=local_apic->lvt_lint0;
  apic_lvt_pc_t lvt_pc=local_apic->lvt_pc;

  max_lvt=__get_maxlvt();

  if(max_lvt>=3) {
    v=0xfe;
    lvt_error.vector=v;
    lvt_error.mask |= (1 << 0);
    local_apic->lvt_error.reg=lvt_error.reg;
  }

  /* mask timer and LVTs*/
  lvt_timer.mask = 0x1;
  local_apic->lvt_timer.reg=lvt_timer.reg;
  lvt_lint.mask=0x1;
  local_apic->lvt_lint0.reg = lvt_lint.reg;
  lvt_lint=local_apic->lvt_lint1;
  lvt_lint.mask=0x1;
  local_apic->lvt_lint1.reg = lvt_lint.reg;

  if(max_lvt>=4) {
    lvt_pc.mask = 0x1;
    local_apic->lvt_pc.reg=lvt_pc.reg;
  } 
}

static int __local_apic_chkerr(void)
{
  apic_esr_t esr;
  int i=0;

  esr=local_apic->esr;

  if (esr.rx_cs_err) { i++; kprintf("[LA] Receive checksum failed.\n"); }
  if (esr.tx_accept_err) { i++; kprintf("[LA] Transfer failed.\n"); }
  if (esr.rx_accept_err) { i++; kprintf("[LA] IPI is not accepted by any CPU.\n"); }
  if (esr.tx_illegal_vector) { i++; kprintf("[LA] Illegal transfer vector.\n"); }
  if (esr.rx_illegal_vector) { kprintf("[LA] Illegal receive vector.\n"); }
  if (esr.reg_illegal_addr){
	     i++;
	      //while(1);
	      kprintf("APIC: ERROR Illegal register address.\n"); }
  if (i>0){
	  while(1);
  }

  return i;
}
int local_apic_chkerr(){
	__local_apic_chkerr();
	return 1;
}
uint32_t get_apic_version(void)
{
	if (local_apic==0)  while(1);
  apic_version_t version=local_apic->version;
  return version.version;
}

static int __local_apic_check(void)
{
  uint32_t v0;

  /* version check */
  v0=get_apic_version();
  v0=(v0)&0xffu;
  if(v0==0x0 || v0==0xff)
    return -1;

  /*check for lvt*/
  v0=__get_maxlvt();
  if(v0<0x02 || v0==0xff)
		return -1;

  return 0;
}

void local_apic_send_eoi(void)
{
  local_apic->eoi.eoi=APIC_INT_EOI;
}

uint32_t get_local_apic_id(void)
{
  apic_id_t version=local_apic->id;

  return version.phy_apic_id;
}

void set_apic_spurious_vector(uint32_t vector)
{
  apic_svr_t svr=local_apic->svr;
  svr.spurious_vector=vector;
  local_apic->svr.reg=svr.reg;
}

void set_apic_dfr_mode(uint32_t mode)
{
  apic_dfr_t dfr=local_apic->dfr;
  dfr.mode=mode;
  local_apic->dfr.reg=dfr.reg;
}

void set_apic_ldr_logdest(uint32_t dest)
{
  apic_ldr_t ldr=local_apic->ldr;
  ldr.log_dest=dest;
  local_apic->ldr.reg=ldr.reg;
}

static inline void enable_l_apic_in_msr()
{
  __asm__ volatile (
		    "movq $0x1b, %%rcx\n"
		    "rdmsr\n"
		    "orq $(1<<11),%%rax\n"
		    "wrmsr\n"
		    :
		    :
		    :"%rax","%rcx","%rdx"
		    );
}

extern void io_apic_disable_all(void);
extern void io_apic_bsp_init(void);
extern void io_apic_enable_irq(uint32_t virq);
extern void io_apic_enable_all();
#define bool char
static int __local_apic_init(bool msgout)
{
  uint32_t v;
  int i=0,l;
  apic_lvt_lint_t lvt_lint;
  apic_icr1_t icr1;

	if (msgout) {
		kprintf("APIC: Checking APIC is present ... ");
		if(__local_apic_check()<0) {
			kprintf("FAIL\n");
			return -1;
		} else{
			kprintf("OK\n");
		}
	}
	
  enable_l_apic_in_msr(); 

  v=get_apic_version();
	if (msgout)
		kprintf("APIC: APIC version: %d apic id:%x\n",v,get_local_apic_id());

  /* first we're need to clear APIC to avoid magical results */
  __local_apic_clear();
  __disable_apic();

  set_apic_dfr_mode(0xf); 
  set_apic_ldr_logdest(1 << (__apics_number & 7));
	__apics_number++;

  apic_set_task_priority(0);

  /* clear bits for interrupts - can be filled up to other os */
  for(i=7;i>=0;i--){
    v=local_apic->isr[i].bits;
    for(l=31;l>=0;l--)
      if(v & (1 << l))
    local_apic_send_eoi();
  }

  set_apic_spurious_vector(0xff); 
  __enable_apic();  

  /* set nil vectors */
  __set_lvt_lint_vector(0,0x34);
  __set_lvt_lint_vector(1,0x35);
  /*set mode#7 extINT for lint0*/
  lvt_lint=local_apic->lvt_lint0;
  lvt_lint.tx_mode=0x7;
  lvt_lint.mask=1;
  lvt_lint.tx_status = 0x0;
  lvt_lint.polarity = LEVEL_DEASSERT; 
  lvt_lint.trigger = TRIG_EDGE;
  local_apic->lvt_lint0.reg=lvt_lint.reg;
  /*set mode#4 NMI for lint1*/
  lvt_lint=local_apic->lvt_lint1;
  lvt_lint.tx_mode=0x4;
  lvt_lint.mask=1;
  lvt_lint.tx_status = 0x0;
  lvt_lint.polarity = LEVEL_DEASSERT; 
  lvt_lint.trigger = TRIG_EDGE;
  local_apic->lvt_lint1.reg=lvt_lint.reg;

  /* ok, now we're need to set esr vector to 0xfe */
  local_apic->lvt_error.vector = 0xfe;

  /*enable to receive errors*/
  if(__get_maxlvt()>3)
    local_apic->esr.tx_cs_err = 0x0;

  /* set icr1 registers*/
  icr1=local_apic->icr1;
  icr1.tx_mode=TXMODE_INIT;
  icr1.rx_mode=DMODE_PHY;
  icr1.level=0x0;
  icr1.shorthand=0x2;
  icr1.trigger=0x1;
  local_apic->icr1.reg=icr1.reg;

  // set internal apic error interrupt vector
  *(uint32_t*)((uint64_t)local_apic + 0x370) = 200;

	return 0;
}


static void __unmask_extint(void)
{
	apic_lvt_lint_t lvt_lint;	

	lvt_lint = local_apic->lvt_lint0;
	lvt_lint.mask = 0;
	local_apic->lvt_lint0.reg = lvt_lint.reg;

	lvt_lint = local_apic->lvt_lint1;
	lvt_lint.mask = 0;
	local_apic->lvt_lint1.reg = lvt_lint.reg;
	__local_apic_chkerr();
}



void local_apic_bsp_switch(void)
{
  kprintf("APIC: Leaving PIC mode to APIC mode ... ");
  outb(0x22,0x70);
  outb(0x23,0x01); /* old port - 0x71,0x23 */

  kprintf("APIC: OK\n");
}

/* APIC timer implementation */
void local_apic_timer_enable(void)
{
  apic_lvt_timer_t lvt_timer=local_apic->lvt_timer;

  lvt_timer.mask=0x0;
  local_apic->lvt_timer.reg=lvt_timer.reg;
}

void local_apic_timer_disable(void)
{
  apic_lvt_timer_t lvt_timer=local_apic->lvt_timer;

  lvt_timer.mask=0x1;
  local_apic->lvt_timer.reg=lvt_timer.reg;
}

static void __local_apic_timer_calibrate(uint32_t x)
{
  apic_timer_dcr_t timer_dcr=local_apic->timer_dcr;

  switch(x) {
  case 1:    timer_dcr.divisor=0xb;    break;
  case 2:    timer_dcr.divisor=0x0;    break;
  case 4:    timer_dcr.divisor=0x1;    break;
  case 8:    timer_dcr.divisor=0x2;    break;
  case 16:    timer_dcr.divisor=0x3;    break;
  case 32:    timer_dcr.divisor=0x8;    break;
  case 64:    timer_dcr.divisor=0x9;    break;
  case 128:    timer_dcr.divisor=0xa;    break;
  default:    return;
  }
  local_apic->timer_dcr.reg=timer_dcr.reg;
}
static uint32_t delay_loop;
int g_conf_clock_scale=1; //default is 1

void local_apic_timer_calibrate(uint32_t hz)
{
  uint32_t x1,x2;
  local_apic_timer_disable();

  kprintf("APIC: Calibrating lapic delay_loop ...");

  x1=local_apic->timer_ccr.count; /*get current counter*/
  local_apic->timer_icr.count=0xffffffff; /*fillful initial counter */

  kprintf("APIC: ccr %ld\n",local_apic->timer_ccr.count);

  while(local_apic->timer_icr.count==x1) /* wait while cycle will be end */ {
        kprintf("APIC: %ld\n",local_apic->timer_icr.count);
    
  }

  x1=local_apic->timer_ccr.count; /*get current counter*/
  //atom_usleep(1000000/hz); /*delay*/
  udelay(g_conf_clock_scale*4000000/hz);
  x2=local_apic->timer_ccr.count; /*again get current counter to see difference*/

  delay_loop=x1-x2;
  //delay_loop=11931;
  //delay_loop=67489;
  //delay_loop=600;
  kprintf("APIC: delay loop: %d \n",delay_loop);

  /*ok, let's write a difference to icr*/
  local_apic->timer_icr.count=delay_loop; /* <-- this will tell us how much ticks we're really need */
}

void local_apic_timer_ap_calibrate(void)
{
  local_apic->timer_icr.count=delay_loop;   
}


extern void i8254_suspend(void);

void apic_timer_hack(void)
{
  local_apic->timer_icr.count=delay_loop;
}

#define I8254_BASE  0x40
void i8254_suspend(void)
{
  outb(I8254_BASE+3,0x30);
  outb(I8254_BASE,0xff);
  outb(I8254_BASE,0xff);
}
extern void timer_callback(unsigned long  regs);
void local_apic_timer_init(uint8_t vector)
{
  apic_lvt_timer_t lvt_timer=local_apic->lvt_timer;

  i8254_suspend(); /* suspend general intel timer - bye bye, simple and pretty one, welcome to apic ...*/

  /* calibrate timer delimeter */
  __local_apic_timer_calibrate(16);
  /*calibrate to hz*/
  local_apic_timer_calibrate(1000);
  /* setup timer vector  */
  lvt_timer.vector=vector; 
  /* set periodic mode (set bit to 1) */
  lvt_timer.timer_mode = 0x1;
  /* enable timer */
  lvt_timer.mask=0x0;
  local_apic->lvt_timer.reg=lvt_timer.reg;
  ar_registerInterrupt(LOCAL_TIMER_CPU_IRQ_VEC, &timer_callback, "APICtimer");
}

void local_apic_timer_ap_init(uint8_t vector)
{
  apic_lvt_timer_t lvt_timer=local_apic->lvt_timer;

  i8254_suspend(); /* suspend general intel timer - bye bye, simple and pretty one, welcome to apic ...*/

  /* calibrate timer delimeter */
   __local_apic_timer_calibrate(16);
  /*calibrate to hz*/
  local_apic_timer_ap_calibrate();
  /* setup timer vector  */
  lvt_timer.vector=vector; 
  /* set periodic mode (set bit to 1) */
  lvt_timer.timer_mode = 0x1;
  /* enable timer */
  lvt_timer.mask=0x0;
  local_apic->lvt_timer.reg=lvt_timer.reg;
}
extern unsigned char imps_cpu_apic_map[];
int apic_send_ipi_vector(int cpu, uint8_t vector)
{
    int ret = 0;

  apic_icr1_t icr1;
    apic_icr2_t icr2;

    do {
        icr1=local_apic->icr1;
    } while (icr1.tx_status);

  icr1.tx_mode=TXMODE_FIXED;
  icr1.rx_mode=DMODE_PHY;
  icr1.level=LEVEL_DEASSERT;
  icr1.trigger=TRIG_EDGE; /* trigger mode -> edge */
  icr1.shorthand=SHORTHAND_NIL;
  icr1.vector=vector;
    icr2.dest = imps_cpu_apic_map[cpu];
    local_apic->icr2.reg=icr2.reg;
  local_apic->icr1.reg=icr1.reg;

  if (__local_apic_chkerr())
        ret = -1;

    return ret;
}
int apic_broadcast_ipi_vector(uint8_t vector)
{
    int ret = 0;

  apic_icr1_t icr1=local_apic->icr1;

  icr1.tx_mode=TXMODE_FIXED;
  icr1.rx_mode=DMODE_PHY;
  icr1.level=LEVEL_DEASSERT;
  icr1.trigger=TRIG_EDGE; /* trigger mode -> edge */
  icr1.shorthand=SHORTHAND_ALLEXS; /* all exclude ipi */
  icr1.vector=vector;
  local_apic->icr1.reg=icr1.reg;

  if (__local_apic_chkerr())
        ret = -1;

    return ret;
}
static int ready_for_broadcast=0;
void broadcast_msg(){
#if 0
	if (ready_for_broadcast==0) return;
	apic_send_ipi_vector(1,5);
	apic_broadcast_ipi_vector(249);
	return;
#endif
}
int enable_ioapic(){

	local_apic_timer_ap_init(LOCAL_TIMER_CPU_IRQ_VEC);
	return 1;
}
int local_ap_apic_init(void)
{
	if (__local_apic_init(false)){

		//return -1;
	}

	enable_ioapic();
	__unmask_extint();
	ready_for_broadcast=1;
	return 0;
}
/*init functions makes me happy*/
int local_bsp_apic_init(void)
{
	__map_apic_page();

	if (__local_apic_init(true)){
		 //return -1;
	}

	local_apic_timer_init(LOCAL_TIMER_CPU_IRQ_VEC);

	__unmask_extint();

  return 0;
}

/********************************** ELASTIC CPU ****************************/
void apic_disable_partially(){// TODO sometimes cpu stop recving the interrupt interrupt

	/* 1. disable all external interrupt except timer */
	  apic_lvt_lint_t lvt_lint=local_apic->lvt_lint0;
	  lvt_lint.mask=0x1;
	  local_apic->lvt_lint0.reg = lvt_lint.reg;

	  lvt_lint=local_apic->lvt_lint1;
	  lvt_lint.mask=0x1;
	  local_apic->lvt_lint1.reg = lvt_lint.reg;

	/* 2. set the timer so that interrupts happens with very less frequency */
	  local_apic_timer_disable();
	  local_apic->timer_icr.count=delay_loop*100;  // recevies timer interrupt 1 per second instead of 100 per second
	  local_apic_timer_enable();
}
void apic_reenable(){// TODO : sometimes cpu stop recving the interrupt interrupt
	/* 1. enable all external interrupt except timer */
	  apic_lvt_lint_t lvt_lint=local_apic->lvt_lint0;
	  lvt_lint.mask=0x0;
	  local_apic->lvt_lint0.reg = lvt_lint.reg;

	  lvt_lint=local_apic->lvt_lint1;
	  lvt_lint.mask=0x0;
	  local_apic->lvt_lint1.reg = lvt_lint.reg;

	/* 2. set the timer to the defualt frequency frequency */
	  local_apic_timer_disable();
	  local_apic->timer_icr.count=delay_loop;
	  local_apic_timer_enable();
}

