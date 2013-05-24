#ifndef IPC_H
#define IPC_H

#include "list.h"
//#include "spinlock.h"

#define SPINLOCK_DEBUG 1
#define RECURSIVE_SPINLOCK 1
typedef struct {  /* Do not change the order , initilization will break */
        volatile unsigned int lock;
        unsigned long stat_count;
#ifdef SPINLOCK_DEBUG
#define MAX_SPIN_LOG 100
        unsigned char *name;
        unsigned long stat_locks;
        unsigned long stat_unlocks;
        unsigned long stat_recursive_locks;
        unsigned long recursive_count;
        int linked; /* linked this structure to stats */
        int recursion_allowed;
        unsigned long pid;
        unsigned long contention;
        unsigned int log_length;
        struct {
            int line;
            unsigned int pid;
            unsigned int cpuid;
            unsigned long spins;
            unsigned char *name;
        }log[MAX_SPIN_LOG];
#endif
} spinlock_t;

#define IPC_TIMEOUT 0xffffffffUL

typedef struct wait_queue {
	struct list_head head;
	char *name;
	void *used_for; /* it can be used for semaphore/mutex  or raw waitqueue */
} wait_queue_t;

struct semaphore {
	int count;
	spinlock_t sem_lock; /* this is to protect count */
	wait_queue_t wait_queue;
	int valid_entry;
	unsigned char *name;
	unsigned long owner_pid; /* pid that is owning */
	int recursive_count;
	unsigned int stat_line;
};

typedef struct semaphore sys_sem_t;

void *ipc_mutex_create(unsigned char *name);
int ipc_mutex_lock(void *p, int line);
int ipc_mutex_unlock(void *p, int line);
int ipc_mutex_destroy(void *p);

signed char ipc_sem_new(sys_sem_t *sem,uint8_t count);
void ipc_sem_free(sys_sem_t *sem);
void ipc_sem_signal(sys_sem_t *sem);
uint32_t ipc_sem_wait(sys_sem_t *sem, unsigned int timeout);


#define mutexCreate ipc_mutex_create
#define mutexLock(p)      do { ipc_mutex_lock((void *)p,__LINE__); } while (0)
#define mutexUnLock(p)      do { ipc_mutex_unlock((void *)p,__LINE__); } while (0)
#define mutexDestroy ipc_mutex_destroy
#define sys_sem_new ipc_sem_new
#define sys_sem_free ipc_sem_free
#define sys_sem_signal ipc_sem_signal
#define sys_arch_sem_wait ipc_sem_wait


#endif
