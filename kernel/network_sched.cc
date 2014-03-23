/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/network_sched.cc
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 */

/*  Description:
 *   net_sched --> {  socket_layer->tcpip_stack, jdevice }
 *
 *   sendpath from app :   app-> send -> socket -> tcpip_stack(send) -> jdevice -> jdriver
 *   recvpath from app :   app-> recv -> socket(blocking)
 *                     :   app-> recv -> socket -> tcpip_stack(recv)
 *   recvpath from dev (interrupt) :   jdriver -> netif_rx (to queue)
 *                   (polling)  : thread-> (from queue) netif_BH -> socket (attach_raw : unblocking + buffering)
 *      netif_BH and interrupt on the same cpu to minimize the lock.
 *
 * Network:speeding the network using the Vanjacbson paper
        IST+softnet+socket: currently most of the protocol processing done in softnet centrally with the global lock)
        Instead: Push all the work to the edge , and run in the app context , so that locking will be minimum.
          On multiprocessor, the protocol work should be done on the processor that going to consume the data. This means ISR and softnet should do almost nothing and socket should do everything.
         a) socket register signature with driver during the accept call for reaching the packet directly. this is channel for each socket.
         b) driver queues the packet with matching signature into socket channel & wakesup the app if sleeping in the socket layer. packet is processed in the socket layer by the app.
      https://lwn.net/Articles/169961/
      http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf
 *
 */

extern "C" {
#include "common.h"
int g_network_ip=0x0ad18100; /* 10.209.129.0  */
unsigned char g_mac[7];
}
#include "jdevice.h"
#include "file.hh"
#include "network.hh"
#define MAX_QUEUE_LENGTH 600

struct queue_struct {
	wait_queue_t waitq;
	int producer, consumer;
	struct {
		unsigned char *buf;
		unsigned int len; /* actual data length , not the buf lenegth, buf always constant length */
	} data[MAX_QUEUE_LENGTH];
	spinlock_t spin_lock; /* lock to protect while adding and revoing from queue */
	int stat_processed[MAX_CPUS];
	int queue_len;
	int error_full;
};

#define MAX_POLL_DEVICES 5
struct device_under_poll_struct{
	void *private_data;
	int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts);
	int active;
};

class network_scheduler {
	 int network_enabled;
	 struct queue_struct queue;
	 struct device_under_poll_struct device_under_poll[MAX_POLL_DEVICES];
	 int poll_underway ;
	 void  *g_netBH_lock ; /* All BH code will serialised by this lock */
	 int stat_netrx_bh_recvs;
	 int poll_devices();

public :
	jdevice *device;
	int init();
	int add_to_queue(unsigned char *buf, int len, unsigned char **replace_outbuf);
	int remove_from_queue(unsigned char **buf, unsigned int *len, unsigned char *replace_inbuf);
	int netRx_BH(void *arg,void *arg2);
	int netif_rx(unsigned char *data, unsigned int len, unsigned char **replace_buf);
	int netif_rx_enable_polling(void *private_data,
			int (*poll_func)(void *private_data, int enable_interrupt,int total_pkts));
};

static network_scheduler net_sched;
static int stat_from_driver = 0;
static int stat_to_driver = 0;
/***********************************************************************
 *  1) for every buf is added will get back a replace buf
 *  2) for every buf removed will given back the replace buf.
 *  Using 1) and 2) techniques allocation and free  is completely avoided from driver to queue and vice versa.
 *  the Cost is the queue should be prefilled with some buffers.
 *
 *    path: netif_rx -> add_to_queue -> netRx_BH
 *
 *
 */
static unsigned char *get_freebuf(){
	/* size of buf is hardcoded to 4096, this is linked to network driverbuffer size */
	return (unsigned char *)alloc_page(0);
}
int network_scheduler::add_to_queue(unsigned char *buf, int len, unsigned char **replace_outbuf) {
	unsigned long flags;
	int ret=0;
	if (buf == 0 || len == 0)
		return ret;

	//ut_log("Recevied from  network and keeping in queue: len:%d  stat count:%d prod:%d cons:%d\n",len,stat_queue_len,queue.producer,queue.consumer);
	spin_lock_irqsave(&(queue.spin_lock), flags);
	if (queue.data[queue.producer].len == 0) {
		queue.data[queue.producer].len = len;
		*replace_outbuf = queue.data[queue.producer].buf;
		queue.data[queue.producer].buf = buf;
		queue.producer++;
		queue.queue_len++;
		if (queue.producer >= MAX_QUEUE_LENGTH)
			queue.producer = 0;
		ret = 1;
		goto last;
	}
	queue.error_full++;
last:
	spin_unlock_irqrestore(&(queue.spin_lock), flags);
	return ret;
}

int network_scheduler::remove_from_queue(unsigned char **buf, unsigned int *len, unsigned char *replace_inbuf) {
	unsigned long flags;
	int ret=0;
	spin_lock_irqsave(&(queue.spin_lock), flags);
	if ((queue.data[queue.consumer].buf != 0)
			&& (queue.data[queue.consumer].len != 0)) {
		*buf = queue.data[queue.consumer].buf;
		*len = queue.data[queue.consumer].len;
		//	ut_log("netrecv : receving from queue len:%d  prod:%d cons:%d\n",queue.data[queue.consumer].len,queue.producer,queue.consumer);

		queue.data[queue.consumer].len = 0;
		queue.data[queue.consumer].buf = replace_inbuf;
		queue.consumer++;
		queue.queue_len--;
		queue.stat_processed[getcpuid()]++;
		if (queue.consumer >= MAX_QUEUE_LENGTH)
			queue.consumer = 0;
		ret  = 1;
	}
	spin_unlock_irqrestore(&(queue.spin_lock), flags);
	return ret;
}

