/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/isr.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
//#include "task.h"
#include "common.h"
#include "isr.h"
#include "mach_dep.h"
enum fault_idx {
  FLT_DE  = 0, /* Divide-by-Zero-Error */
  FLT_DB  = 1, /* Debug */
  FLT_NMI = 2, /* Non-Maskable-Interrupt */
  FLT_BP  = 3, /* Breakpoint */
  FLT_OF  = 4, /* Overflow */
  FLT_BR  = 5, /* Bound-range */
  FLT_UD  = 6, /* Invalid-Opcode */
  FLT_NM  = 7, /* Device-Not-Available */
  FLT_DF  = 8, /* Double fault */
  /* #9 is reserved */
  FLT_TS = 10, /* Invalid TSS */
  FLT_NP = 11, /* Segment-Not-Present */
  FLT_SS = 12, /* SS register loads and stack references */
  FLT_GP = 13, /* General-Protection */
  FLT_PF = 14, /* Page-Fault */
  /* #15 is reserved. */
  FLT_MF = 16, /* x87 Floating-Point Exception */
  FLT_AC = 17, /* Alignment-Check */
  FLT_MC = 18, /* Machine-Check */
  FLT_XF = 19, /* SIMD Floating-Point */
  /* #20 - 29 are reserved. */
  FLT_SX = 30, /* Security Exception */
  /* #31 is reserved */
};

uint32_t faults_with_errcode =
  (POW2(FLT_DF) | POW2(FLT_TS) | POW2(FLT_NP) |
   POW2(FLT_SS) | POW2(FLT_GP) | POW2(FLT_PF) |
   POW2(FLT_AC));
struct irq_handler_t {
	isr_t action;
	unsigned char *name;
	struct {
		long num_irqs;
		long num_error;
	} stat[MAX_CPUS];
	void *private_data;
};
irq_cpustat_t irq_stat[NR_CPUS] ;

struct irq_handler_t g_interrupt_handlers[MAX_IRQS];

extern void ar_pageFault(struct fault_ctx *ctx);
extern int getcpuid();
long fault_ip_g=0;
long fault_error_g=0;
long fault_num_g=0;
struct fault_ctx cpu_ctx;
static int gpFault(struct fault_ctx *ctx)
{
	fault_ip_g=ctx->istack_frame->rip;
	fault_error_g=ctx->errcode;
	fault_num_g=ctx->fault_num;

	ut_printf(" ERROR: cpuid:%d Gp Fault fault ip:%x error code:%x sp:%x fault number:%x \n",getcpuid(),fault_ip_g,fault_error_g,ctx->istack_frame->rsp,fault_num_g);
        if (g_current_task->mm != g_kernel_mm) /* user level thread */
        {
        	   while(1);
                SYS_sc_exit(100);
                return 0;
        }
	/*ut_printf("GP fault:  ip: %x  error:%x  fault:%x cs:%x ss:%x \n",ctx->istack_frame->rip,ctx->errcode,ctx->fault_num,ctx->istack_frame->cs,ctx->istack_frame->ss);
	  ut_printf("GP fault:  ip: %x  error:%x  fault:%x cs:%x ss:%x \n",ctx->istack_frame->rip,ctx->errcode,ctx->fault_num,ctx->istack_frame->cs,ctx->istack_frame->ss);
	  show_trace(&i); */
	while(1);
}

void init_handlers()
{
	int i,j;

	for (i=0; i<MAX_IRQS;i++)
	{
		g_interrupt_handlers[i].action=0;
		g_interrupt_handlers[i].name=0;
		for (j = 0; j < MAX_CPUS; j++) {
			g_interrupt_handlers[i].stat[j].num_irqs = 0;
			g_interrupt_handlers[i].stat[j].num_error = 0;
		}
	}
	for (i=0; i<32;i++)
		ar_registerInterrupt(i, gpFault,"gpfault", NULL);

	ar_registerInterrupt(14, ar_pageFault,"pagefault", NULL);
	ar_registerInterrupt(13, gpFault,"gpfault", NULL);
}

int Jcmd_irq_stat(char *arg1,char *arg2)
{
	int i, j;

	ut_printf("         ");
	for (j = 0; (j < MAX_CPUS) && (j < getmaxcpus()); j++) {
		ut_printf("CPU%d        ", j);
	}
	ut_printf("\n");
	for (i = 0; i < MAX_IRQS; i++) {
		if (g_interrupt_handlers[i].action == 0
				&& g_interrupt_handlers[i].stat[j].num_error == 0)
			continue;
#if 1
		if (i < 32 && g_interrupt_handlers[i].action == gpFault)
			continue;
#endif
		ut_printf(" %3d: ",i);
		for (j = 0; (j < MAX_CPUS) && (j < getmaxcpus()); j++) {
			ut_printf("[%8d %3d] ",g_interrupt_handlers[i].stat[j].num_irqs,g_interrupt_handlers[i].stat[j].num_error);
		}
		ut_printf(":%s \n",g_interrupt_handlers[i].name);
	}

	return 1;
}

