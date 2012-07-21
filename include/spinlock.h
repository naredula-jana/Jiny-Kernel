#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H
typedef struct {
        volatile unsigned int lock;
} spinlock_t;

#define __sti() __asm__ __volatile__ ("sti": : :"memory")
#define __cli() __asm__ __volatile__ ("cli": : :"memory")



#if 1
#define BITS_PER_LONG 64
#define warn_if_not_ulong(x) do { unsigned long foo; (void) (&(x) == &foo); } while (0)
#define __save_flags(x)         do { warn_if_not_ulong(x); __asm__ __volatile__("# save_flags \n\t pushfq ; popq %q0":"=g" (x): /* no input */ :"memory"); } while (0)
#define __restore_flags(x)      __asm__ __volatile__("# restore_flags \n\t pushq %0 ; popfq": /* no output */ :"g" (x):"memory", "cc")
#else
#define BITS_PER_LONG 32
#define __save_flags(x) \
	__asm__ __volatile__("pushfl ; popl %0":"=g" (x): /* no input */ :"memory")
#define __restore_flags(x) \
										   __asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (x):"memory")
#endif


#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)

#define SPIN_LOCK_UNLOCKED (spinlock_t) { 0 }


#define spin_lock_init(x)       do { (x)->lock = 0; } while (0)
#define spin_trylock(lock)      (!test_and_set_bit(0,(lock)))

#define spin_lock(x)            do { (x)->lock = 1; } while (0)
#define spin_unlock_wait(x)     do { } while (0)
#define spin_unlock(x)          do { (x)->lock = 0; } while (0)
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
