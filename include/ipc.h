#ifndef IPC_H
#define IPC_H

#include "list.h"
//#include "spinlock.h"

#define SPINLOCK_DEBUG 1
typedef struct {
        volatile unsigned int lock;
        unsigned long stat_count;
#ifdef SPINLOCK_DEBUG
#define MAX_SPIN_LOG 100
        unsigned long stat_locks;
        unsigned long stat_unlocks;
        unsigned char *name;
        int linked; /* linked this structure to stats */
        unsigned long pid;
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


#define IPC_TIMEOUT 0xffffffffUL
typedef struct queue{
	struct list_head head;
	char *name;
}queue_t;

struct semaphore
{
	int count;
	spinlock_t sem_lock; /* this is to protect count */
	queue_t wait_queue;
	int valid_entry;
};

typedef struct semaphore sys_sem_t;
/* ipc */

void *mutexCreate();
int mutexLock(void *p);
int mutexUnLock(void *p);
int mutexDestroy(void *p);

signed char sys_sem_new(sys_sem_t *sem,uint8_t count);
void sys_sem_free(sys_sem_t *sem);
void sys_sem_signal(sys_sem_t *sem);
uint32_t sys_arch_sem_wait(sys_sem_t *sem, unsigned int timeout);
#endif
