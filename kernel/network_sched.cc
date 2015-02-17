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
 *   recvpath from dev (interrupt) :   jdriver -> netif_rx_enable_polling(enable polling)
 *                   (polling)  : thread-> (from device queue) netif_BH -> socket (attach_raw : unblocking + buffering)
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
extern int net_bh();
unsigned char g_mac[7];
int g_conf_nic_intr_off=0;
//int g_conf_send_alltime=0;
}
#include "jdevice.h"
#include "file.hh"
#include "network.hh"

network_scheduler net_sched;
static int stat_from_driver = 0;
static int stat_to_driver = 0;
static int net_bh_active = 0;

extern int g_conf_net_sendbuf_delay;
/*   Release the buffer after calling the callback */
int network_scheduler::netRx_thread(void *arg, void *arg2) {
	while(1){
		net_bh();
		netrx_cpuid = getcpuid();
		g_cpu_state[netrx_cpuid].net_BH = 0;
		if (g_conf_net_sendbuf_delay != 0) { /* if sendbuf_delay is enabled, smp is having negative impact and rate of recving falls down */
			if (device != 0) {
				device->ioctl(NETDEV_IOCTL_FLUSH_SENDBUF, 0);
			}
			waitq->wait(50);
		} else {
			waitq->wait(10000);
		}
	}
}
int network_scheduler::netRx_BH() {
	int ret =1 ;

	ret = poll_devices(0);

	return 1;
}
int network_scheduler::poll_devices(int enable_interrupt) {
	int i, j;
	int ret = 0;
	int pending_devices = 0;

	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data != 0 && device_under_poll[i].active == 1) {
			int max_pkts = 50;
			int total_pkts = 0;
			int pkts = 0;
			for (j = 0; j < 2000; j++) {
				pkts = device_under_poll[i].poll_func(device_under_poll[i].private_data, 0, max_pkts);
				if (pkts == 0)
					break;
				total_pkts = total_pkts + pkts;
			}
			if (total_pkts == 0) {
				//device_under_poll[i].active = 0;
				device_under_poll[i].poll_func(device_under_poll[i].private_data, enable_interrupt, max_pkts); /* enable interrupts */
			} else {
				pending_devices++;
			}
		}
	}
	poll_underway = 0;
	return pending_devices;
}
int network_scheduler::netif_rx(unsigned char *data, unsigned int len) {
	if (socket::attach_rawpkt(data, len) == JFAIL) {
		jfree_page(data);
	}
	return JSUCCESS;
}
int network_scheduler::netif_rx_enable_polling(void *private_data,
		int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts)) {
	int i;
	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data == 0 || device_under_poll[i].private_data == private_data) {
			device_under_poll[i].private_data = private_data;
			device_under_poll[i].poll_func = poll_func;
			device_under_poll[i].active = 1;
		//	g_cpu_state[netrx_cpuid].net_BH = 1;
			net_bh_active =1;
		//	waitq->wakeup();
			if (g_cpu_state[1].idle_state == 1){
			//	apic_send_ipi_vector(1, IPI_INTERRUPT);
			}
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

	waitq = jnew_obj(wait_queue,"netRx_BH", WAIT_QUEUE_WAKEUP_ONE);
	g_netBH_lock = mutexCreate("mutex_netBH");
	poll_underway = 0;
	network_enabled = 1;
	return JSUCCESS;
}

int register_netdevice(jdevice *device) {
	net_sched.device = device;
	socket::net_dev = device;
	device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &g_mac);
	if (g_conf_nic_intr_off ==1){
		device->ioctl(NETDEV_IOCTL_DISABLE_RECV_INTERRUPTS, 0);
	}
	ut_log(" register netdev : %x \n", g_mac[5]);
	return JSUCCESS;
}

extern "C" {
static int process_send_queue();
extern int init_udpstack();
extern int init_netmod_uipstack();
spinlock_t netbh_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"netbh_lock");
extern unsigned long  get_100usec();
/* TODO : currently it is assum,ed for 1 nic, later this need to be extended for multiple nics*/
int net_bh(){
	unsigned long flags;
	static int netbh_in_progress=0;
	static unsigned long last_timestamp=0; /* 100usec units */
	unsigned long curr_time; /* interms of 100usec*/

	process_send_queue();

	if (g_conf_nic_intr_off == 1) {  /* only in the poll mode */
		curr_time = get_100usec(); /* interms of 100 usec units*/
		if (last_timestamp < curr_time) {
			last_timestamp = curr_time;
			net_bh_active = 1;
		}
	}

	if (netbh_in_progress == 1 ||  (net_bh_active == 0)) return 0;

	/* only one cpu will be in net_bh */
	spin_lock_irqsave(&netbh_lock, flags);
	if (netbh_in_progress == 0){
		netbh_in_progress = 1;
		spin_unlock_irqrestore(&netbh_lock, flags);
	}else{
		spin_unlock_irqrestore(&netbh_lock, flags);
		return 0;
	}

	do {
		net_bh_active++;
		net_sched.netRx_BH();
		g_cpu_state[getcpuid()].stats.netbh++;
	}while (net_bh_active == 1);

	if (net_bh_active > 10){
		net_bh_active =0;
	}
	if (g_conf_nic_intr_off == 0 && net_sched.device){
		net_bh_active =0;
		net_sched.device->ioctl(NETDEV_IOCTL_ENABLE_RECV_INTERRUPTS, 0);
		net_sched.netRx_BH();
	}
	netbh_in_progress = 0;

	return 1;
}
int netif_thread(void *arg1, void *arg2) {
	return net_sched.netRx_thread(arg1, arg2);
}

