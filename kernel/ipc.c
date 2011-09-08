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

void sys_sem_init(struct semaphore *sem,uint8_t count)
{
	  sem->count = count;
	  sc_register_waitqueue(&sem->wait_queue);
}

/* Creates and returns a new semaphore. The "count" argument specifies
 * the initial state of the semaphore. */
sys_sem_t sys_sem_new(uint8_t count)
{
    struct semaphore *sem = mm_malloc(sizeof(struct semaphore),0);

    sys_sem_init(sem,count );
    return sem;
}



/* Deallocates a semaphore. */
void sys_sem_free(sys_sem_t sem)
{
    sc_unregister_waitqueue(&sem->wait_queue);
    mm_free(sem);
}

/* Signals a semaphore. */
void sys_sem_signal(sys_sem_t sem)
{
    unsigned long flags;
    local_irq_save(flags);
    sem->count++; /* TODO : this may break on multiprocessor */
    sc_wakeUp(&sem->wait_queue,NULL);
    local_irq_restore(flags);
}


uint32_t sys_arch_sem_wait(sys_sem_t sem, uint32_t timeout_arg) {
	unsigned long flags;
	uint32_t timeout;

	timeout=timeout_arg*100;
	while (1) {
		if (sem->count <= 0 )
		{
			timeout=sc_wait(&(sem->wait_queue), timeout);
		}
		if (timeout_arg == 0) timeout=100;
		local_irq_save(flags);
		/* Atomically check that we can proceed */
		if (sem->count > 0 || (timeout <= 0))
			break;
		local_irq_restore(flags);
	}

	if (sem->count > 0) {
		sem->count--;  /* TODO : this may break on multiprocessor */
		local_irq_restore(flags);
		return 1;
	}

	local_irq_restore(flags);
	return IPC_TIMEOUT;
}
#endif
