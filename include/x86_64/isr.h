#ifndef __isr_h
#define __isr_h
#include "types.h"
typedef struct {
        unsigned int __softirq_pending;
        unsigned int __local_irq_count;
        unsigned int __local_bh_count;
        unsigned int __syscall_count;
} irq_cpustat_t;
#ifdef CONFIG_SMP
#define __IRQ_STAT(cpu, member) (irq_stat[cpu].member)
#else
#define smp_processor_id()			0
#define NR_CPUS 1
#define __IRQ_STAT(cpu, member) (irq_stat[((void)(cpu), 0)].member)
#endif
extern irq_cpustat_t irq_stat[NR_CPUS] ;
#define softirq_pending(cpu)    __IRQ_STAT((cpu), __softirq_pending)
#define local_irq_count(cpu)    __IRQ_STAT((cpu), __local_irq_count)
#define local_bh_count(cpu)     __IRQ_STAT((cpu), __local_bh_count)
#define syscall_count(cpu)      __IRQ_STAT((cpu), __syscall_count)
#define ksoftirqd_task(cpu)     __IRQ_STAT((cpu), __ksoftirqd_task)
  /* arch dependent irq_stat fields */
#define nmi_count(cpu)          __IRQ_STAT((cpu), __nmi_count)          /* i386, ia64 */
#define in_interrupt() ({ int __cpu = smp_processor_id(); \
        (local_irq_count(__cpu) + local_bh_count(__cpu) != 0); })

#define irq_enter(cpu, irq)     (local_irq_count(cpu)++)
#define irq_exit(cpu, irq)      (local_irq_count(cpu)--)

struct gpregs {
  uint64_t rbp;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rax;
};

struct intr_stack_frame {
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};

struct fault_ctx {
  struct gpregs *gprs;
  struct intr_stack_frame *istack_frame;
  uint32_t errcode;
  int fault_num;
  void *old_rsp;
};



#endif
