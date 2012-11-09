#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H
//#define SPINLOCK_DEBUG 1
//#include "task.h"
typedef struct {
        volatile unsigned int lock;
        unsigned long stat_count;
#ifdef SPINLOCK_DEBUG
#define MAX_SPIN_LOG 100
        unsigned long stat_locks;
        unsigned long stat_unlocks;
        unsigned long contention;
        unsigned int log_length;
        struct {
            int line;
            unsigned int process_id;
            unsigned int cpuid;
            unsigned long spins;
        }log[MAX_SPIN_LOG];
#endif
} spinlock_t;

#define __sti() __asm__ __volatile__ ("sti": : :"memory")
#define __cli() __asm__ __volatile__ ("cli": : :"memory")

#define __LOCK_PREFIX "lock "

#if 1
#define BITS_PER_LONG 64
#define warn_if_not_ulong(x) do { unsigned long foo; (void) (&(x) == &foo); } while (0)
#define __save_flags(x)         do { warn_if_not_ulong(x); __asm__ __volatile__("# save_flags \n\t pushfq ; popq %q0":"=g" (x): /* no input */ :"memory"); } while (0)
#define __restore_flags(x)      __asm__ __volatile__("# restore_flags \n\t pushq %0 ; popfq": /* no output */ :"g" (x):"memory", "cc")
#endif


#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)


#define __SPINLOCK_LOCKED   1
#define __SPINLOCK_UNLOCKED 0
#ifdef SPINLOCK_DEBUG
#define SPIN_LOCK_UNLOCKED (spinlock_t) { __SPINLOCK_UNLOCKED,0,0,0 }
#else
#define SPIN_LOCK_UNLOCKED (spinlock_t) { __SPINLOCK_UNLOCKED }
#endif

static inline void arch_spinlock_lock(spinlock_t *lock, int line) {
	__asm__ __volatile__( "movq $0,%%rbx\n"
			"mov  %0,%%edx\n"
			"spin:addq $1,%%rbx\n"
			"mov   %1, %%eax\n"
			"test %%eax, %%eax\n"
			"jnz  spin\n"
			"lock cmpxchg %%edx, %1 \n"
			"test  %%eax, %%eax\n"
			"jnz  spin\n"
			"mov %%rbx, %3\n"
			:: "r"(__SPINLOCK_LOCKED),"m"(lock->lock), "rI"(__SPINLOCK_UNLOCKED),"m"(lock->stat_count)
			: "%rax","%rbx", "memory" );
#ifdef SPINLOCK_DEBUG
	lock->stat_locks++;
	if (lock->log_length >= MAX_SPIN_LOG) lock->log_length=0;
	lock->log[lock->log_length].line = line ;
	lock->log[lock->log_length].process_id = g_current_task->pid ;
	lock->log[lock->log_length].cpuid = g_current_task->cpu ;
	lock->log[lock->log_length].spins = 1 + (lock->stat_count/10) ;
	lock->log_length++;
	lock->contention=lock->contention+(lock->stat_count/10);
#endif
}

 static inline void arch_spinlock_unlock(spinlock_t *lock, int line)
{
#ifdef SPINLOCK_DEBUG
	lock->stat_unlocks++;
	if (lock->log_length >= MAX_SPIN_LOG) lock->log_length=0;
	lock->log[lock->log_length].line = line ;
	lock->log[lock->log_length].process_id = g_current_task->pid ;
	lock->log[lock->log_length].cpuid = g_current_task->cpu ;
	lock->log[lock->log_length].spins = 0 ;
	lock->log_length++;
#endif

  __asm__ __volatile__( __LOCK_PREFIX "xchgl %0, %1\n"
                        :: "r"(__SPINLOCK_UNLOCKED), "m"( lock->lock )
                        : "memory" );
}


#define spin_lock_init(x)       do { (x)->lock = __SPINLOCK_UNLOCKED; } while (0)
#define spin_trylock(lock)      (!test_and_set_bit(0,(lock)))

#define spin_lock(x)            do { arch_spinlock_lock(x,__LINE__); } while (0)
#define spin_unlock(x)          do { arch_spinlock_unlock(x,__LINE__); } while (0)


#define spin_lock_irq(x)        do { cli(); spin_lock(x); } while (0)
#define spin_unlock_irq(x)      do { spin_unlock(x); sti(); } while (0)

#define local_irq_save(flags) \
        do { save_flags(flags); cli(); } while (0)
#define local_irq_restore( flags) \
        do {  restore_flags(flags); } while (0)

#define spin_lock_irqsave(x, flags) \
        do { save_flags(flags); spin_lock_irq(x); } while (0)
#define spin_unlock_irqrestore(x, flags) \
        do { spin_unlock(x); restore_flags(flags); } while (0)

#endif
