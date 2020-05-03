/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/ipc.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "file.hh"
#include "ipc.hh"
#include "futex.h"

enum {
FUTEX_WAIT = 0,
FUTEX_WAKE = 1,
FUTEX_WAKE_OP = 5,
FUTEX_WAIT_BITSET =9,
FUTEX_PRIVATE_FLAG = 128,
FUTEX_CLOCK_REALTIME = 256,
FUTEX_CMD_MASK = ~(FUTEX_PRIVATE_FLAG|FUTEX_CLOCK_REALTIME),
};
#define FUTEX_BITSET_MATCH_ANY  0xffffffff

#define MAX_FUTEXS 1024
int total_futexs=0;
class futex *futex_list[MAX_FUTEXS];

futex::futex(int *uaddr_arg){
	arch_spinlock_init(&spin_lock,"futexlock");
	waitq = jnew_obj(wait_queue,"futex", 0);
	uaddr = uaddr_arg;
	mm = g_current_task->mm;

	type =0;
	stat_waits=0;
	stat_nowaits=0;
	stat_lnowaits=0;
	stat_wakeup_sucess=0;
	stat_wakeups_reqs=0;
}
void futex::destroy(){
	wait_queue *tmp_waitq=0;
	ut_log(" %x: futex waits: %d wakeups_req/succ:%d / %d nowaits:%d lnowaits:%d  uaddr:%x mm:%x wakedups:%d wakeonsame:%d count:%d\n",this,stat_waits,stat_wakeups_reqs,stat_wakeup_sucess,stat_nowaits,stat_lnowaits,uaddr,mm,waitq->stat_wakeups,waitq->stat_wakeon_samecpu,stat_count);

	tmp_waitq=waitq;
	waitq=0;
	if (tmp_waitq!= 0){
		tmp_waitq->wakeup();
		tmp_waitq->unregister();
	}
	mm=0;
	arch_spinlock_free(&spin_lock);
	jfree_obj((unsigned long)this);
}
void futex::print_stats(unsigned char *arg1,unsigned char *arg2){

}
void futex::lock(spinlock_t *spin_lock){
	if (waitq ==0){
		return;
	}
	g_cpu_state[getcpuid()].task_on_wait = 1;
	waitq->wait_with_lock(50,spin_lock);
}
int futex::unlock(){
	int ret;
	if (waitq ==0){
		return 0;
	}
	ret =  waitq->wakeup();
	stat_wakeup_sucess = stat_wakeup_sucess +ret;
	return ret;
}


