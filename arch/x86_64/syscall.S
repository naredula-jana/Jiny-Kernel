#include "descriptor_tables.h"

.global syscall_entry
.global enter_userspace
.global HP_syscall

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

#define TOTAL_SYSCALLS  400


enter_userspace:
	addq $0x8,%rsp
        RESTORE_GPRS 

/*	addq $0x70,%rsp */
	popq %rax

          /* Now restore user %gs */
    cli
    RESTORE_USER_SEGMENT_REGISTERS
    pushq %rax
    movl %gs:(CPU_STATE_USER_GS_OFFSET), %eax
    swapgs
    movl %eax, %gs
    popq %rax

    orq $(1 << 9),IRET_OFFSET_RFLAGS(%rsp)
    iretq
	
syscall_entry:
        /* Local interrupts are disabled upon entering the entrypoint.
         */
    swapgs
    movq %rsp,%gs:CPU_STATE_USER_STACK_OFFSET
    movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rsp
    SAVE_AND_LOAD_SEGMENT_REGISTERS

/* enter using syscall but exit using iret, so we need to setup the stack accordingly */
	subq $IRET_OFFSET_SIZE,%rsp
    movq %rcx,IRET_OFFSET_RIP(%rsp)
    movq %r11,IRET_OFFSET_RFLAGS(%rsp)
    movq %rcx,%gs:CPU_STATE_USER_IP
    movq $(GDT_SEL(UCODE_DESCR) | SEG_DPL_USER),IRET_OFFSET_CS(%rsp)
    movq $(GDT_SEL(UDATA_DESCR) | SEG_DPL_USER),IRET_OFFSET_SS(%rsp)
 
    movq %gs:CPU_STATE_USER_STACK_OFFSET,%rcx
    movq %rcx,IRET_OFFSET_RSP(%rsp)

        /* After changing stack pointer, we can enable IRQs */
    sti
	pushq %rax
    SAVE_GPRS

    cmp $TOTAL_SYSCALLS,%rax
    jb good_syscall_number

    movq $-35,%rax
tight_loop_error:
    jmp tight_loop_error

good_syscall_number:
    pushq %rax
    pushq %rdx
    movq %rax,%gs:CPU_STATE_SYSCALLID
    call sc_before_syscall
    popq %rdx
    popq %rax
        /* Since %rcx contains the return address, we should reload
         * %rcx from %r10.   rcx contains syscall arguments
         */
    movq %r10,%rcx
    call *syscalltable(,%rax,8)
    pushq %rax
    call sc_after_syscall
    popq %rax

syscall_done:
	RESTORE_GPRS
          /* Now restore user %gs */
	cli
	addq $8,%rsp /* Simulate our extra 'push %rax' */
	RESTORE_USER_SEGMENT_REGISTERS
	pushq %rax
	movl %gs:(CPU_STATE_USER_GS_OFFSET), %eax
	swapgs
	movl %eax, %gs
	popq %rax
	orq $(1 << 9),IRET_OFFSET_RFLAGS(%rsp)
	iretq



HP_syscall:
	pushq %r8
	pushq %r9
    movq %rax,%gs:CPU_STATE_SYSCALLID /* used for stat */
	pushq %rbx
	pushq %rbp
	movq %rsp, %rbx
	movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rsp
	movq %gs:CPU_STATE_KERNEL_STACK_OFFSET,%rbp
	subq $16, %rsp
	subq $16, %rbp
	pushq %rbx // old stack

    pushq %rax
    call sc_before_syscall
    popq %rax

	call *syscalltable(,%rax,8)

    pushq %rax
	call sc_after_syscall
	popq %rax

	popq %rbx // old stack
	movq %rbx, %rsp
	popq %rbp
	popq %rbx


	popq %r9
	popq %r8
	ret
