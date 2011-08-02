#include "task.h"
#include "common.h"
#include "isr.h"
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
//addr_t dummy_data[124596]={0xabcd}; /* TODO : REMOVE me later this to place data in seprate area */
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
	} stat;
};
irq_cpustat_t irq_stat[NR_CPUS] ;
struct irq_handler_t g_interrupt_handlers[256];

extern void ar_pageFault(struct fault_ctx *ctx);
long fault_ip_g=0;
long fault_error_g=0;
long fault_num_g=0;
static void gpFault(struct fault_ctx *ctx)
{

	fault_ip_g=ctx->istack_frame->rip;
	fault_error_g=ctx->errcode;
	fault_num_g=ctx->fault_num;


	ut_printf(" ERROR: Gp Fault fault ip:%x error code:%x sp:%x fault number:%x \n",fault_ip_g,fault_error_g,ctx->istack_frame->rsp,fault_num_g);
        if (g_current_task->mm != g_kernel_mm) /* user level thread */
        {
                SYS_sc_exit(1);
                return;
        }
	/*ut_printf("GP fault:  ip: %x  error:%x  fault:%x cs:%x ss:%x \n",ctx->istack_frame->rip,ctx->errcode,ctx->fault_num,ctx->istack_frame->cs,ctx->istack_frame->ss);
	  ut_printf("GP fault:  ip: %x  error:%x  fault:%x cs:%x ss:%x \n",ctx->istack_frame->rip,ctx->errcode,ctx->fault_num,ctx->istack_frame->cs,ctx->istack_frame->ss);
	  show_trace(&i); */
	while(1);
}

void init_handlers()
{
	int i;

	for (i=0; i<256;i++)
	{
		g_interrupt_handlers[i].action=0;
		g_interrupt_handlers[i].name=0;
		g_interrupt_handlers[i].stat.num_irqs=0;
		g_interrupt_handlers[i].stat.num_error=0;
	}
	for (i=0; i<32;i++)
		ar_registerInterrupt(i, gpFault,"gpfault");

	ar_registerInterrupt(14, ar_pageFault,"pagefault");
	ar_registerInterrupt(13, gpFault,"gpfault");
}

void ar_printIrqStat(char *arg1,char *arg2)
{
	int i;

	ut_printf("irq_no : name : address: total_calls : errors\n");
	for (i=0; i<256;i++)
	{
		if (g_interrupt_handlers[i].action==0 && g_interrupt_handlers[i].stat.num_error==0) continue;
		if (i<32 && g_interrupt_handlers[i].stat.num_error==0 && g_interrupt_handlers[i].stat.num_irqs==0) continue;
		ut_printf(" %d: %s  %x %d %d\n",i-32,g_interrupt_handlers[i].name,g_interrupt_handlers[i].action,g_interrupt_handlers[i].stat.num_irqs,g_interrupt_handlers[i].stat.num_error);
	}
}

void ar_registerInterrupt(uint8_t n, isr_t handler,char *name)
{
	g_interrupt_handlers[n].action = handler;
	g_interrupt_handlers[n].name=name;
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
}

// This gets called from our ASM interrupt handler stub.
void ar_faultHandler(void *p, unsigned int  int_no)
{
	struct fault_ctx ctx;

	asm volatile("cli"); /* SOME BAD happened STOP all th einterrupts */
	fill_fault_context(&ctx,p,int_no);
	if (g_interrupt_handlers[int_no].action != 0)
	{
		isr_t handler = g_interrupt_handlers[int_no].action;
		handler(&ctx);
		g_interrupt_handlers[int_no].stat.num_irqs++;
		if (int_no ==14 ) return ; /* return properly for page fault */
	}else
	{
		g_interrupt_handlers[int_no].stat.num_error++;
		ut_printf("UNhandled FAULT ..: %d \n",int_no);
	}
	while(1);
}
extern void do_softirq();
extern addr_t g_error_i,g_after_i,g_before_i;
// This gets called from our ASM interrupt handler stub.
void ar_irqHandler(void *p,unsigned int int_no)
{
	//if (g_error_i ==0) g_after_i=g_before_i;
	// Send an EOI (end of interrupt) signal to the PICs.
	// If this interrupt involved the slave. 
	if (int_no >= 40)
	{
		// Send reset signal to slave.
		outb(0xA0, 0x20);
	}
	// Send reset signal to master. (As well as slave, if necessary).
	outb(0x20, 0x20);

	if (g_interrupt_handlers[int_no].action != 0)
	{
		isr_t handler = g_interrupt_handlers[int_no].action;
		if (int_no == 128)
		{
			struct fault_ctx ctx;

			fill_fault_context(&ctx,p,int_no);
			handler(&ctx);
		}else
		{
			handler();
		}
		g_interrupt_handlers[int_no].stat.num_irqs++;
	}else
	{
		g_interrupt_handlers[int_no].stat.num_error++;
		if (int_no != 32)	    ut_printf("UNhandled interrupt ..: %d \n",int_no);
	}
}



