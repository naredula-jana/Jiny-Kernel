#ifndef IPC_H
#define IPC_H

#if 0
#include "common.h"
#include "task.h"
#endif

#include "list.h"
#include "spinlock.h"


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

int sem_alloc(struct semaphore *sem, unsigned char count);
int sem_free(struct semaphore *sem);

signed char sys_sem_new(sys_sem_t *sem,uint8_t count);
void sys_sem_free(sys_sem_t *sem);
void sys_sem_signal(sys_sem_t *sem);
uint32_t sys_arch_sem_wait(sys_sem_t *sem, unsigned int timeout);
#endif
