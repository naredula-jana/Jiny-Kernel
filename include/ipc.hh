/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   ipc.hh
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#ifndef IPC_HH
#define IPC_HH

#define MAX_WAIT_QUEUES 500

class wait_queue: public jobject {
	int wait_internal(unsigned long ticks,spinlock_t *spin_lock);
public:
	struct list_head head;
	char *name;
	void *used_for; /* it can be used for semaphore/mutex  or raw waitqueue */
	unsigned long flags;
	//spinlock_t lock; /* this is to protect list */

	unsigned long stat_wait_count;
	unsigned long stat_wait_ticks;
	unsigned long stat_wakeups;
	unsigned long stat_wakeon_samecpu;

	int _del_from_me(struct task_struct *p);
	void _add_to_me(struct task_struct * p, long ticks);

	wait_queue(char *name, unsigned long flags);
	int wakeup();
	int wait(unsigned long ticks);
	int wait_with_lock(unsigned long ticks, spinlock_t *spin_lock);
	void print_stats(unsigned char *arg1,unsigned char *arg2);
	int unregister();  /* TODO : this should be  merged with destructor */

	static wait_queue *wait_queues[MAX_WAIT_QUEUES];
	static void del_from_waitqueues(struct task_struct *task);
};
#define IPC_TIMEOUT 0xffffffffUL
#define WAIT_QUEUE_WAKEUP_ONE 1
#define IPC_NAME_MAX 100
class semaphore: public jobject{
public:
	int count;
	spinlock_t sem_lock; /* this is to protect count */
	wait_queue *waitqueue;
	int valid_entry;
	char name[IPC_NAME_MAX];
	unsigned long owner_pid; /* pid that is owning */
	int recursive_count;

	unsigned int stat_line;
	unsigned int stat_recursive_count;
	unsigned long stat_acquired_start_time;
	unsigned long stat_total_acquired_time;
	unsigned long stat_lock,stat_contention,stat_cont_time;

	semaphore(uint8_t arg_count, char *arg_name);
	void signal();
	uint32_t wait(uint32_t timeout_arg);
	int lock(int line_no);
	int unlock(int line_no);
	void print_stats(unsigned char *arg1,unsigned char *arg2);
	void free(); /* TODO : this should be  merged with destructor */
};
class futex: public jobject{
	wait_queue *waitq;
	int type;
public:
	int *uaddr;
	struct mm_struct *mm;
	spinlock_t spin_lock;

	unsigned long stat_count,stat_waits,stat_nowaits,stat_lnowaits,stat_wakeups_reqs;
	unsigned long stat_wakeup_sucess;

	futex(int *uaddr);
	void lock(spinlock_t *spin_lock);
	int unlock();
	void destroy();
	void print_stats(unsigned char *arg1,unsigned char *arg2);
};
extern "C"{

}
#endif