extern "C"{
#include "interface.h"
static int ipc_init_done=0;
int g_stat_idle_busy;
extern int sc_task_assign_to_cpu(struct task_struct *task);
unsigned long _schedule(unsigned long flags);
void sc_remove_dead_tasks();

static int find_futex(int *uaddr){
	int i;

	for (i=0; (i<MAX_FUTEXS) && (i<total_futexs); i++){
		if (i<total_futexs && futex_list[i]!=0 &&  futex_list[i]->mm==g_current_task->mm && futex_list[i]->uaddr==uaddr){
			return i;
		}
	}
	return -1;
}
static futex *get_futex(int *uaddr){ /* find the futex if not found create it */
	futex *p;
	futex *ret=0;
	int i,k;
	unsigned long irq_flags;

	i = find_futex(uaddr);
	if (i != -1) {
		futex_list[i]->stat_count++;
		return futex_list[i];
	}

	spin_lock_irqsave(&g_global_lock, irq_flags);
	for (i=0; i<MAX_FUTEXS && (i<total_futexs); i++){
		if (i<total_futexs && futex_list[i]==0){
			break;
		}
	}
	if (i < MAX_FUTEXS) {
		k = find_futex(uaddr);
		if (k != -1) {
			ret = futex_list[k];
		} else {
			p = jnew_obj(futex, uaddr);
			futex_list[i] = p;
			if (i >= total_futexs){
				total_futexs = i + 1;
			}
			ret = p;
		}
	}
	spin_unlock_irqrestore(&g_global_lock, irq_flags);

	return ret;
}
int destroy_futex(struct mm_struct *mm) {
	int i;
	for (i = 0; i < MAX_FUTEXS && (i<total_futexs); i++) {
		if (i < total_futexs && futex_list[i] != 0 && futex_list[i]->mm == mm) {
			futex_list[i]->destroy();
			futex_list[i] = 0;
		}
	}
}
/* TODO: other op need to implement
 *    hitting operation 0x85 is hitting  i.e : FUTEX_WAKE_OP with provate flag set is hitting in memcached
 */
int SYS_futex(int *uaddr, int op, int val, unsigned long timeout,
		unsigned long addr2, int val2) {
	unsigned long irq_flags;
	int retries=0;
	int ret=0;
	int op_ret;
	futex *futex_p;
	SYSCALL_DEBUG("futex uaddr: %x op:%x val:%x timeout:%x\n", uaddr, op, val,timeout);

	if ((op & FUTEX_CMD_MASK) ==0){

	}
	futex_p = get_futex(uaddr);
	if (futex_p != 0){
		ut_log("ERROR: futex value is zero \n");
		return -ENOSYS;
	}
	//ut_printf("futex uaddr: %x op:%x val:%x waits:%d wakeups:%d\n", uaddr, op, val,futex_p->stat_waits,futex_p->stat_wakeups);
	switch (op & FUTEX_CMD_MASK) {
	case FUTEX_WAIT:
//		  val2 = FUTEX_BITSET_MATCH_ANY;
//	case FUTEX_WAIT_BITSET:  /*TODO: needed for multi cpu application */
		//assert(timeout == 0);
		retries=0;
		while (retries < 1000) {
			if (*uaddr != val) {
				futex_p->stat_nowaits++;
				SYSCALL_DEBUG("futex uaddr: not match ret=0\n");
				return 0;
			}
			spin_lock(&futex_p->spin_lock);
			if (*uaddr == val) {
				futex_p->stat_waits++;
				futex_p->lock(&futex_p->spin_lock);
			} else {
				spin_unlock(&futex_p->spin_lock);
				futex_p->stat_lnowaits++;
				SYSCALL_DEBUG("futex uaddr: ret =0\n");
				return 0;
			}
			retries++;
		}
		SYSCALL_DEBUG("futex uaddr: count exceed ret=0\n");
		return 0;
	case FUTEX_WAKE_OP : /* TODO need to do additional operations */
		op_ret = futex_atomic_op_inuser(val2, (unsigned int *)addr2);
	case FUTEX_WAKE:
		spin_lock(&futex_p->spin_lock);
		futex_p->stat_wakeups_reqs++;
		ret = futex_p->unlock();
		/* wake up all the threads and reset to 1 */
		spin_unlock(&futex_p->spin_lock);
		return ret;
	default:
		ut_printf("ERROR : fake return= - %d Unimplemented futex() OP %x\n", ENOSYS,op);
		return -ENOSYS;
	}
	return -EINVAL;
}
/************************************  end of futex ******************************/
#define MAX_MUTEXS 100
static semaphore *stat_semaphores[MAX_MUTEXS];
void *ipc_mutex_create(char *name) {
	unsigned long flags,i;
	semaphore *sem ;
	assert (ipc_init_done !=0);

	sem = jnew_obj(semaphore, 1, name);
	sem->waitqueue->used_for = sem;
	sem->stat_acquired_start_time =0;

	spin_lock_irqsave(&g_global_lock, flags);
	for (i = 0; i < MAX_MUTEXS; i++) {
		if (stat_semaphores[i] != 0 ) continue;
		stat_semaphores[i] = sem;
		break;
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

	return sem;
}
int ipc_mutex_destroy(void *p) {
	unsigned long flags,i;
	semaphore *sem = p;
	if (p == 0)
		return 0;
	sem->free();

	spin_lock_irqsave(&g_global_lock, flags);
	for (i = 0; i < MAX_MUTEXS; i++) {
		if (stat_semaphores[i] == sem ) {
			stat_semaphores[i] = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
	return 1;
}
int ipc_mutex_lock(void *p, int line) {
	semaphore *sem = p;
	int ret;

	return sem->lock(line);
}
int ipc_mutex_unlock(void *p, int line) {
	semaphore *sem = p;
	sem->unlock(line);
}

int init_ipc(unsigned long arg){
	int i;

	ut_log("Initilizing the IPC ... \n");
	for (i = 0; i < MAX_MUTEXS; i++) {
		stat_semaphores[i] = 0;
	}
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		wait_queue::wait_queues[i] = 0;
	}
	for (i = 0; i < MAX_SPINLOCKS; i++) {
		g_spinlocks[i]=0;
	}
	ipc_init_done =1;
	return 1;
}

static int wakeup_cpus(int wakeup_cpu) {
	int i;
	int ret = 0;

	/* wake up specific cpu */
	if ((wakeup_cpu != -1)) {
		if (g_current_task->current_cpu == wakeup_cpu) return 0;
		//if (g_cpu_state[wakeup_cpu].current_task == g_cpu_state[wakeup_cpu].idle_task) {
		if ( g_cpu_state[wakeup_cpu].idle_state ==1){
			apic_send_ipi_vector(wakeup_cpu, IPI_INTERRUPT);
			return 1;
		} else {
			if ((g_cpu_state[wakeup_cpu].md_state.kernel_stack-TASK_SIZE) == g_cpu_state[wakeup_cpu].idle_task) {
				g_stat_idle_busy++;
			}
			return 0;
		}
	}

	/* wakeup all cpus */
	for (i = 0; i < getmaxcpus(); i++) {
		if (g_current_task->current_cpu != i
				&& g_cpu_state[i].idle_state ==1
				&& g_cpu_state[i].active) {
			apic_send_ipi_vector(i, IPI_INTERRUPT);
			ret++;
		}
	}
	return ret;
}

void _ipc_delete_from_waitqueues(struct task_struct *task) {
	int i;
	unsigned long flags;
	int assigned_to_running_cpu;

	if (task ==0) return;
	spin_lock_irqsave(&g_global_lock, flags);
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queue::wait_queues[i] == 0)
			continue;
		//spin_lock_irqsave(&wait_queue::wait_queues[i]->lock, flags);
		if (wait_queue::wait_queues[i]->_del_from_me(task)==JFAIL){
			//spin_unlock_irqrestore(&wait_queue::wait_queues[i]->lock, flags);
			continue;
		}
		//spin_unlock_irqrestore(&wait_queue::wait_queues[i]->lock, flags);
		break;
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
}
extern "C" {
unsigned long g_stat_timeout_wakeups ;
unsigned long g_stat_async_wakeups ;
}
void ipc_check_waitqueues() {
	int i;
	unsigned long flags;
	int assigned_to_running_cpu;
	int wakeup_cpu=-1;

	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queue::wait_queues[i] == 0){
			continue;
		}
		if (wait_queue::wait_queues[i]->head.next == &(wait_queue::wait_queues[i]->head)){/* empty queue */
			continue;
		}

		wakeup_cpu = -1;
		spin_lock_irqsave(&g_global_lock, flags);
		if (wait_queue::wait_queues[i]->head.next != &(wait_queue::wait_queues[i]->head)) {
			struct task_struct *task;
			task =list_entry(wait_queue::wait_queues[i]->head.next, struct task_struct, wait_queue);
			assert(task!=0);
			if ((task <  KADDRSPACE_START) || (task >KADDRSPACE_END)){
				BUG();
			}

			task->sleep_ticks--;
			if (task->sleep_ticks <= 0) {
				wait_queue::wait_queues[i]->_del_from_me(task);
				assigned_to_running_cpu = 0;
				if (task->run_queue.next == 0)
					assigned_to_running_cpu = sc_task_assign_to_cpu(task);
				else
					BUG();
				if (assigned_to_running_cpu == 0) {
					wakeup_cpu = task->allocated_cpu;
				}else{
					wait_queue::wait_queues[i]->stat_wakeon_samecpu++;
				}
			}
		}
		spin_unlock_irqrestore(&g_global_lock, flags);
		if (wakeup_cpu != -1){
			g_stat_timeout_wakeups++;
			wakeup_cpus(wakeup_cpu);
		}
	}

}

