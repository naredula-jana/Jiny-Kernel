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

int g_conf_net_pmd __attribute__ ((section ("confdata")))=0; /* pollmode driver on/off */
int g_conf_net_auto_intr __attribute__ ((section ("confdata")))=0; /* auto interrupts, switch on/off based on the recv packet frequency */
int g_conf_netbh_cpu __attribute__ ((section ("confdata")))=0;

int g_conf_net_send_int_disable __attribute__ ((section ("confdata"))) = 1;
//int g_conf_net_send_int_disable=1;
int g_conf_net_send_dur __attribute__ ((section ("confdata")))=0;
int g_conf_net_send_burst  __attribute__ ((section ("confdata"))) =128;
int g_conf_net_recv_burst __attribute__ ((section ("confdata")))=128;
int g_conf_netbh_dedicated __attribute__ ((section ("confdata")))=1; /* all netbh by only dedicated thread  for send and recv */
int g_conf_net_send_shaping __attribute__ ((section ("confdata")))=0 ; /* on error or speedy sending , slowy down sending if shaping is enabled */

int g_net_interrupts_disable=1;
unsigned long g_stat_net_send_errors;
unsigned long g_stat_net_intr_mode_toggle;
}
#include "jdevice.h"
#include "file.hh"
#include "network.hh"

network_scheduler net_sched;
//static int stat_from_driver = 0;
//static int stat_to_driver = 0;
int g_net_bh_active __attribute__ ((aligned (64))) = 0;