/*   Release the buffer after calling the callback */
int network_scheduler::netRx_BH(void *arg,void *arg2) {
	unsigned char *data;
	unsigned int len;
	int qret;
	unsigned char *replace_buf = get_freebuf();

	while (1) {
		poll_devices();

		qret = remove_from_queue(&data, &len, replace_buf);
		if (qret ==0 ){
			ipc_waiton_waitqueue(&queue.waitq, 1000);
			qret = remove_from_queue(&data, &len, replace_buf);
		}

		while ( qret == 1) {
			int ret = 0;
			stat_netrx_bh_recvs++;
			if (socket::attach_rawpkt(data,len,&replace_buf)==JSUCCESS){

			}else{
				replace_buf = data;
			}
			qret = remove_from_queue(&data, &len, replace_buf);
		}
	}
	return 1;
}
int network_scheduler::poll_devices() {
	int i,j;
	int ret = 0;


	mutexLock(g_netBH_lock);
	if (poll_underway == 0) {
		poll_underway = 1;
		ret = 1;
	}
	mutexUnLock(g_netBH_lock);
	if (ret == 0)
		return ret;

	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data != 0
				&& device_under_poll[i].active == 1) {
			int max_pkts=50;
			int total_pkts=0;
			int pkts=0;
			for (j=0; j<20; j++){
				pkts = device_under_poll[i].poll_func(device_under_poll[i].private_data, 0, max_pkts);
				if (pkts==0 ) break;
				total_pkts = total_pkts+pkts;
			}
			if (total_pkts == 0){
				device_under_poll[i].active = 0;
				device_under_poll[i].poll_func(device_under_poll[i].private_data, 1, max_pkts); /* enable interrupts */
			}else{
				if (total_pkts > 2){ /* if there are more then 2 unprocessed packets wake up any sleeping thread to picku[p */
					ipc_wakeup_waitqueue(&queue.waitq);
				}
			}
		}
	}
	poll_underway = 0;
	return 1;
}
int network_scheduler::netif_rx(unsigned char *data, unsigned int len, unsigned char **replace_buf) {
	if (network_enabled==0){
		*replace_buf = data;
		return 0;
	}
	if (add_to_queue(data, len,replace_buf) == 0 && data != 0 && len != 0) {
		*replace_buf = data; /* fail to queue, so the packet is getting dropped and freed  */
	}else{
		ipc_wakeup_waitqueue(&queue.waitq); /* wake all consumers , i.e all the netBH */
	}
	stat_from_driver++;
	if (*replace_buf < (unsigned char *)0x10000){ /* invalid address*/
		*replace_buf=get_freebuf();
	}
	return 1;
}
int network_scheduler::netif_rx_enable_polling(void *private_data,
		int (*poll_func)(void *private_data, int enable_interrupt,
				int total_pkts)) {
	int i;
	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data == 0 || device_under_poll[i].private_data == private_data) {
			device_under_poll[i].private_data = private_data;
			device_under_poll[i].poll_func = poll_func;
			device_under_poll[i].active = 1;
			ipc_wakeup_waitqueue(&queue.waitq);
			return 1;
		}
	}
	return 0;
}
int network_scheduler::init() {
	unsigned long pid;
	int i;

	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		device_under_poll[i].private_data = 0;
		device_under_poll[i].active = 0;
	}
	ut_memset((unsigned char *) &queue, 0, sizeof(struct queue_struct));
	for (i=0; i<MAX_QUEUE_LENGTH; i++){
		queue.data[i].buf = get_freebuf();
		queue.data[i].len = 0; /* this represent actual data length */
		if (queue.data[i].buf == 0){
			BUG();
		}
	}
	queue.spin_lock = SPIN_LOCK_UNLOCKED("netq_lock");
	ipc_register_waitqueue(&queue.waitq, "netRx_BH",WAIT_QUEUE_WAKEUP_ONE);
	//ipc_register_waitqueue(&queue.waitq, "netRx_BH",0);
	g_netBH_lock = mutexCreate("mutex_netBH");
	poll_underway =0;
	network_enabled = 1;
	return JSUCCESS;
}
int register_netdevice(jdevice *device){
	net_sched.device = device;
	socket::net_dev = device;
	device->ioctl(0,(unsigned long)&g_mac);

	g_network_ip = g_network_ip | g_mac[5];
	ut_log(" register netdev : %x  ip:%x\n",g_mac[5],g_network_ip);
	return JSUCCESS;
}
extern "C" {
extern int init_udpstack();
int netif_thread(void *arg1,void *arg2){
	return net_sched.netRx_BH(arg1,arg2);
}
int init_networking() {
	int pid;

	net_sched.init();
	pid = sc_createKernelThread(netif_thread, 0, (unsigned char *) "netRx_BH_1",0);
#ifdef JINY_UDPSTACK
	init_udpstack();
#endif

	return 0;
}

int netif_rx(unsigned char *data, unsigned int len, unsigned char **replace_buf) {
	return net_sched.netif_rx(data,len,replace_buf);
}
int netif_rx_enable_polling(void *private_data, int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts)) {
	return net_sched.netif_rx_enable_polling(private_data,poll_func);
}
extern struct Socket_API *socket_api;

void Jcmd_network(unsigned char *arg1, unsigned char *arg2) {
	if (net_sched.device){
		unsigned char mac[10];

		net_sched.device->print_stats();
		net_sched.device->ioctl(0,(unsigned long)&mac);
		ut_printf(" mac: %x:%x:%x:%x:%x:%x  ip:%x\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],g_network_ip);
	}
	socket::print_stats();

	return;
}
}