void ipc_release_resources(struct task_struct *task){
	int i;
	unsigned long flags;

	spin_lock_irqsave(&g_global_lock, flags);
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queue::wait_queues[i] == 0)
			continue;
		if (wait_queue::wait_queues[i]->used_for == 0) continue;
		semaphore *sem = wait_queue::wait_queues[i]->used_for;
		if (sem->owner_pid != task->task_id) continue;

		spin_unlock_irqrestore(&g_global_lock, flags);
		sem->signal();
		spin_lock_irqsave(&g_global_lock, flags);
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

}
/*********************************  end of Wait queue ***********************************/

#ifdef SPINLOCK_DEBUG
spinlock_t *g_spinlocks[MAX_SPINLOCKS];
int g_spinlock_count = 0;

int Jcmd_locks(char *arg1, char *arg2) {
	int i,k;
	unsigned long flags;
	struct list_head *pos;
	struct task_struct *task;
	int len,max_len;
	unsigned char *buf;

	ut_printf("SPIN LOCKS:  Name  pid count contention(rate) recursive# \n");
	for (i = 0; i < MAX_SPINLOCKS; i++) {
		unsigned long rate=0;
		if (g_spinlocks[i]==0) continue;
		if (g_spinlocks[i]->stat_locks !=0){
			rate = g_spinlocks[i]->contention / g_spinlocks[i]->stat_locks;
		}
		ut_printf(" %9s %3x %5d %5d(%d) %3d", g_spinlocks[i]->name,
				g_spinlocks[i]->pid, g_spinlocks[i]->stat_locks,
				g_spinlocks[i]->contention,
				rate,
				g_spinlocks[i]->stat_recursive_locks);
		for (k=0; k<getmaxcpus(); k++){
			ut_printf(" %d,",g_spinlocks[i]->stat_cpu_used[k]);
		}
		ut_printf("\n");
	}

	ut_printf("WAIT QUEUES: name: [owner pid] (wait_ticks/count:recursive_count) : waiting pid(name-line_no)\n");

	len = PAGE_SIZE*100;
	max_len=len;
	buf = (unsigned char *) vmalloc(len,0);
	if (buf == 0) {
		ut_printf(" Unable to get vmalloc memory \n");
		return 0;
	}
	spin_lock_irqsave(&g_global_lock, flags);
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		struct semaphore *sem=0;
		int recursive_count=0;
		int total_acquired_time=0;
		if (wait_queue::wait_queues[i] == 0)
			continue;
		if (wait_queue::wait_queues[i]->used_for == 0)
			len = len - ut_snprintf(buf+max_len-len,len," %9s : ", wait_queue::wait_queues[i]->name);
		else {
			len = len - ut_snprintf(buf+max_len-len,len,"[%9s]: ", wait_queue::wait_queues[i]->name);
			sem = wait_queue::wait_queues[i]->used_for;
			if (sem){
				recursive_count = sem->stat_recursive_count;
				total_acquired_time = sem->stat_total_acquired_time;
				//if (sem->owner_pid != 0)
					len = len - ut_snprintf(buf+max_len-len,len," [%x] ", sem->owner_pid);
			}
		}
		len = len - ut_snprintf(buf+max_len-len,len," (%d/%d:%d: AT:%d) ", wait_queue::wait_queues[i]->stat_wait_ticks,wait_queue::wait_queues[i]->stat_wait_count,recursive_count,total_acquired_time);

		if (len <= 0) break;
		list_for_each(pos, &wait_queue::wait_queues[i]->head) {
			int wait_line=0;
			task = list_entry(pos, struct task_struct, wait_queue);
			if (sem!=0) wait_line=task->stats.wait_line_no;
			len = len - ut_snprintf(buf+max_len-len,len,":%x(%s-%d),", task->task_id, task->name,wait_line);
		}

		len = len - ut_snprintf(buf+max_len-len,len,"\n");
		if (len <= 0) break;
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

	len = len - ut_snprintf(buf+max_len-len,len,"SEMAPORE name : locks  - contentions :- cont_time -[pid/recursivecount] \n");
	for (i = 0; i < MAX_MUTEXS; i++) {
		if (stat_semaphores[i] != 0 ) {
			len = len - ut_snprintf(buf+max_len-len,len,"%s: %d - %d : %d  [%d:%d]\n",stat_semaphores[i]->name,stat_semaphores[i]->stat_lock,stat_semaphores[i]->stat_contention,stat_semaphores[i]->stat_cont_time,stat_semaphores[i]->owner_pid,stat_semaphores[i]->recursive_count );
		}
	}

	ut_printf("%s",buf);
	vfree((unsigned long)buf);
	return 1;
}

