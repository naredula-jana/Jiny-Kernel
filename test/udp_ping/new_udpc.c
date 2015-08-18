/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   udp_ping/udp_client.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/


#include<netinet/in.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include<stdio.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include<fcntl.h> 
#include <pthread.h>
//#define DEBUG 1
int sfd;
int recv_stop = 0;
int duration = 30;
int error_in_time=0;
struct sockaddr_in server, client;
unsigned long send_pkts, recv_pkts, pkt_size, total_pkts;

void recv_func(int total_pkts) {
	char buf[1424] = "";
	int l;
	int rc;
	int iters = total_pkts + 1;

	fd_set readSet;
	FD_ZERO(&readSet);

	struct timeval timeout;

	while (recv_stop == 0 && iters > 0) {
		FD_SET(sfd, &readSet);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;


//		rc = select(sfd + 1, 0 , NULL, NULL, &timeout);
//		continue;

		rc = select(sfd + 1, &readSet, NULL, NULL, &timeout);
		if (rc != 0) {
			recvfrom(sfd, buf, 1024, 0, (struct sockaddr *) &client, &l);
			recv_pkts++;
#ifdef DEBUG
			printf("MESSAGE FROM CLIENT:%s\n", buf);
#endif
		} else {
			return 0;
		}
		iters--;
	}
}

void recv_thread_func() {
	unsigned long sbps, rbps;

	printf(" Recv thread started \n");
	while (recv_stop == 0) {
		recv_func(0);
	}
	sbps = (send_pkts * pkt_size * 8) / (duration * 1000000);
	rbps = (recv_pkts * pkt_size * 8) / (duration * 1000000);
	printf(
				" RECV thread udp_client pktsize:%d send:%d recved:%d  loss:%d SBit rate:%d Mbps Rbir rate:%d err_in_time:%d\n",
				pkt_size, send_pkts, recv_pkts, (send_pkts - recv_pkts), sbps,
				rbps,error_in_time);
	recv_stop = 99;
}
join() {
	while (recv_stop != 99) {
		delay(500000); /* delay 500 ms */
	}
}

unsigned long old_ts=0;
unsigned long mtime() {
	struct timeval td;
	unsigned long ret;
	gettimeofday(&td, (struct timezone *) 0);
	ret = (td.tv_sec * 1000000 + ((double) td.tv_usec));
#if 1
	if (ret < old_ts){
		error_in_time++;
		//printf("ERROR: oldts : %x(%d)  new ts: %x(%d) \n",old_ts,old_ts,ret,ret);
		//recv_stop = 1;
	}
#endif
	old_ts = ret;
	return ret;
}

int delay(long us) {
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = us;
	(void) select(1, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv);
	return (1);
}

unsigned long wait_till(unsigned long ts) {
	unsigned long t = mtime();
	if (t >= ts)
		return t;
	delay((ts - t));
	return ts;
}

send_func() {
	int i;
	char buf[1424] = "";
	struct timeval td;
	unsigned long start_time, end_time, time_per_pkt, target_t, curr_t;/* time in milliseconds */

	printf(" START time: %d ms\n", mtime() / 1000);
	start_time = mtime();
	end_time = start_time + duration * 1000000; /* 30 seconds */

	time_per_pkt = (end_time - start_time) / total_pkts;
	target_t = start_time;
	curr_t = start_time;

	while (curr_t < end_time && send_pkts < total_pkts) {
		sprintf(buf, "[pkt Current time :%d ]", curr_t);
		sendto(sfd, buf, pkt_size - 28, 0, (struct sockaddr *) &server,
				sizeof(server));
#ifdef DEBUG
		printf("packet send : currtime:%d\n",curr_t);
#endif
		send_pkts++;

		target_t = target_t + time_per_pkt;
		curr_t = wait_till(target_t);
	}
	printf("Only sending End time: %d ms sendpkts: %d \n", mtime() / 1000, send_pkts);
}
unsigned char stack[9000];
main(int argc, char *argv[]) {
	pthread_t recv_thread; /* thread variables */
	unsigned long sbps, rbps;
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(1300);
	if (argc != 4) {
		printf("./udp_client <server_ip> <pkt_size> <total_pkts>\n");
		return 1;
	}
	inet_aton(argv[1], &server.sin_addr);
	send_pkts = recv_pkts = 0;

	pkt_size = atoi(argv[2]);
	total_pkts = atoi(argv[3]);
	int newMaxBuff = 0;
	int len = sizeof(newMaxBuff);
//getsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &newMaxBuff, &len);
	printf("sock buf size :%d \n", newMaxBuff);
	newMaxBuff = 1024000;
	setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &newMaxBuff, sizeof(newMaxBuff));
//getsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &newMaxBuff, &len);
	printf("New sock buf size :%d \n", newMaxBuff);
	if (pkt_size < 28) {
		printf(" ERROR: packet size should be atleast 28 \n");
		return 0;
	}


	send_func();
	recv_stop = 1;

	//join();
	//pthread_join(recv_thread, NULL);

	sbps = (send_pkts * pkt_size * 8) / (duration * 1000000);
	rbps = (recv_pkts * pkt_size * 8) / (duration * 1000000);
	printf(
			"udp_client pktsize:%d send:%d recved:%d  loss:%d SBit rate:%d Mbps Rbir rate:%d duration:%d\n",
			pkt_size, send_pkts, recv_pkts, (send_pkts - recv_pkts), sbps,
			rbps,duration);
}
