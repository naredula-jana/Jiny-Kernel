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
extern int net_bh(int full_recv_send);
unsigned char g_mac[7];

int g_conf_net_pmd=1; /* pollmode driver on/off */
int g_conf_net_auto_intr=0; /* auto interrupts, switch on/off based on the recv packet frequency */
int g_conf_netbh_cpu=0;

int g_conf_net_virtio_burst=1;

int g_conf_net_send_int_disable = 1;
int g_conf_net_send_dur=0;
int g_conf_net_send_burst=128;
int g_conf_net_recv_burst=128;

int g_conf_netbh_dedicated=1; /* all netb by only deidcated thread  for send and recv*/
int g_conf_net_send_shaping=1; /* on error or speedy sending , slowy down sending if shaping is enabled*/

int g_net_interrupts_disable=1;

unsigned long g_stat_net_send_errors=0;

unsigned long g_stat_net_intr_mode_toggle=0;
}
#include "jdevice.h"
#include "file.hh"
#include "network.hh"

network_scheduler net_sched;
static int stat_from_driver = 0;
static int stat_to_driver = 0;
int g_net_bh_active = 0;

static int stats_pktrecv[1024];
extern "C"{
void Jcmd_netbhstat(void *arg1,void *arg2){
	int i;
	int total=0;
	int count=0;
	for (i=0; i<1000; i++){
		if (stats_pktrecv[i]==0) continue;
		ut_printf(" %d -> %d  ::",i,stats_pktrecv[i]);
		total=total+(stats_pktrecv[i]*i);
		count=count+stats_pktrecv[i];
	}
	ut_printf("\n total pkts: %d total calls:%d  AVG pkts per call:%d\n",total,count,total/count);
}

}


int network_scheduler::netif_rx(unsigned char *data, unsigned int len) {
	if (data == 0){
		return JFAIL;
	}
	if (socket::attach_rawpkt(data, len) == JFAIL) {
		jfree_page(data);
	}
	return JSUCCESS;
}

int network_scheduler::init() {
	unsigned long pid;
	int i;

	device_count = 0;
	waitq = jnew_obj(wait_queue,"netRx_BH", WAIT_QUEUE_WAKEUP_ONE);
	g_netBH_lock = mutexCreate("mutex_netBH");
	network_enabled = 1;
	return JSUCCESS;
}

