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


#ifndef IPC_C
#define IPC_C
#include "interface.h"
void *mutexCreate() { // TODO implementing TODO
    struct semaphore *sem = mm_malloc(sizeof(struct semaphore),0);

    sem_alloc(sem, 1);
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
int sem_alloc(struct semaphore *sem,uint8_t count)
{
	  int ret;

	  sem->count = count;
	  sem->sem_lock = SPIN_LOCK_UNLOCKED;
	  sem->valid_entry = 1;
	  ret = sc_register_waitqueue(&sem->wait_queue,"semaphore");
	  return ret;
}

int sem_free(struct semaphore *sem)
{
    sc_unregister_waitqueue(&sem->wait_queue);
    return 0;
}
/* Creates and returns a new semaphore. The "count" argument specifies
 * the initial state of the semaphore. */
signed char  sys_sem_new(sys_sem_t *sem,uint8_t count)
{
    sem_alloc(sem,count );
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
    sc_unregister_waitqueue(&sem->wait_queue);
    mm_free(sem);
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


uint32_t sys_arch_sem_wait(sys_sem_t *sem, uint32_t timeout_arg) {
	unsigned long flags;
	unsigned long timeout;

	timeout=timeout_arg*100;
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
#endif
