#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H



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
#define SPIN_BUG() while(1)

#define __SPINLOCK_LOCKED   1
#define __SPINLOCK_UNLOCKED 0

//#define SPINLOCK_DEBUG_LOG 1

#ifdef SPINLOCK_DEBUG
#define SPIN_LOCK_UNLOCKED(x) (spinlock_t) { __SPINLOCK_UNLOCKED,0,x,0,0,0,0,-1,1,0,0,0}
#define MAX_SPINLOCKS 100
extern spinlock_t *g_spinlocks[MAX_SPINLOCKS];
extern int g_spinlock_count;
#else
#define SPIN_LOCK_UNLOCKED (spinlock_t) { __SPINLOCK_UNLOCKED }
#endif

#if 0
static inline void arch_spinlock_transfer(spinlock_t *lock,
		struct task_struct *prev, struct task_struct *next) {
	if (lock->recursion_allowed == 1) SPIN_BUG();
	if (lock->recursion_allowed == 0 && prev->pid != lock->pid) {
		SPIN_BUG();
	}
	prev->locks_nonsleepable--;
	next->locks_nonsleepable++;
	lock->pid = next->pid;
#ifdef SPINLOCK_DEBUG
#ifdef SPINLOCK_DEBUG_LOG
	if (lock->log_length >= MAX_SPIN_LOG) lock->log_length=0;
	lock->log[lock->log_length].line = 99999;
	lock->log[lock->log_length].pid = prev->pid;
	lock->log[lock->log_length].cpuid = next->current_cpu;
	lock->log_length++;
#endif
#endif
}
#endif

static inline void arch_spinlock_lock(spinlock_t *lock, int line) {
#ifdef SPINLOCK_DEBUG
#ifdef RECURSIVE_SPINLOCK
	if (lock->recursion_allowed==1 && lock->pid == g_current_task->pid  && g_current_task->pid!=0) {
		lock->recursive_count++;
		lock->stat_recursive_locks++;
		return;
	}else{
		if (lock->recursion_allowed==0 && lock->pid == g_current_task->pid){ /* already owning the lock */
			SPIN_BUG();
		}
	}
#endif
#endif /* DEBUG*/
	if (1){
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
#if 0
	if (lock->name!=0 && lock->linked == -1 && g_spinlock_count<MAX_SPINLOCKS) {
		lock->linked = 0;
		g_spinlocks[g_spinlock_count]=lock;
		g_spinlock_count++;
	}
#endif

#ifdef SPINLOCK_DEBUG_LOG
	if (lock->log_length >= MAX_SPIN_LOG) lock->log_length=0;
	lock->log[lock->log_length].line = line;
	lock->log[lock->log_length].pid = g_current_task->pid;
	lock->log[lock->log_length].cpuid = g_current_task->current_cpu;
	lock->log[lock->log_length].name = g_current_task->name;
	lock->log[lock->log_length].spins = 1 + (lock->stat_count/10);
	//lock->log[lock->log_length].line = line;
	lock->log_length++;
#endif
	lock->pid = g_current_task->pid;
	g_current_task->locks_nonsleepable++;

	lock->contention=lock->contention+(lock->stat_count/10);
#endif
	  }/* toplevel if */
}

static inline void arch_spinlock_free(spinlock_t *lock){
	int i;

	for (i=0; i<MAX_SPINLOCKS; i++) {
		if (g_spinlocks[i]==lock) {
			g_spinlocks[i]=0;
		}
	}
	return;
}
static inline void arch_spinlock_link(spinlock_t *lock){ /* this is for logging and debugging purpose */
	int i;

	for (i=0; i<MAX_SPINLOCKS; i++) {
			if (g_spinlocks[i]==lock) {
				return;
			}
	}
	for (i=0; i<MAX_SPINLOCKS; i++) {
			if (g_spinlocks[i]==0) {
				g_spinlocks[i]=lock;
				lock->linked = 0;
				if (i>g_spinlock_count){
					g_spinlock_count = i+1;
				}
				return;
			}
		}
}
static inline void arch_spinlock_init(spinlock_t *lock, unsigned char *name){
	int i;

	*lock = SPIN_LOCK_UNLOCKED(name);
	arch_spinlock_link(lock);
}
static inline void arch_spinlock_unlock(spinlock_t *lock, int line) {
#ifdef SPINLOCK_DEBUG
#ifdef RECURSIVE_SPINLOCK
	if (lock->recursion_allowed==1 && lock->pid == g_current_task->pid && lock->recursive_count>0) {
		lock->recursive_count--;
		return;
	} else {
		if (lock->recursion_allowed==0 && lock->pid != g_current_task->pid){
			SPIN_BUG();
		}
	}
#endif
	if (1){
		lock->stat_unlocks++;
#ifdef SPINLOCK_DEBUG_LOG
		if (lock->log_length >= MAX_SPIN_LOG) lock->log_length=0;
		lock->log[lock->log_length].line = line;
		lock->log[lock->log_length].pid = g_current_task->pid;
		lock->log[lock->log_length].cpuid = g_current_task->current_cpu;
		lock->log[lock->log_length].name = g_current_task->name;
		lock->log[lock->log_length].spins = 0;
		lock->log_length++;
#endif
		lock->pid = 0;
		lock->recursive_count = 0;
		g_current_task->locks_nonsleepable--;
	}
#endif

	__asm__ __volatile__( __LOCK_PREFIX "xchgl %0, %1\n"
			:: "r"(__SPINLOCK_UNLOCKED), "m"( lock->lock )
			: "memory" );
#ifdef SPINLOCK_DEBUG
#endif
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