int register_netdevice(jdevice *device) {
	net_sched.device_list[net_sched.device_count] = (virtio_net_jdriver *)device->driver;
	net_sched.device_count++;

	net_sched.device = device;
	socket::net_dev = device;
	device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &g_mac);
	if (g_net_interrupts_disable ==1){
		device->ioctl(NETDEV_IOCTL_DISABLE_RECV_INTERRUPTS, 0);
	}
	ut_log(" register netdev : %x \n", g_mac[5]);
	return JSUCCESS;
}
int network_scheduler::netRx_BH() {
	int i, j;
	int ret = 0;

	for (i = 0; i < device_count; i++) {
		int max_pkts = g_conf_net_recv_burst;
		int total_pkts = 0;
		int pkts = 0;
		while (ret < g_conf_net_recv_burst) {
			if (g_conf_net_virtio_burst == 1){
				pkts = device_list[i]->burst_recv(max_pkts);
			}else{
				pkts = device_list[i]->virtio_net_poll_device(max_pkts);
			}
			if (pkts == 0) {
				return ret;
			}
			if (pkts < 900 && pkts > 0) {
				stats_pktrecv[pkts]++;
			}
			ret = ret + pkts;
		}
	}
	return ret;
}
extern "C" {
static int net_bh_send();
extern int init_udpstack();
extern int init_netmod_uipstack();
static spinlock_t netbh_lock;
static spinlock_t netbhsend_lock;

static int net_bh_recv() {
	unsigned long flags;
	static int netbh_in_progress = 0;
	int pkt_consumed = 0;
	int ret_bh = 0;
	int i;

	for (i = 0; i < net_sched.device_count; i++) {
		if (net_sched.device_list[i]->check_for_pkts() == 0){
			continue;
		}
		break;
	}
	if (i == net_sched.device_count){
		return 0;
	}

	if (netbh_in_progress == 1) {
		goto last;
	}

	if (g_net_interrupts_disable == 1) { /* only in the poll mode */
		g_net_bh_active = 1;
	}

	if ((g_net_bh_active == 0) || (netbh_in_progress == 1)) {
		goto last;
	}

	/* only one cpu will be in net_bh */
	spin_lock_irqsave(&netbh_lock, flags);
	if (netbh_in_progress == 0) {
		netbh_in_progress = 1;
		spin_unlock_irqrestore(&netbh_lock, flags);
	} else {
		spin_unlock_irqrestore(&netbh_lock, flags);
		goto last;
	}

	ret_bh = net_sched.netRx_BH();
	g_cpu_state[getcpuid()].stats.netbh_recv =
			g_cpu_state[getcpuid()].stats.netbh_recv + ret_bh;
	g_cpu_state[getcpuid()].stats.netbh++;

	if (g_net_interrupts_disable == 0 && net_sched.device) {
		g_net_bh_active = 0;
		net_sched.device->ioctl(NETDEV_IOCTL_ENABLE_RECV_INTERRUPTS, 0);
		pkt_consumed = net_sched.netRx_BH();
	}

	netbh_in_progress = 0;

	last: return 1;
}

/* TODO : currently it is assumed for 1 nic, later this need to be extended for multiple nics*/
int net_bh(int send_bh){
	int ret = 0;
	static unsigned long last_active=0;
	unsigned long ts;
	static unsigned long last_jiffies=0;
	static unsigned long recvied_pkts=0;

	if (g_conf_net_pmd==1  && g_conf_netbh_cpu != getcpuid()){
		if (g_conf_netbh_dedicated == 1){ /* only one thread does the netbh */
			return 0;
		}
	}
	if (send_bh) {
		net_bh_send();
	}
	if (g_conf_netbh_cpu != getcpuid()) {
		return 1;
	}

	if (g_conf_net_pmd==1 && g_conf_net_auto_intr==1 && g_jiffies>last_jiffies){
		last_jiffies = g_jiffies;
		if (recvied_pkts > 100){
			g_net_interrupts_disable = 1;
		}else{  /* enable interrupts  */
			if (g_net_interrupts_disable == 1){
				g_stat_net_intr_mode_toggle++;
			}
			g_net_interrupts_disable = 0;
		}
		recvied_pkts =0;
	}

	ret = net_bh_recv();
	recvied_pkts = recvied_pkts + ret;
	return ret;
}
/*****************************************************************/
static int sendqs_empty=1;
static class fifo_queue *send_queues[MAX_CPUS];

static int sendq_add(unsigned char *buf, int len, int write_flags){
	int ret = send_queues[getcpuid()]->add_to_queue(buf,len,write_flags,0);
	if (sendqs_empty == 1 && ret==JSUCCESS){
		sendqs_empty = 0;
	}
	return ret;
}
static int sendq_remove(){
	static int last_q=0;
	int i,cpu;
	int ret=JFAIL;
	int outer;
	int pkts=0;
	int wr_flags;
	int maxcpu=getmaxcpus();
	virtio_net_jdriver *driver = (virtio_net_jdriver *)socket::net_dev->driver;

	if (sendqs_empty == 1){
		return ret;
	}
	for (outer = 1; outer < 3; outer++) {
		cpu = last_q;
		if (outer == 2 && pkts==0){
			sendqs_empty = 1;
		}
		for (i = 0; i < maxcpu; i++) {
			cpu++;
			if (cpu >= maxcpu) {
				cpu = 0;
			}
			if (send_queues[cpu]->queue_len.counter == 0) {
				continue;
			}

			ret = JSUCCESS;
			while (ret==JSUCCESS && pkts<MAX_BUF_LIST_SIZE ){
				ret = send_queues[cpu]->remove_from_queue(&driver->send_mbuf_list[pkts].buf,&driver->send_mbuf_list[pkts].len,&wr_flags);
				if (ret == JSUCCESS){
					pkts++;
				}
			}
			if (pkts >= MAX_BUF_LIST_SIZE){
				goto last;
			}
		}
	}
last:
	if (pkts>0){
		driver->send_mbuf_len = pkts;
		last_q = cpu;
		if (sendqs_empty != 0){
			sendqs_empty =0;
		}
		return JSUCCESS;
	}else{
		return JFAIL;
	}
	return ret;
}
/*******************************************************************/
int init_networking() {
	int pid;
	unsigned char name[100];
	int i;

	arch_spinlock_init(&netbh_lock, (unsigned char *)"NetBhRecv_lock");
	arch_spinlock_init(&netbhsend_lock, (unsigned char *)"NetBsSend_lock");
	net_sched.init();
	socket::init_socket_layer();
	for (i=0; i<MAX_CPUS && i<getmaxcpus(); i++){
		send_queues[i] = jnew_obj(fifo_queue);
		ut_sprintf(name,"Sendq-%d",i);
		send_queues[i]->init((unsigned char *)name,0);
	}
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


static unsigned long last_fail_send_ts_us = 0;
static int put_to_send_queue(unsigned char *buf, int len, int write_flags){
	return sendq_add(buf,len,write_flags);
}

static int bulk_remove_from_send_queue() {  /* under lock */
	int i,ret;
	unsigned char *buf;
	int len,wr_flags;

	virtio_net_jdriver *driver = (virtio_net_jdriver *)socket::net_dev->driver;
	if (sendqs_empty == 1 && driver->send_mbuf_len == 0) {
		return 0;
	}

#if 1
	if (last_fail_send_ts_us > 0  && (g_conf_net_send_shaping==1)){  /* give a gap of 10us if the virtio ring is full */
		if (((ut_get_systemtime_ns()/1000) - last_fail_send_ts_us) < 100){
			return 0;
		}
		last_fail_send_ts_us = 0;
	}
#endif
	if (driver->send_mbuf_len != 0){
		ret = driver->burst_send();
		return ret;
	}

	ret = sendq_remove();

	if (ret == JSUCCESS) {
		ret = driver->burst_send();
		if (ret == 0) {
			last_fail_send_ts_us = ut_get_systemtime_ns() / 1000;
		}
	}
	return ret;
}

static int net_bh_send() {
	int duration = g_conf_net_send_dur;
	int max_pkts_tosend =  g_conf_net_send_burst;
	unsigned long flags;
	int ret = JFAIL;
	int pkt_send = 0;
	static int in_progress = 0;
	static unsigned long last_active=0;
	unsigned long ts;

	if (sendqs_empty == 1  || (in_progress == 1) ) {
		return JFAIL;
	}

	spin_lock_irqsave(&netbhsend_lock, flags);
	if (in_progress == 0) {
		in_progress = 1;
		ret = JSUCCESS;
	}
	spin_unlock_irqrestore(&netbhsend_lock, flags);

	if (ret == JFAIL) {
		return ret;
	}

	if (duration > 0 && g_conf_net_send_shaping==1) {
		ts = ut_get_systemtime_ns() / 1000;
		if ((ts - last_active) < duration) {
			in_progress = 0;
			return JFAIL;
		}
		last_active = ts;
	}

	int pkts=0;
	do {
		pkts = bulk_remove_from_send_queue();
		pkt_send = pkt_send + pkts;
	}while(pkts > 0  && (pkt_send < max_pkts_tosend));

	g_cpu_state[getcpuid()].stats.netbh_send = g_cpu_state[getcpuid()].stats.netbh_send + pkt_send;
	if (socket::net_dev != 0 ){
		socket::net_dev->ioctl(NETDEV_IOCTL_FLUSH_SENDBUF, 0);
	}
last:
	in_progress = 0;
	return ret;
}
int net_send_eth_frame(unsigned char *buf, int len, int write_flags) {
	int ret = JFAIL;

	if (socket::net_dev != 0) {
		ret = put_to_send_queue(buf, len, write_flags);
	}

	if (ret == JFAIL){
		g_stat_net_send_errors++;
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
	int i;
	if (net_sched.device) {
		unsigned char mac[10];

		net_sched.device->ioctl(NETDEV_IOCTL_PRINT_STAT, 0);
		net_sched.device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &mac);
		for (i=0; i<getmaxcpus(); i++){
			ut_printf(" %d: sendq_attached:%d sendq_DROP:%d LEN :%d \n",i,send_queues[i]->stat_attached,send_queues[i]->stat_drop,send_queues[i]->queue_len.counter);
		}
		ut_printf(" Maac===++++addr : %x:%x:%x:%x:%x:%x current interrupt disable:%i qeuesstatus:%d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],g_net_interrupts_disable,sendqs_empty);
	}
	socket::print_all_stats();

	return;
}
}
