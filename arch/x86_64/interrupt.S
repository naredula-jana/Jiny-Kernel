#include "descriptor_tables.h"

	.align	0x4000
irq_stack1:
        .fill   0x8000,1,0
irq_stack2:
        .fill   0x8000,1,0

.code64
.text
#define SAVE_AND_LOAD_SEGMENT_REGISTERS         \
   mov %ds, %gs:CPU_STATE_USER_DS_OFFSET;            \
   mov %es, %gs:CPU_STATE_USER_ES_OFFSET;            \
 /*  mov %fs, %gs:CPU_STATE_USER_FS_OFFSET;            \*/
   mov %gs, %gs:CPU_STATE_USER_GS_OFFSET;            \
   mov %gs:(CPU_STATE_KERNEL_DS_OFFSET),%ds;

#define RESTORE_USER_SEGMENT_REGISTERS        \
   mov %gs:(CPU_STATE_USER_DS_OFFSET),%ds;  \
   mov %gs:(CPU_STATE_USER_ES_OFFSET),%es; \
/*   mov %gs:(CPU_STATE_USER_FS_OFFSET),%fs; \*/
   pushq %rax;     \
   pushq %rcx;     \
   pushq %rdx;     \
   mov %gs:(CPU_STATE_USER_FS_BASE),%rax; \
   movq $0xC0000100, %rcx; \
   movq %rax,%rdx; \
   shr $32,%rdx;\
   wrmsr; \
   popq %rdx; \
   popq %rcx; \
   popq %rax;

/* Save all general purpose registers  */
#define SAVE_GPRS                               \
    pushq %r8;                                  \
    pushq %r9;                                  \
    pushq %r10;                                 \
    pushq %r11;                                 \
    pushq %r12;                                 \
    pushq %r13;                                 \
    pushq %r14;                                 \
    pushq %r15;                                 \
    pushq %rbx;                                 \
    pushq %rcx;                                 \
    pushq %rdx;                                 \
    pushq %rdi;                                 \
    pushq %rsi;                                 \
    pushq %rbp;                                 \

#define RESTORE_GPRS                            \
    popq %rbp;                                  \
    popq %rsi;                                  \
    popq %rdi;                                  \
    popq %rdx;                                  \
    popq %rcx;                                  \
    popq %rbx;                                  \
    popq %r15;                                  \
    popq %r14;                                  \
    popq %r13;                                  \
    popq %r12;                                  \
    popq %r11;                                  \
    popq %r10;                                  \
    popq %r9;                                   \
    popq %r8;                                   \
	
#define NUM_GPR_SAVED 14
#define SAVED_GPR_SIZE (NUM_GPR_SAVED * 8)

.macro GEN_INTRS_TABLE idx, items, entry_point
.set cnt, \idx
.rept \items - \idx
        .align 16
        pushq %rax
        movq $cnt, %rax
        jmp \entry_point
.set cnt, cnt+1
.endr
.endm

.globl ar_faultsTable
.globl ar_irqsTable
.globl irq_handler

fault_entry_point:
        btq %rax, faults_with_errcode
        jc 1f

        subq $8, %rsp
        pushq %rbx
        movq 16(%rsp), %rbx
        movq $0, 16(%rsp)
        movq %rbx, 8(%rsp)
        popq %rbx

1:
        cmpq $GDT_SEL(KCODE_DESCR), 16 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 2f
        swapgs
2:
        SAVE_GPRS
	    movq %rsp, %rdi /* contains the context : first argument */
        movq %rax, %rsi /* fault number : second argument */
        cmpq $03,%rax /* if it is a breakpoint trap exception */
        jz breakpoint_handler

movq %rsp,%rax  /* copy existing sp */
and $0xffffffffffffc000,%rax /* find base of stack */
add $0x4000,%rax
cmpq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rax
je 9f
movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rsp
movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rbp
subq $0x10, %rsp /* TODO-HP1 : need to leave some head room , look like some memory corruption is happening */
subq $0x10, %rbp

9:
pushq %rdi  /* contains old stack pointer */
        callq ar_faultHandler   /* rest of the exceptions */
popq %rdi
movq %rdi, %rsp

	    RESTORE_GPRS
	    cmpq $GDT_SEL(KCODE_DESCR), 16 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 4f
        RESTORE_USER_SEGMENT_REGISTERS
        swapgs
4:
        popq %rax
	    addq $8, %rsp  /*  error code on th stack */
		  /* Make sure IF is set after returning from interrupt context.  */
        orq $(1 << 9),HW_INTERRUPT_CTX_RFLAGS_OFFT(%rsp) 
	    sti
        iretq

