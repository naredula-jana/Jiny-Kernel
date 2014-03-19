
extern "C" {
#include "common.h"
#include "descriptor_tables.h"


static struct task_struct *__switch_to(struct task_struct *prev_p, struct task_struct *next_p) {
	return prev_p;
}

#define __STR(x) #x
#define STR(x) __STR(x)

#define __PUSH(x) "pushq %%" __STR(x) "\n\t"
#define __POP(x)  "popq  %%" __STR(x) "\n\t"

#define SAVE_CONTEXT \
	__PUSH(rsi) __PUSH(rdi) \
__PUSH(r12) __PUSH(r13) __PUSH(r14) __PUSH(r15)  \
__PUSH(rdx) __PUSH(rcx) __PUSH(r8) __PUSH(r9) __PUSH(r10) __PUSH(r11)  \
__PUSH(rbx) __PUSH(rbp)
#define RESTORE_CONTEXT \
	__POP(rbp) __POP(rbx) \
__POP(r11) __POP(r10) __POP(r9) __POP(r8) __POP(rcx) __POP(rdx) \
__POP(r15) __POP(r14) __POP(r13) __POP(r12) \
__POP(rdi) __POP(rsi)

#define _switch_to(prev, next, last)   do {   \
		/*                                                              \
		 * Context-switching clobbers all registers, so we clobber      \
		 * them explicitly, via unused output variables.                \
		 * (EAX and EBP is not listed because EBP is saved/restored     \
		 * explicitly for wchan access and EAX is the return value of   \
		 * __switch_to())                                               \
		 */                                                             \
		unsigned long rbx, rcx, rdx, rsi, rdi;                          \
		\
		asm volatile("pushfq\n\t"               /* save    flags */     \
				SAVE_CONTEXT \
				"pushq %%rbp\n\t"          /* save    EBP   */     \
				"movq %%rbp,%[prev_rbp]\n\t"        /* save    RBP   */ \
				"movq %%rsp,%[prev_sp]\n\t"        /* save    ESP   */ \
				"movq %[next_sp],%%rsp\n\t"        /* restore ESP   */ \
				"movq $1f,%[prev_ip]\n\t"  /* save    EIP   */     \
				"pushq %[next_ip]\n\t"     /* restore EIP   */     \
				"jmp __switch_to\n"        /* regparm call  */     \
				"1:\t"                                             \
				"popq %%rbp\n\t"           /* restore EBP   */     \
				RESTORE_CONTEXT \
				"popfq\n"                  /* restore flags */     \
				\
				/* output parameters */                            \
				: [prev_sp] "=m" (prev->thread.sp),                \
				[prev_ip] "=m" (prev->thread.ip),                \
				[prev_rbp] "=m" (prev->thread.rbp),                \
				"=a" (last),                                     \
				\
				/* clobbered output registers: */                \
				"=b" (rbx), "=c" (rcx), "=d" (rdx),              \
				"=S" (rsi), "=D" (rdi)                           \
		\
		\
		/* input parameters: */                          \
		: [next_sp]  "m" (next->thread.sp),                \
		[next_ip]  "m" (next->thread.ip),                \
		\
		/* regparm parameters for __switch_to(): */      \
		[prev]     "a" (prev),                           \
		[next]     "d" (next)                            \
		\
		\
		: /* reloaded segment registers */                 \
		"memory");                                      \
	} while (0)
}


namespace arch {
inline void sti_hlt() __attribute__((always_inline));
inline void switch_to(struct task_struct *prev, struct task_struct *next, struct task_struct *lastp) __attribute__((always_inline));


inline void sti_hlt() {
    asm volatile ("sti; hlt" : : : "memory");
}
inline void switch_to(struct task_struct *prev, struct task_struct *next, struct task_struct *lastp)
{
	_switch_to(prev, next, lastp);
}
}
