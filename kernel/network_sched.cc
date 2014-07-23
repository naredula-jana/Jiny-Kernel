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

unsigned char g_mac[7];
}
#include "jdevice.h"
#include "file.hh"
#include "network.hh"

#define MAX_POLL_DEVICES 5
struct device_under_poll_struct {
	void *private_data;
	int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts);
	int active;
};

class network_scheduler {
	int network_enabled;
	wait_queue_t waitq;
	struct device_under_poll_struct device_under_poll[MAX_POLL_DEVICES];
	int poll_underway;
	void *g_netBH_lock; /* All BH code will serialised by this lock */
	int stat_netrx_bh_recvs;
	int poll_devices();

public:
	jdevice *device;
	int init();
	int netRx_BH(void *arg, void *arg2);
	int netif_rx(unsigned char *data, unsigned int len);
	int netif_rx_enable_polling(void *private_data, int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts));
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


extern int g_conf_net_sendbuf_delay;
/*   Release the buffer after calling the callback */
int network_scheduler::netRx_BH(void *arg, void *arg2) {
	int qret;

	while (1) {
		if (poll_devices() > 0) {
			continue;
		}
#if 1
		if (g_conf_net_sendbuf_delay != 0) { /* if sendbuf_delay is enabled, smp is having negative impact and rate of recving falls down */
			if (device != 0) {
				device->ioctl(NETDEV_IOCTL_FLUSH_SENDBUF, 0);
			}
			ipc_waiton_waitqueue(&waitq, 50);
		} else {
			ipc_waiton_waitqueue(&waitq, 10000);
		}
#endif
	}
	return 1;
}
int network_scheduler::poll_devices() {
	int i, j;
	int ret = 0;
	int pending_devices = 0;

#if 0
	mutexLock(g_netBH_lock);
	if (poll_underway == 0) {
		poll_underway = 1;
		ret = 1;
	}
	mutexUnLock(g_netBH_lock);
	if (ret == 0)
		return ret;
#endif

	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data != 0 && device_under_poll[i].active == 1) {
			int max_pkts = 50;
			int total_pkts = 0;
			int pkts = 0;
			for (j = 0; j < 20; j++) {
				pkts = device_under_poll[i].poll_func(device_under_poll[i].private_data, 0, max_pkts);
				if (pkts == 0)
					break;
				total_pkts = total_pkts + pkts;
			}
			if (total_pkts == 0) {
				device_under_poll[i].active = 0;
				device_under_poll[i].poll_func(device_under_poll[i].private_data, 1, max_pkts); /* enable interrupts */
				pkts = device_under_poll[i].poll_func(device_under_poll[i].private_data, 0, max_pkts);
				if (pkts != 0){
					device_under_poll[i].active = 1;
					pending_devices++;
				}
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
			ipc_wakeup_waitqueue(&waitq);
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

	ipc_register_waitqueue(&waitq, "netRx_BH", WAIT_QUEUE_WAKEUP_ONE);
	//ipc_register_waitqueue(&waitq, "netRx_BH",0);
	g_netBH_lock = mutexCreate("mutex_netBH");
	poll_underway = 0;
	network_enabled = 1;
	return JSUCCESS;
}
int register_netdevice(jdevice *device) {
	net_sched.device = device;
	socket::net_dev = device;
	device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &g_mac);

	ut_log(" register netdev : %x \n", g_mac[5]);
	return JSUCCESS;
}
extern "C" {
extern int init_udpstack();
extern int init_netmod_uipstack();
int netif_thread(void *arg1, void *arg2) {
	return net_sched.netRx_BH(arg1, arg2);
}

int init_networking() {
	int pid;
	net_sched.init();
	pid = sc_createKernelThread(netif_thread, 0, (unsigned char *) "netRx_BH_1", 0);
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

int netif_rx(unsigned char *data, unsigned int len) {
	return net_sched.netif_rx(data, len);
}
int netif_rx_enable_polling(void *private_data, int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts)) {
	return net_sched.netif_rx_enable_polling(private_data, poll_func);
}
int net_send_eth_frame(unsigned char *buf, int len, int write_flags) {
	if (socket::net_dev != 0) {
		return socket::net_dev->write(0, buf, len, write_flags);
	} else {
		return JFAIL;
	}
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

		net_sched.device->print_stats();
		net_sched.device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &mac);
		ut_printf(" mac: %x:%x:%x:%x:%x:%x  \n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
	socket::print_all_stats();

	return;
}
}
