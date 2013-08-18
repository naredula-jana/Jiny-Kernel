/* 
 * lwip-arch.c
 *
 * Arch-specific semaphores and mailboxes for lwIP running on mini-os 
 *
 * Tim Deegan <Tim.Deegan@eu.citrix.net>, July 2007
 *
 */

#include <stdarg.h>
#include "interface.h"
#include "arch/sys_arch.h"

/*************************** TODO below copied from lwip **************/
typedef void (*sys_timeout_handler)(void *arg);

struct sys_timeo {
	struct sys_timeo *next;
	unsigned int time;
	sys_timeout_handler h;
	void *arg;
};

struct sys_timeouts {
	struct sys_timeo *next;
};




//#define MBOX_DEBUG 1
/******************************* below copied from sys *****************/
#define SYS_ARCH_TIMEOUT 0xffffffffUL
#define SYS_MBOX_EMPTY SYS_ARCH_TIMEOUT
/* Definitions for error constants. */

#define ERR_OK          0    /* No error, everything OK. */
#define ERR_MEM        -1    /* Out of memory error.     */
#define ERR_BUF        -2    /* Buffer error.            */
#define ERR_TIMEOUT    -3    /* Timeout.                 */
#define ERR_RTE        -4    /* Routing problem.         */
#define ERR_INPROGRESS -5    /* Operation in progress    */
#define ERR_VAL        -6    /* Illegal value.           */
#define ERR_WOULDBLOCK -7    /* Operation would block.   */
#define ERR_USE        -8    /* Address in use.          */
#define ERR_ISCONN     -9    /* Already connected.       */

#define ERR_IS_FATAL(e) ((e) < ERR_ISCONN)

#define ERR_ABRT       -10   /* Connection aborted.      */
#define ERR_RST        -11   /* Connection reset.        */
#define ERR_CLSD       -12   /* Connection closed.       */
#define ERR_CONN       -13   /* Not connected.           */

#define ERR_ARG        -14   /* Illegal argument.        */

#define ERR_IF         -15   /* Low-level netif error    */
/************************************************************/

sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);
#define up sys_sem_signal

/* Is called to initialize the sys_arch layer */
void sys_init(void) {
}
uint32_t sys_arch_sem_wait(sys_sem_t *sem, uint32_t timeout_arg);
uint32_t sys_arch_sem_wait(sys_sem_t *sem, uint32_t timeout_arg){
	uint32_t ret;
	int net_locked=0;

	sys_sem_t *net_sem = g_netBH_lock;
	if (net_sem && (net_sem->owner_pid == g_current_task->pid)){
		net_locked=1;
		mutexUnLock(g_netBH_lock);
	}

	ret = ipc_sem_wait(sem,timeout_arg);

	if (net_locked==1){
		mutexLock(g_netBH_lock);
	}

	return ret;
}
/* Creates an empty mailbox. */
signed char sys_mbox_new(sys_mbox_t *mbox, int size) {

	if (!size)
		size = 32;
	else if (size == 1)
		size = 2;
	mbox->count = size + 1; /* this is to create one empty slot , so as always  writer and reader does not meet*/
	mbox->messages = mm_malloc(sizeof(void*) * (size + 1), 0);
	mbox->read_sem.name =  "sem_lwip_read" ;
	ipc_sem_new(&mbox->read_sem, 0);
	mbox->reader = 0;
	mbox->write_sem.name =  "sem_lwip_write";
	ipc_sem_new(&mbox->write_sem, size);
	mbox->writer = 0;
	mbox->valid_entry = 1;
	return 0; /* should return zero if everything is ok */
}

/* Deallocates a mailbox. If there are messages still present in the
 * mailbox when the mailbox is deallocated, it is an indication of a
 * programming error in lwIP and the developer should be notified. */
void sys_mbox_free(sys_mbox_t *mbox) {
	mm_free(mbox->messages);
	sys_sem_free(&mbox->read_sem);
	sys_sem_free(&mbox->write_sem);
}
int sys_mbox_valid(sys_mbox_t *mbox){
	return mbox->valid_entry;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox){
	mbox->valid_entry=0;
}
/* Posts the "msg" to the mailbox, internal version that actually does the
 * post. */