void ar_registerInterrupt(uint8_t n, isr_t handler,char *name, void *data)
{
	g_interrupt_handlers[n].action = handler;
	g_interrupt_handlers[n].name=(unsigned char *)name;
	g_interrupt_handlers[n].private_data = data;
}

void DisableTimer(void)
{
	outb(0x21, inb(0x21) | 1);
}
static void fill_fault_context(struct fault_ctx *fctx, void *rsp,
		int fault_num)
{
	uint8_t *p = rsp;

	/* Save fault index */
	fctx->fault_num = fault_num;

	/* Save pointer to purpose registers [%rax .. %r15] */
	fctx->gprs = (struct gpregs *)p;
	p += sizeof(struct gpregs);

	fctx->errcode = *(uint32_t *)p;
	p += 8;

	/* Save pointer to interrupt stack frame */
	fctx->istack_frame = (struct intr_stack_frame *)p;
	p += sizeof(struct intr_stack_frame);

	/* An address RSP pointed to berfore fault occured. */
	fctx->old_rsp = p;
	cpu_ctx.gprs=fctx->gprs;
	cpu_ctx.istack_frame=fctx->istack_frame ;
}
static int stack_depth=0;
// This gets called from our ASM interrupt handler stub.
int ar_faultHandler(void *p, unsigned int  int_no)
{
	struct fault_ctx ctx;

	asm volatile("cli"); /* SOME BAD happened STOP all the interrupts */
	fill_fault_context(&ctx,p,int_no);
	if (g_interrupt_handlers[int_no].action != 0)
	{
		int ret;

		stack_depth++;
		isr_t handler = g_interrupt_handlers[int_no].action;
		ret = handler(&ctx);
		g_interrupt_handlers[int_no].stat[getcpuid()].num_irqs++;

		stack_depth--;
		return ret; /* return properly for page fault or debug fault or trap fault */
	}else
	{
		g_interrupt_handlers[int_no].stat[getcpuid()].num_error++;
		ut_printf("UNhandled FAULT ..: %d \n",int_no);
	}
	while(1);
}
extern void do_softirq();
// This gets called from our ASM interrupt handler stub.
#define I8259_PIC_MASTER    0x20
#define I8259_PIC_SLAVE     0xA0
#define PIC_EOI             0x20
#if 0
static void i8259a_mask_irq(unsigned int irq)
{
  uint8_t mask;

  if(irq < 0x08) {
    mask = inb(I8259_PIC_MASTER + 1);
    mask |= (1 << irq);
    outb(I8259_PIC_MASTER + 1, mask);
  }
  else { /*located in PIC1*/
    mask = inb(I8259_PIC_SLAVE + 1);
    mask |= (1 << (irq-0x08));
    outb(I8259_PIC_SLAVE + 1, mask);
  }
}
#endif
void ar_irqHandler(void *p,unsigned int int_no)
{

	if (int_no > 100) { // APIC or MSI based interrupts
		int isr_status= read_apic_isr(int_no); // TODO: make use of isr_status
	} else {
		if (int_no >= 40) {
			// Send reset signal to slave.
			outb(0xA0, 0x20);
		}
		// Send reset signal to master. (As well as slave, if necessary).
		outb(0x20, 0x20);
	}

	if (g_interrupt_handlers[int_no].action != 0)
	{
		isr_t handler = g_interrupt_handlers[int_no].action;
		if (int_no == 128)
		{
			struct fault_ctx ctx;

			fill_fault_context(&ctx,p,int_no);
			handler(&ctx, g_interrupt_handlers[int_no].private_data);
		}else
		{
			handler(g_interrupt_handlers[int_no].private_data);
		}
		g_interrupt_handlers[int_no].stat[getcpuid()].num_irqs++;


#ifdef SMP
	local_apic_send_eoi();
#endif
	}else
	{
		g_interrupt_handlers[int_no].stat[getcpuid()].num_error++;
		if (int_no != 32)	 {
			//ut_printf("UNhandled interrupt ..: %d \n",int_no);
		}
#ifdef SMP
	local_apic_send_eoi();
#endif
	}
	//start_debug();
	//do_softirq();
}
extern void timer_callback(registers_t regs);
void init_timer() {
	addr_t frequency = 100;
	// Firstly, register our timer callback.
	ar_registerInterrupt(32, &timer_callback, "timer_callback", NULL);

	// The value we send to the PIT is the value to divide it's input clock
	// (1193180 Hz) by, to get our required frequency. Important to note is
	// that the divisor must be small enough to fit into 16-bits.
	addr_t divisor = 1193180 / frequency;

	// Send the command byte.
	outb(0x43, 0x36);

	// Divisor has to be sent byte-wise, so split here into upper/lower bytes.
	uint8_t l = (uint8_t) (divisor & 0xFF);
	uint8_t h = (uint8_t) ((divisor >> 8) & 0xFF);

	// Send the frequency divisor.
	outb(0x40, l);
	outb(0x40, h);
}


