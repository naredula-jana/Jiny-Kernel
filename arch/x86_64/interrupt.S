#include "descriptor_tables.h"

.code64
.text

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
        SAVE_GPRS
	movq %rsp, %rdi /* contains the context : first argument */
        movq %rax, %rsi /* fault number : second argument */
        callq ar_faultHandler
	RESTORE_GPRS
        popq %rax
	addq $8, %rsp  /*  error code on th stack */
		  /* Make sure IF is set after returning from interrupt context.  */
        orq $(1 << 9),HW_INTERRUPT_CTX_RFLAGS_OFFT(%rsp) 
	sti
        iretq

irq_entry_point:

/*        cmpq $GDT_SEL(KCODE_DESCR), 8 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 2f

        swapgs
        SAVE_AND_LOAD_SEGMENT_REGISTERS
*/
2:
        SAVE_GPRS
	movq %rsp, %rdi /* contains the context : first argument */
        movq %rax, %rsi  /* contains interrupts number */
        callq ar_irqHandler
	RESTORE_GPRS
/*		
	cmpq $GDT_SEL(KCODE_DESCR), 8 + INT_STACK_FRAME_CS_OFFT(%rsp)
        je 1f

        RESTORE_USER_SEGMENT_REGISTERS
        cli
        movl %gs:(CPU_SCHED_STAT_USER_GS_OFFT), %eax
        swapgs
        movl %eax, %gs
1:	*/	
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
         GEN_INTRS_TABLE 32, 255, irq_entry_point