static void do_mbox_post(sys_mbox_t *mbox, void *msg) {
	/* The caller got a semaphore token, so we are now allowed to increment
	 * writer, but we still need to prevent concurrency between writers
	 * (interrupt handler vs main) */

#if MBOX_DEBUG
	ut_log("mbox post: %x  write:%d read:%d \n",mbox,mbox->writer,mbox->reader);
#endif
	sys_prot_t prot = sys_arch_protect();
	mbox->messages[mbox->writer] = msg;
	mbox->writer = (mbox->writer + 1) % mbox->count;
	assert(mbox->reader != mbox->writer);
	sys_arch_unprotect(prot);
	sys_sem_signal(&mbox->read_sem);
}

/* Posts the "msg" to the mailbox. */
void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
	uint32_t ret;

	if (mbox == SYS_MBOX_NULL)
		return;
	ret = IPC_TIMEOUT;
	while (ret == IPC_TIMEOUT) {
		ret = sys_arch_sem_wait(&mbox->write_sem, 1000);
	}

	do_mbox_post(mbox, msg);
}

/* Try to post the "msg" to the mailbox. */
signed char  sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
	if (mbox == SYS_MBOX_NULL)
		return ERR_BUF;
	if (sys_arch_sem_wait(&mbox->write_sem, 1) == IPC_TIMEOUT)
		return ERR_MEM;
	do_mbox_post(mbox, msg);
	return ERR_OK;
}

/*
 * Fetch a message from a mailbox. Internal version that actually does the
 * fetch.
 */
static void do_mbox_fetch(sys_mbox_t *mbox, void **msg) {
	sys_prot_t prot;
	/* The caller got a semaphore token, so we are now allowed to increment
	 * reader, but we may still need to prevent concurrency between readers.
	 * FIXME: can there be concurrent readers? */
#if MBOX_DEBUG
	ut_log("mbox fetch: %x  write:%d read:%d \n",mbox,mbox->writer,mbox->reader);
#endif
	prot = sys_arch_protect();
	assert(mbox->reader != mbox->writer);
	// TODO : as Hit once
	if (msg != NULL)
		*msg = mbox->messages[mbox->reader];
	mbox->reader = (mbox->reader + 1) % mbox->count;
	sys_arch_unprotect(prot);
	up(&mbox->write_sem);
}

/* Blocks the thread until a message arrives in the mailbox, but does
 * not block the thread longer than "timeout" milliseconds (similar to
 * the sys_arch_sem_wait() function). The "msg" argument is a result
 * parameter that is set by the function (i.e., by doing "*msg =
 * ptr"). The "msg" parameter maybe NULL to indicate that the message
 * should be dropped.
 *
 * The return values are the same as for the sys_arch_sem_wait() function:
 * Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
 * timeout. */
uint32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, uint32_t timeout) {
	uint32_t rv;
	if (mbox == SYS_MBOX_NULL)
		return SYS_ARCH_TIMEOUT;

	rv = sys_arch_sem_wait(&mbox->read_sem, timeout);
	if (rv == SYS_ARCH_TIMEOUT)
		return rv;

	do_mbox_fetch(mbox, msg);
	return 0;
}

/* This is similar to sys_arch_mbox_fetch, however if a message is not
 * present in the mailbox, it immediately returns with the code
 * SYS_MBOX_EMPTY. On success 0 is returned.
 *
 * To allow for efficient implementations, this can be defined as a
 * function-like macro in sys_arch.h instead of a normal function. For
 * example, a naive implementation could be:
 *   #define sys_arch_mbox_tryfetch(mbox,msg) \
 *     sys_arch_mbox_fetch(mbox,msg,1)
 * although this would introduce unnecessary delays. */

uint32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
	if (mbox == SYS_MBOX_NULL)
		return SYS_ARCH_TIMEOUT;

	if (sys_arch_sem_wait(&mbox->read_sem, 1) == SYS_ARCH_TIMEOUT)
		return SYS_MBOX_EMPTY;

	do_mbox_fetch(mbox, msg);
	return 0;
}

/* Returns a pointer to the per-thread sys_timeouts structure. In lwIP,
 * each thread has a list of timeouts which is repressented as a linked
 * list of sys_timeout structures. The sys_timeouts structure holds a
 * pointer to a linked list of timeouts. This function is called by
 * the lwIP timeout scheduler and must not return a NULL value. 
 *
 * In a single threadd sys_arch implementation, this function will
 * simply return a pointer to a global sys_timeouts variable stored in
 * the sys_arch module. */
