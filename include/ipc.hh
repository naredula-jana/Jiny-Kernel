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
public:
	struct list_head head;
	char *name;
	void *used_for; /* it can be used for semaphore/mutex  or raw waitqueue */
	unsigned long flags;

	int stat_wait_count;
	int stat_wait_ticks;

	int _del_from_me(struct task_struct *p);
	void _add_to_me(struct task_struct * p, long ticks);

	wait_queue(char *name, unsigned long flags);
	int wakeup();
	int wait(unsigned long ticks);
	void print_stats();
	int unregister();  /* TODO : this should be  merged with destructor */

	static wait_queue *wait_queues[MAX_WAIT_QUEUES];
	static void del_from_waitqueues(struct task_struct *task);
};
#define IPC_TIMEOUT 0xffffffffUL
#define WAIT_QUEUE_WAKEUP_ONE 1
class semaphore: public jobject{
public:
	int count;
	spinlock_t sem_lock; /* this is to protect count */
	wait_queue *waitqueue;
	int valid_entry;
	char *name;
	unsigned long owner_pid; /* pid that is owning */
	int recursive_count;

	unsigned int stat_line;
	unsigned int stat_recursive_count;
	unsigned long stat_acquired_start_time;
	unsigned long stat_total_acquired_time;

	semaphore(uint8_t arg_count, char *arg_name);
	void signal();
	uint32_t wait(uint32_t timeout_arg);
	int lock(int line_no);
	int unlock(int line_no);
	void print_stats();
	void free(); /* TODO : this should be  merged with destructor */
};
extern "C"{
#if 0
struct semaphore {
	int count;
	spinlock_t sem_lock; /* this is to protect count */
	wait_queue *wait_queue;
	int valid_entry;
	char *name;
	unsigned long owner_pid; /* pid that is owning */
	int recursive_count;

	unsigned int stat_line;
	unsigned int stat_recursive_count;
	unsigned long stat_acquired_start_time;
	unsigned long stat_total_acquired_time;
};
#endif
}
#endif
