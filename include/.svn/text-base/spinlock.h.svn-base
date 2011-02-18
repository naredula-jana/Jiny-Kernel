#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H
typedef struct {
        volatile unsigned int lock;
} spinlock_t;
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