//static int stats_pktrecv[1024];
extern "C"{
#if 0
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
#endif
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
	net_sched.device_list[net_sched.device_count] = (jnetdriver *)device->driver;
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

extern "C" {
static int net_bh_send();
extern int init_udpstack();
extern int init_netmod_uipstack();
static spinlock_t netbh_lock;
static spinlock_t netbhsend_lock;

static int net_bh_recv() {
	unsigned long flags;
	int pkt_consumed = 0;
	//int ret_bh = 0;
	int i;

	for (i = 0; i < net_sched.device_count; i++) {
		if (net_sched.device_list[i]->check_for_pkts() == 0){
			continue;
		} else {
			int pkts = 0;
			int ret=0;
			while (ret < g_conf_net_recv_burst) {
				pkts = net_sched.device_list[i]->burst_recv(g_conf_net_recv_burst);
				if (pkts == 0) {
					break;
				}
				ret = ret + pkts;
			}
			pkt_consumed=pkt_consumed+ret;
		}
	}
#if 0
	if (i == net_sched.device_count){
		return 0;
	}

	if (g_net_interrupts_disable == 1) { /* only in the poll mode */
		g_net_bh_active = 1;
	}
	if ((g_net_bh_active == 0)) {
		goto last;
	}

	ret_bh = net_sched.netRx_BH();
	g_cpu_state[getcpuid()].stats.netbh_recv = g_cpu_state[getcpuid()].stats.netbh_recv + ret_bh;
	g_cpu_state[getcpuid()].stats.netbh++;

	if (g_net_interrupts_disable == 0 && net_sched.device) {
		g_net_bh_active = 0;
		net_sched.device->ioctl(NETDEV_IOCTL_ENABLE_RECV_INTERRUPTS, 0);
		pkt_consumed = net_sched.netRx_BH();
	}
#endif
last:
	return pkt_consumed;
}
extern int g_conf_test_dummy_send;
/* TODO -1 : currently it is assumed for 1 nic, later this need to be extended for multiple nics*/
/* TODO -2 : all sending and recv is done by only one cpu any time, this minimises the locks */
int net_bh(){

	int cpu = getcpuid();

	//if (g_conf_net_pmd==1  && g_conf_netbh_cpu != cpu){
	if (g_conf_netbh_cpu != cpu){
		return 0;
	}
	if (g_cpu_state[cpu].net_bh.inprogress == 1){
		return 0;
	}
	g_cpu_state[cpu].net_bh.inprogress =1;  /* this is to protect to enter this function by the same thread during soft interrupts */
	//socket::tcp_housekeep();
	int ret = 0;
	do {
		ret = net_bh_send();
		ret = ret + net_bh_recv();
		g_cpu_state[cpu].net_bh.pkts_processed = g_cpu_state[cpu].net_bh.pkts_processed + ret;
	} while (ret > 0);

	g_cpu_state[cpu].net_bh.inprogress = 0;
	return ret;
}
/*****************************************************************/
static int sendqs_empty __attribute__ ((aligned (64))) =1;
static class fifo_queue *send_queues[MAX_CPUS];
/* from socket layer -->  NIC */
static int sendq_add(unsigned char *buf, int len, int write_flags){
	int ret = send_queues[getcpuid()]->add_to_queue(buf,len,write_flags,0);

	if (sendqs_empty == 1 && ret==JSUCCESS){
		sendqs_empty = 0;
	}
	return ret;
}

int check_emptyspace_net_sendq(){
	return send_queues[getcpuid()]->check_emptyspace();
}
/* from NIC -->  socket layer */
int netif_rx(unsigned char *data, unsigned int len) {
	if (data == 0){
		return JFAIL;
	}

	if (socket::attach_rawpkt(data, len) == JFAIL) {
		jfree_page(data);
	}
	return JSUCCESS;
}
static int sendq_remove(){
	static int last_q __attribute__ ((aligned (64)))=0;
	int i,cpu;
	int ret=JFAIL;
	int outer;
	int pkts=0;
	int wr_flags;
	int maxcpu=getmaxcpus();

	jnetdriver *driver = (jnetdriver *)socket::net_dev->driver;

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
#if 1
			if (send_queues[cpu]->is_empty() == JSUCCESS) {
				continue;
			}
#endif
			pkts = pkts + send_queues[cpu]->Bulk_remove_from_queue(&driver->send_mbuf_list[pkts],MAX_BUF_LIST_SIZE-pkts);
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

static int put_to_send_queue(unsigned char *buf, int len, int write_flags){
	return sendq_add(buf,len,write_flags);
}

static int bulk_remove_from_send_queue() {
	int i,ret;

	jnetdriver *driver = (jnetdriver *)socket::net_dev->driver;
	if (sendqs_empty == 1 && driver->send_mbuf_len == 0) {
		return 0;
	}
	if (driver->send_mbuf_len != 0){
		ret = driver->burst_send();
		return ret;
	}
	ret = sendq_remove();
	if (ret == JSUCCESS) {
		ret = driver->burst_send();
	}
	return ret;
}

static int net_bh_send() {
	unsigned long flags;
	int pkt_send = 0;

	if (sendqs_empty == 1 || g_conf_net_send_burst==0) {
		return pkt_send;
	}

	int pkts=0;
	do {
		pkts = bulk_remove_from_send_queue();
		pkt_send = pkt_send + pkts;
	}while(pkts > 0  && (pkt_send < g_conf_net_send_burst));

	g_cpu_state[getcpuid()].stats.netbh_send = g_cpu_state[getcpuid()].stats.netbh_send + pkt_send;
	if (socket::net_dev != 0 ){
		socket::net_dev->ioctl(NETDEV_IOCTL_FLUSH_SENDBUF, 0);
	}

	return pkt_send;
}
int net_send_eth_frame(unsigned char *buf, int len, int write_flags) {
	int ret = JFAIL;

	if (socket::net_dev != 0) {
		ret = put_to_send_queue(buf, len, write_flags);
	}
	if (ret == JFAIL){
		STAT_INC(g_stat_net_send_errors);
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

		net_sched.device->ioctl(NETDEV_IOCTL_PRINT_STAT, arg1);
		net_sched.device->ioctl(NETDEV_IOCTL_GETMAC, (unsigned long) &mac);
		for (i=0; i<getmaxcpus(); i++){
			ut_printf(" %d: sendq_attached:%d sendq_DROP:%d LEN :%d err_check_full:%d\n",i,send_queues[i]->stat_attached,send_queues[i]->stat_drop,send_queues[i]->queue_size(),send_queues[i]->error_empty_check);
			if (arg1 && ut_strcmp(arg1,"clear")==0){
				send_queues[i]->stat_attached = 0;
				send_queues[i]->stat_drop = 0;
				send_queues[i]->error_empty_check =0;
			}
		}
		ut_printf(" new Mac-address : %x:%x:%x:%x:%x:%x current interrupt disable:%i qeuesstatus:%d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],g_net_interrupts_disable,sendqs_empty);
	}
	socket::print_all_stats();

	return;
}
}