int init_networking() {
	int pid;
	net_sched.init();
	//pid = sc_createKernelThread(netif_thread, 0, (unsigned char *) "netRx_BH_1", 0);
	socket::init_socket_layer();
	return JSUCCESS;
}
int init_network_stack() { /* this should be initilised once the devices are up */
#ifdef JINY_UDPSTACK
	socket::net_stack_list[0] = init_udpstack();
#endif
#ifdef UIP_NETWORKING_MODULE
	socket::net_stack_list[0] = init_netmod_uipstack();
#endif

	return JSUCCESS;
}

#define MAX_QUEUE_LEN 500
static unsigned int from_q=0;
static unsigned int to_q=0;
static unsigned long send_qlen=0;
struct {
	unsigned char *buf;
	int len;
	int write_flags;
}queue[MAX_QUEUE_LEN+1];

static int put_to_send_queue(unsigned char *buf, int len, int write_flags){
	unsigned long flags;
	int ret = JFAIL;

	spin_lock_irqsave(&netbh_lock, flags);
	if (send_qlen < MAX_QUEUE_LEN) {
		queue[from_q].buf = buf;
		queue[from_q].len = len;
		queue[from_q].write_flags = write_flags;
		from_q++;
		send_qlen++;
		if (from_q >= MAX_QUEUE_LEN) {
			from_q = 0;
		}
		ret = JSUCCESS;
	}
	spin_unlock_irqrestore(&netbh_lock, flags);
	return ret;
}
static int remove_from_send_queue() {
	unsigned long flags;
	int ret;
	if (send_qlen == 0) {
		return 0;
	}
	ret = socket::net_dev->write(0, queue[to_q].buf, queue[to_q].len,
			queue[to_q].write_flags);
	if (ret == JSUCCESS) {
		spin_lock_irqsave(&netbh_lock, flags);
		to_q++;
		send_qlen--;
		if (to_q >= MAX_QUEUE_LEN) {
			to_q = 0;
		}
		spin_unlock_irqrestore(&netbh_lock, flags);
	}
	return ret;
}
static int process_send_queue() {
	unsigned long flags;
	int ret = JFAIL;
	int pkt_send = 0;
	static int in_progress = 0;

#if 0
	if (g_conf_send_alltime == 0) {
		if (send_qlen == 0) {
			return JFAIL;
		}
	}
#else
	if (send_qlen == 0) {
		return JFAIL;
	}
#endif
	spin_lock_irqsave(&netbh_lock, flags);
	if (in_progress == 0) {
		in_progress = 1;
		ret = JSUCCESS;
	}
	spin_unlock_irqrestore(&netbh_lock, flags);
	if (ret == JFAIL) {
		return ret;
	}

	while (remove_from_send_queue() == JSUCCESS) {
		pkt_send++;
	}
	if (pkt_send > 0) {
		/* send the kick */
	}
	in_progress = 0;
	if (socket::net_dev != 0){
		socket::net_dev->ioctl(NETDEV_IOCTL_FLUSH_SENDBUF, 0);
	}
	return ret;
}
int net_send_eth_frame(unsigned char *buf, int len, int write_flags) {
	int ret = JFAIL;

	if (socket::net_dev != 0) {
		if ((write_flags & WRITE_SLEEP_TILL_SEND)
				&& (write_flags & WRITE_BUF_CREATED)) {
			ret = put_to_send_queue(buf, len, write_flags);
			process_send_queue();
			goto last;
		} else {
			ret = socket::net_dev->write(0, buf, len, write_flags);
			socket::net_dev->ioctl(NETDEV_IOCTL_FLUSH_SENDBUF, 0);
		}
	}
	last: if (ret < 0) {
		ret = JFAIL;
	}
	return ret;
}
extern struct Socket_API *socket_api;
void net_get_mac(unsigned char *mac) {
	if (net_sched.device) {
		net_sched.device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) mac);
	}
}

void Jcmd_network(unsigned char *arg1, unsigned char *arg2) {
	if (net_sched.device) {
		unsigned char mac[10];

		net_sched.device->print_stats("all",0);
		net_sched.device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &mac);
		ut_printf(" Mac: %x:%x:%x:%x:%x:%x  sendqlen :%d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],send_qlen);
	}
	socket::print_all_stats();

	return;
}
}