breakpoint_handler:
        callq ar_faultHandler      /* for breakpoint handling */
	    RESTORE_GPRS
	    cmpq $GDT_SEL(KCODE_DESCR), 16 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 6f
        RESTORE_USER_SEGMENT_REGISTERS
        swapgs
6:
		cmpq $1,%rax  /* if return value of trap handler is to pushq %rbp*/
		jz rbp_push
		cmpq $2,%rax
		jz rbp_pop

        popq %rax   /* when we reach , something wrong , return value 0 is not expected */
	    addq $8, %rsp  /*  error code on th stack */
		  /* Make sure IF is set after returning from interrupt context.  */
 //       orq $(1 << 9),HW_INTERRUPT_CTX_RFLAGS_OFFT(%rsp)
//	    sti
        iretq

err1:
 jmp err1
 err2:
 jmp err2

rbp_push:
        popq %rax
	    addq $8, %rsp  /*  error code on th stack */

		subq	$16, %rsp		/* make room for %rbp */
		pushq	%rax			/* push temp */
		movq	24(%rsp), %rax		/* Load RIP */
		movq	%rax, 8(%rsp)		/* STORE RIP */
		movq	32(%rsp), %rax		/* Load CS */
		movq	%rax, 16(%rsp)		/* Store CS */
		movq	40(%rsp), %rax		/* Load RFLAGS */
		movq	%rax, 24(%rsp)		/* Store RFLAGS */
		movq	48(%rsp), %rax		/* Load RSP */
		subq	$8, %rax		/* 8byte space %rbp */
		movq	%rax, 32(%rsp)		/* Store RSP */
		movq	56(%rsp), %rax		/* Load SS */
		movq	%rax, 40(%rsp)		/* Store SS */
		movq	32(%rsp), %rax		/* reload RSP */
		movq	%rbp, (%rax)		/* store %rbp there */
		popq	%rax			/* pop off temp rax */

		/* Make sure IF is set after returning from interrupt context.  */
  //      orq $(1 << 9),HW_INTERRUPT_CTX_RFLAGS_OFFT(%rsp)
//		sti
		iretq				/* return from interrupt */

rbp_pop:
        popq %rax
	    addq $8, %rsp  /*  error code on th stack */

		pushq	%rax			/* push temp */
		movq	8(%rsp), %rax		/* load calling RIP */
		movq	%rax, 8(%rsp)		/* store calling RIP */
		movq	(%rbp), %rax		/* get new %rbp */
		addq	$8, %rbp		/* adjust new %rsp */
		movq	%rbp, 32(%rsp)		/* store new %rsp */
		movq	%rax, %rbp		/* set new %rbp */
		popq	%rax			/* pop off temp */

				/* Make sure IF is set after returning from interrupt context.  */
//        orq $(1 << 9),HW_INTERRUPT_CTX_RFLAGS_OFFT(%rsp)
//		sti
		iretq				/* return from interrupt */

irq_entry_point:
        cmpq $GDT_SEL(KCODE_DESCR), 8 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 1f

        swapgs
        SAVE_AND_LOAD_SEGMENT_REGISTERS
1:
        SAVE_GPRS
	    movq %rsp, %rdi /* contains the context : first argument */
        movq %rax, %rsi  /* contains interrupts number */

movq %rsp,%rax
and $0xffffffffffffc000,%rax
add $0x4000,%rax
cmpq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rax
je 9f

movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rsp
movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rbp
subq $0x10, %rsp
subq $0x10, %rbp
9:
pushq %rdi  /* contains old stack pointer */
        callq ar_irqHandler
popq %rdi
movq %rdi, %rsp

	    RESTORE_GPRS
	    cmpq $GDT_SEL(KCODE_DESCR), 8 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 2f

        RESTORE_USER_SEGMENT_REGISTERS
        cli
        movl %gs:(CPU_STATE_USER_GS_OFFSET), %eax
        swapgs
        movl %eax, %gs
2:		
        popq %rax

		  /* Make sure IF is set after returning from interrupt context. */
        orq $(1 << 9),HW_INTERRUPT_CTX_RFLAGS_OFFT(%rsp) 
	    sti
/*	callq do_softirq   this call desctorys EAX  */
        iretq    /* ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP */

.align 16
ar_faultsTable:
        GEN_INTRS_TABLE 0, 32, fault_entry_point

.align 16
ar_irqsTable:
         GEN_INTRS_TABLE 32, 256, irq_entry_point