#endif

/**********************************************/
#if 0
void ipc_test1() {
	int i;
	struct semaphore sem;

	ipc_sem_new(&sem, 1);
	ipc_sem_signal(&sem);
	for (i = 0; i < 40; i++) {
		ut_printf("Iterations  :%d  jiffes:%x\n", i, g_jiffies);
		ipc_sem_wait(&sem, 1000);
	}
	ipc_sem_free(&sem);
}
#endif
} /* end of c */


wait_queue *wait_queue::wait_queues[MAX_WAIT_QUEUES];
wait_queue::wait_queue( char *arg_name, unsigned long arg_flags) {
	int i;
	unsigned long irq_flags;

	spin_lock_irqsave(&g_global_lock, irq_flags);
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queues[i] == 0) {
			INIT_LIST_HEAD(&(head));
			name = arg_name;

			wait_queue::wait_queues[i] = this;
			ut_log("        Initalled new waitqueue at:%d %s flags:%x\n",i,arg_name,arg_flags);
			used_for = 0;
			flags = arg_flags;
			goto last;
		}
	}
last:
    spin_unlock_irqrestore(&g_global_lock, irq_flags);
}
/* TODO It should be called from all the places where sc_register_waitqueue is called, currently it is unregister is called only from few places*/
int wait_queue::unregister() {
	int i;
	unsigned long irq_flags;

	spin_lock_irqsave(&g_global_lock, irq_flags);
	for (i = 0; i < MAX_WAIT_QUEUES; i++) {
		if (wait_queue::wait_queues[i] == this) {
			/*TODO:  remove the tasks present in the queue */
			wait_queues[i] = 0;
			//ut_log(" UNINSTALLED  waitqueue at:%d \n",i);
			goto last;
		}
	}
last:
    spin_unlock_irqrestore(&g_global_lock, irq_flags);
    jfree_obj((unsigned long)this);
	return -1;
}
void wait_queue::_add_to_me( struct task_struct * p, long ticks) {
	long cum_ticks, prev_cum_ticks;
	struct list_head *pos, *prev_pos;
	struct task_struct *task;

	cum_ticks = 0;
	prev_cum_ticks = 0;

	assert (p != g_cpu_state[getcpuid()].idle_task ) ;

	if (head.next == &head) {
		p->sleep_ticks = ticks;
		list_add_tail(&p->wait_queue, &head);
	} else {
		prev_pos = &head;
		list_for_each(pos, &head) {
			task = list_entry(pos, struct task_struct, wait_queue);
			prev_cum_ticks = cum_ticks;
			cum_ticks = cum_ticks + task->sleep_ticks;

			if (cum_ticks > ticks) {
				p->sleep_ticks = (ticks - prev_cum_ticks);
				task->sleep_ticks = task->sleep_ticks - p->sleep_ticks;
				list_add(&p->wait_queue, prev_pos);
				goto last;
			}
			prev_pos = pos;
		}
		p->sleep_ticks = (ticks - cum_ticks);
		list_add_tail(&p->wait_queue, &head);
	}

last: ;

}
/* delete from the  wait queue */
int wait_queue::_del_from_me( struct task_struct *p) {
	struct list_head *pos;
	struct task_struct *task;
	int ret = JFAIL;

	if (p==0 ) return JFAIL;

	list_for_each(pos, &head) {
		task = list_entry(pos, struct task_struct, wait_queue);

		if (p == task) {
			if(pos == 0){
				BUG();
			}
			pos = pos->next;
			if (pos != &head) {
				if (p<0x100 || p->magic_numbers[0] != MAGIC_LONG){
					BUG();
				}
				task = list_entry(pos, struct task_struct, wait_queue);

				if ( task<0x100 || task>0xfffffffffffff000){
					BUG();
				}
				task->sleep_ticks = task->sleep_ticks + p->sleep_ticks;
			}
			p->sleep_ticks = 0;
			ret = JSUCCESS;
			list_del(&p->wait_queue);//TODO crash happening */
			return ret;
		}
	}

	return ret;
}
/* return the number of tasks dequeued */
int wait_queue::wakeup() {
	int ret = 0;
	struct task_struct *task;
	int assigned_to_running_cpu;
	unsigned long irq_flags;
	int wakeup_cpu = -1;

	if (head.next == &head ){
		return ret;
	}

	while (head.next != &head) {
		wakeup_cpu  = -1;
		spin_lock_irqsave(&g_global_lock, irq_flags);
		task = list_entry(head.next, struct task_struct, wait_queue);
		if (_del_from_me(task) == JSUCCESS) {
			assigned_to_running_cpu = 0;
			if (task->run_queue.next == 0 ){
				assigned_to_running_cpu = sc_task_assign_to_cpu(task);
			}else{
				BUG();
			}
			if (assigned_to_running_cpu == 0){
				int wk_ret;
				wakeup_cpu = task->allocated_cpu;
				stat_wakeups  = stat_wakeups + wk_ret;
			}
			ret++;
		}
		spin_unlock_irqrestore(&g_global_lock, irq_flags);
		if (wakeup_cpu != -1){
			g_stat_async_wakeups++;
			wakeup_cpus(wakeup_cpu);
		}

		if ((ret > 0) && (flags & WAIT_QUEUE_WAKEUP_ONE)){
			return ret;
		}
	}
	return ret;
}
/* ticks in terms of 10 ms */
int wait_queue::wait_internal(unsigned long ticks, spinlock_t *spin_lock) {
	unsigned long intr_flags;

	local_irq_save(intr_flags);
	spin_lock(&g_global_lock);  /* this lock will be released in Schedule */
	g_cpu_state[getcpuid()].sched_lock = &g_global_lock;

	if (g_current_task->state == TASK_NONPREEMPTIVE){
		BUG();
	}
	g_current_task->state = TASK_INTERRUPTIBLE;
	ut_snprintf(g_current_task->status_info,MAX_TASK_STATUS_DATA ,"WAIT-%s: %d",name,ticks);
	g_current_task->stats.wait_start_tick_no = g_jiffies ;
	stat_wait_count++;
	_add_to_me(g_current_task, ticks);

	if (spin_lock != 0){
		spin_unlock(spin_lock);
	}

	sc_schedule();
	local_irq_restore(intr_flags);

	g_current_task->status_info[0] = 0;
	stat_wait_ticks = stat_wait_ticks + (g_jiffies-g_current_task->stats.wait_start_tick_no);
	if (g_current_task->sleep_ticks <= 0)
		return 0;
	else
		return g_current_task->sleep_ticks;
}
int wait_queue::wait(unsigned long ticks) {
	return wait_internal(ticks,0);
}
int wait_queue::wait_with_lock(unsigned long ticks, spinlock_t *spin_lock) {
	return wait_internal(ticks,spin_lock);
}
void wait_queue::print_stats(unsigned char *arg1,unsigned char *arg2){
	ut_printf("name:%s ticks:%d count:%d wakeups:%d \n",name,stat_wait_ticks,stat_wait_count,stat_wakeups);
}
/******************************************************************************************/
/* this call consume system resources */
semaphore::semaphore(uint8_t arg_count, char *arg_name) {
	ut_snprintf(name,IPC_NAME_MAX,"SEM:%s",arg_name);
	owner_pid = 0;
	count = arg_count;
	arch_spinlock_init(&sem_lock, (unsigned char *)name);
	valid_entry = 1;
	waitqueue = jnew_obj(wait_queue, name ,WAIT_QUEUE_WAKEUP_ONE);
}
/* Signals a semaphore. */
void semaphore::signal() {
	unsigned long irq_flags;

	spin_lock_irqsave(&(sem_lock), irq_flags);
	count++;
	spin_unlock_irqrestore(&(sem_lock), irq_flags);
	waitqueue->wakeup();
}

