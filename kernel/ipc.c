/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/task.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "interface.h"
void *mutexCreate() { // TODO implementing TODO
    struct semaphore *sem = mm_malloc(sizeof(struct semaphore),0);

    sys_sem_new(sem, 1);
    return sem;
}
int mutexLock(void *p){
	if (p==0) return 0;
	while (sys_arch_sem_wait(p,100) != 1) ;
	return 1;
}
int mutexUnLock(void *p) {
	if (p==0) return 0;
	sys_sem_signal(p);
	return 1;
}

int mutexDestroy(void *p) {
	if (p==0) return 0;
	sys_sem_free(p);
	return 1;
}

/* this call consume system resources */
signed char sys_sem_new(struct semaphore *sem,uint8_t count)
{
	  int ret;

	  sem->count = count;
	  sem->sem_lock = SPIN_LOCK_UNLOCKED(0);
	  sem->valid_entry = 1;
	  ret = sc_register_waitqueue(&sem->wait_queue,"semaphore");
	  return 0;
}

int sys_sem_valid(sys_sem_t *sem){
  return sem->valid_entry;
}

void sys_sem_set_invalid(sys_sem_t *sem){
	sem->valid_entry=0;
}
/* Deallocates a semaphore. */
void sys_sem_free(sys_sem_t *sem)
{
    sc_unregister_waitqueue(&(sem->wait_queue));
}

/* Signals a semaphore. */
void sys_sem_signal(sys_sem_t *sem)
{
    unsigned long flags;

	spin_lock_irqsave(&(sem->sem_lock), flags);
    sem->count++;
    sc_wakeUp(&sem->wait_queue);
    spin_unlock_irqrestore(&(sem->sem_lock), flags);
}


uint32_t sys_arch_sem_wait(sys_sem_t *sem, uint32_t timeout_arg) /* timeout_arg in ms */
{
	unsigned long flags;
	unsigned long timeout;

	timeout=timeout_arg/10;
	while (1) {
		if (sem->count <= 0 )
		{
			timeout=sc_wait(&(sem->wait_queue), timeout);
		}
		if (timeout_arg == 0) timeout=100;
		spin_lock_irqsave(&(sem->sem_lock), flags);
		/* Atomically check that we can proceed */
		if (sem->count > 0 || (timeout <= 0))
			break;
	    spin_unlock_irqrestore(&(sem->sem_lock), flags);
	}

	if (sem->count > 0) {
		sem->count--;
	    spin_unlock_irqrestore(&(sem->sem_lock), flags);
		return 1;
	}

    spin_unlock_irqrestore(&(sem->sem_lock), flags);
	return IPC_TIMEOUT;
}

#ifdef SPINLOCK_DEBUG
spinlock_t *g_spinlocks[MAX_SPINLOCKS];
int g_spinlock_count=0;
int Jcmd_locks(char *arg1, char *arg2) {
	int i;

	ut_printf(" Name  pid count contention(rate) \n");
	for (i = 0; i < g_spinlock_count; i++) {
		ut_printf(" %9s %3x %5d %5d(%d) \n", g_spinlocks[i]->name,g_spinlocks[i]->pid,
				g_spinlocks[i]->stat_locks, g_spinlocks[i]->contention,g_spinlocks[i]->contention/g_spinlocks[i]->stat_locks);
	}
	return 1;
}
#endif

/**********************************************/

void ipc_test1(){
	int i;
	struct semaphore sem;

	sys_sem_new(&sem,1);
	sys_sem_signal(&sem);
	for (i=0; i<40; i++){
      ut_printf("Iterations  :%d  jiffes:%x\n",i,g_jiffies);
      sys_arch_sem_wait(&sem,1000);
	}
	sys_sem_free(&sem);
}