/*struct sys_timeouts *sys_arch_timeouts(void)
 {
 static struct sys_timeouts timeout;
 return &timeout;
 }*/

/* Starts a new thread with priority "prio" that will begin its execution in the
 * function "thread()". The "arg" argument will be passed as an argument to the
 * thread() function. The id of the new thread is returned. Both the id and
 * the priority are system dependent. */
static struct thread *lwip_thread;
sys_thread_t sys_thread_new(char *name, void (*thread)(void *arg), void *arg,
		int stacksize, int prio) {
	unsigned long pid;

	pid = sc_createKernelThread(thread, (unsigned char *)arg, (unsigned char *)name);
//	sc_task_stick_to_cpu(pid, 0); /* TODO: currently all network related threads are sticked to cpu-o to avoid crash in tcp layer */
	DEBUG(" Thread created for tcp/ip: %d:\n", pid);
	return pid;
}

/* This optional function does a "fast" critical region protection and returns
 * the previous protection level. This function is only called during very short
 * critical regions. An embedded system which supports ISR-based drivers might
 * want to implement this function by disabling interrupts. Task-based systems
 * might want to implement this by using a mutex or disabling tasking. This
 * function should support recursive calls from the same task or interrupt. In
 * other words, sys_arch_protect() could be called while already protected. In
 * that case the return value indicates that it is already protected.
 *
 * sys_arch_protect() is only required if your port is supporting an operating
 * system. */
sys_prot_t sys_arch_protect(void) {
	unsigned long flags;
	local_irq_save(flags);
	return flags;
}

/* This optional function does a "fast" set of critical region protection to the
 * value specified by pval. See the documentation for sys_arch_protect() for
 * more information. This function is only required if your port is supporting
 * an operating system. */
void sys_arch_unprotect(sys_prot_t pval) {
	local_irq_restore(pval);
}

/* non-fatal, print a message. */
void lwip_printk(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printk("lwIP: ");
	DEBUG(0, fmt, args);
	va_end(args);
}

/* fatal, print message and abandon execution. */
void lwip_die(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printk("lwIP assertion failed: ");
	DEBUG(0, fmt, args);
	va_end(args);
	printk("\n");
	BUG();
}

/* Returns a pointer to the per-thread sys_timeouts structure. In lwIP,
 * each thread has a list of timeouts which is repressented as a linked
 * list of sys_timeout structures. The sys_timeouts structure holds a
 * pointer to a linked list of timeouts. This function is called by
 * the lwIP timeout scheduler and must not return a NULL value.
 *
 * In a single threadd sys_arch implementation, this function will
 * simply return a pointer to a global sys_timeouts variable stored in
 * the sys_arch module. */
struct sys_timeouts *sys_arch_timeouts(void) {
	static struct sys_timeouts timeout;
	return &timeout;
}

int sio_open(int i, int j) {
	DEBUG(
			"ERROR .... sio_send called , not supposed to be called , this is a fix to compilation \n");
	return 1;
}
int sio_recv(int i, int j) {
	DEBUG(
			"ERROR .... sio_send called , not supposed to be called , this is a fix to compilation \n");
	return 1;
}
int sio_send(int i, int j) {
	DEBUG(
			"ERROR .... sio_send called , not supposed to be called , this is a fix to compilation \n");
	return 1;
}

/************************************************************************** TEST **********************/
struct mbox tbox;
int test_init() {
	static int init = 0;
	if (init == 0) {
		init = 1;
		sys_mbox_new(&tbox,30);
	}
	return 1;
}

int test_prod(char *arg1, char *arg2) {
	static unsigned long prod = 1;
	signed char ret;
	test_init();
	 sys_mbox_trypost(&tbox, prod);

	if (ret == 0) {
		DEBUG("Sucessfully produces :%d \n", prod);
		prod++;
	} else {
		DEBUG(" ERROR : failed to produced :%d :%d \n", ret, prod);
	}
	return 1;
}
int test_cons(char *arg1, char *arg2) {
	int ret;
	unsigned long cons;

	test_init();
	sys_arch_mbox_tryfetch(&tbox, &cons);

	if (ret == 0) {
		DEBUG("Sucessfully consumed :%d \n", cons);

	} else {
		DEBUG(" ERROR : failed to consumed :%d :%d \n", ret, cons);
	}
	return 1;
}
unsigned int LWIP_RAND(){
	static unsigned int i=1;
	i=i*2;
	return i;
}