uint32_t semaphore::wait(uint32_t timeout_arg){ /* timeout_arg in ms */
	unsigned long flags;
	unsigned long timeout;

	timeout = timeout_arg / 10;
	while (1) {
		if (count <= 0) {
			stat_contention++;
			timeout = waitqueue->wait(timeout);
		}
		if (timeout_arg == 0)
			timeout = 100;

		spin_lock_irqsave(&(sem_lock), flags);
		/* Atomically check that we can proceed */
		if (count > 0 || (timeout <= 0))
			break;
		spin_unlock_irqrestore(&(sem_lock), flags);

		if (g_current_task->killed == 1){
			return IPC_TIMEOUT;
		}
	}

	if (count > 0) {
		count--;
		spin_unlock_irqrestore(&(sem_lock), flags);
		stat_lock++;
		return 1;
	}

	spin_unlock_irqrestore(&(sem_lock), flags);
	return IPC_TIMEOUT;
}
int semaphore::lock(int line) {

	int ret;
	unsigned long stat_arrive_time;
	if ((owner_pid == g_current_task->task_id) && recursive_count != 0){
		//ut_log("mutex_lock Recursive mutex: thread:%s  count:%d line:%d \n",g_current_task->name,sem->recursive_count,line);
		recursive_count++;
		stat_recursive_count++;
		return 1;
	}
	assert (g_current_task->locks_nonsleepable == 0);

	g_current_task->stats.wait_line_no = line;
	ret = 0;
	stat_arrive_time = g_jiffies;
	while (ret != 1){
		ret = wait(10000);
	}
	stat_cont_time = g_jiffies - stat_arrive_time;

	g_current_task->status_info[0] = 0;
	owner_pid = g_current_task->task_id;
	recursive_count =1;
	g_current_task->locks_sleepable++;
	assert (g_current_task->locks_nonsleepable == 0);

	stat_line = line;
	stat_acquired_start_time = g_jiffies;
	return 1;
}
int semaphore::unlock(int line) {

	if ((owner_pid == g_current_task->task_id) && recursive_count > 1){
		//ut_log("mutex_unlock Recursive mutex: thread:%s  recursive count:%d line:%d \n",g_current_task->name,sem->recursive_count,line);
		recursive_count--;
		return 1;
	}
	owner_pid = 0;
	recursive_count =0;
	g_current_task->locks_sleepable--;
	stat_line = -line;
	stat_total_acquired_time += (g_jiffies-stat_acquired_start_time);
	signal();

	return 1;
}
/* Deallocates a semaphore. */
void semaphore::free() {
	arch_spinlock_free(&(sem_lock));
	waitqueue->unregister();
    jfree_obj((unsigned long)this);
}
void semaphore::print_stats(unsigned char *arg1,unsigned char *arg2){
ut_printf(" name:%s \n",name);
}

