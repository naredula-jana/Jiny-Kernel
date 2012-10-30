/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/network.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 */

/*  Description:
 *   - Middle layer between network drivers(like virtio..)  and protocol stacks(lwip, udp responser .. etc)
 *   - It works as a bottom Half for network packets.
 *   - It should come up before drivers and protocol stacks, so that they hook to this layer
 */
#define DEBUG_ENABLE 1
#include "common.h"


#define MAX_QUEUE_LENGTH 250
struct queue_struct{
	queue_t waitq;
	int producer,consumer;
	struct {
		unsigned char *buf;
		unsigned int len;
	} data[MAX_QUEUE_LENGTH];
    int stat_processed[MAX_CPUS];
};
static struct queue_struct queue;

#define MAX_HANDLERS 10
struct net_handlers{
	void *private;
	int (*callback)(unsigned char *buf, unsigned int len, void *private_data);
};
static int count_net_drivers=0;
static int count_protocol_drivers=0;
static struct net_handlers net_drivers[MAX_HANDLERS];
static struct net_handlers protocol_drivers[MAX_HANDLERS];
static int add_to_queue(unsigned char *buf, int len) {
	if (buf==0 || len==0) return 0;
	if (queue.data[queue.producer].buf == 0) {
		queue.data[queue.producer].len = len;
		queue.data[queue.producer].buf = buf;
		queue.producer++;
		if (queue.producer >= MAX_QUEUE_LENGTH)
			queue.producer = 0;
		return 1;
	}
	return 0;
}
static int remove_from_queue(unsigned char **buf, unsigned int *len){
	if ((queue.data[queue.consumer].buf != 0) && (queue.data[queue.consumer].len != 0)) {
		*buf = queue.data[queue.consumer].buf;
		*len = queue.data[queue.consumer].len;
		queue.data[queue.consumer].len = 0;
		queue.data[queue.consumer].buf = 0;
		queue.consumer++;
		queue.stat_processed[getcpuid()]++;
		if (queue.consumer >= MAX_QUEUE_LENGTH)
			queue.consumer = 0;
		return 1;
	}
	return 0;
}
static int netRx_BH(void *arg) {
	unsigned char *data;
	unsigned int len;

	while (1) {
		sc_wait(&queue.waitq, 10);
		while (remove_from_queue(&data, &len) == 1) {
			int ret=0;
			if (count_protocol_drivers > 0) {
				ret=protocol_drivers[0].callback(data, len, protocol_drivers[0].private);
			}
			if (ret==0){
				mm_putFreePages(data, 0);
			}
		}
	}
	return 1;
}

int registerNetworkHandler(int type,
		int (*callback)(unsigned char *buf, unsigned int len, void *private_data),
		void *private_data)
{
	if (type == NETWORK_PROTOCOLSTACK){
		protocol_drivers[count_protocol_drivers].callback = callback;
		protocol_drivers[count_protocol_drivers].private = private_data;
		count_protocol_drivers++;
	}else{
		net_drivers[count_net_drivers].callback = callback;
		net_drivers[count_net_drivers].private = private_data;
		count_net_drivers++;
	}
    return 1;
}
int netif_rx(unsigned char *data, unsigned int len) {
	if (add_to_queue(data,len)==0 && data!=0 && len!=0){
		mm_putFreePages(data, 0);
	}
	return 1;
}
int netif_tx(unsigned char *data, unsigned int len) {
	int ret;
	if (data==0 || len==0) return 0;
	if (count_net_drivers > 0) {
		ret=net_drivers[0].callback(data, len, net_drivers[0].private);
		if (ret ==0 ){
			mm_putFreePages(data, 0);
		}
		return 1;
	}else{

	}
	return 0;
}
int init_networking(){
	int ret;

    ut_memset((unsigned char *)&queue,0,sizeof(struct queue_struct));
	sc_register_waitqueue(&queue.waitq ,"netRx_BH");
	ret = sc_createKernelThread(netRx_BH, 0, (unsigned char *)"netRx_BH");
	return 1;
}
