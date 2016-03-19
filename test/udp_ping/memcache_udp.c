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
#define MAX_THREADS 20
int sfd;
int recv_stop = 0;
int send_stop[MAX_THREADS];
int duration = 30;
int error_in_time=0;
int total_thds=0;
struct sockaddr_in server, client;
unsigned long g_send_pkts[MAX_THREADS], recv_pkts, pkt_size, total_pkts,total_records,print_pkts;
int recv_touch=0;
void recv_func(int total_pkts) {
	char buf[1424] = "";
	int l;
	int rc;
	int iters = total_pkts + 1;

	fd_set readSet;
	FD_ZERO(&readSet);

	struct timeval timeout;

	while (recv_stop == 0 && iters > 0) {
		recv_touch = 1;
		FD_SET(sfd, &readSet);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		rc = select(sfd + 1, &readSet, NULL, NULL, &timeout);
		if (rc != 0) {
			int ret;
			ret = recvfrom(sfd, buf, 1024, 0, (struct sockaddr *) &client, &l);
			recv_pkts++;
//buf[15]=0;
			if ( (recv_pkts % print_pkts) ==0)
			printf("%d : MESSAGE FROM memcached:%s ret:%d\n", recv_pkts, &buf[8],ret );

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
	sbps = ((g_send_pkts[0]+g_send_pkts[1]) * pkt_size * 8) / (duration * 1000000);
	rbps = (recv_pkts * pkt_size * 8) / (duration * 1000000);
	printf(
				" RECV thread udp_client pktsize:%d send:%d:%d recved:%d  loss:%d SBit rate:%d Mbps Rbir rate:%d err_in_time:%d\n",
				pkt_size, g_send_pkts[0],g_send_pkts[1], recv_pkts, (g_send_pkts[0]+g_send_pkts[1] - recv_pkts), sbps,
				rbps,error_in_time);
	recv_stop = 99;
}
join() {
	int i;

	while (recv_stop != 99) {
		recv_touch =0;
		delay(600000); /* delay 600 ms */
		delay(800000); /* delay 800 ms */
		delay(900000); /* delay 800 ms */
		if (recv_touch == 0){
			goto last;
		}
	}

#if 0
	while (1) {
		int found = 0;
		for (i = 0; i < total_thds; i++) {
			if (send_stop[i] == 99) {
				found++;
			}
		}
		if (found == total_thds) {
			goto last;
		}
	}
#endif
	last: delay(700000);
	printf(" Joing is existing \n");

}

unsigned long old_ts=0;
unsigned long mtime() {
	struct timeval td;
	unsigned long ret;
	gettimeofday(&td, (struct timezone *) 0);
	ret = (td.tv_sec * 1000000 + ((double) td.tv_usec));
#if 0
	if (ret < old_ts){
		error_in_time++;
		//printf("ERROR: oldts : %x(%d)  new ts: %x(%d) \n",old_ts,old_ts,ret,ret);
		//recv_stop = 1;
	}
	old_ts = ret;
#endif

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
struct memcache_pkt{
	unsigned  short req_id;
	unsigned short seq;
	unsigned char tot1,tot2;
	unsigned short reserv;
};
int serial_no=10;
send_func(int th_id) {
	int i;
	char buf[1424] = "";
	struct timeval td;
	unsigned long start_time, end_time, time_per_pkt, target_t, curr_t;/* time in milliseconds */
	struct memcache_pkt *pkt;

	printf("one time %d: send thread START time: %d ms  memcache hdr size:%d\n", th_id, mtime() / 1000, sizeof(struct memcache_pkt));

	start_time = mtime();
	end_time = start_time + duration * 1000000; /* 30 seconds */
//return 1;
	time_per_pkt = (end_time - start_time) / total_pkts;
	target_t = start_time;
	curr_t = start_time;
	pkt=&buf[0];

	while (curr_t < end_time && g_send_pkts[th_id] < total_pkts) {
		int len;

		sprintf(&buf[8], "set TST%d 0 200 5\r\n12345\r\n", serial_no);
		serial_no++;
		if (serial_no > total_records){
			serial_no = 0;
		}
		pkt->tot1 =0;
		pkt->tot2 = 1;
		pkt->seq = serial_no;
		len=strlen(&buf[8]);
		sendto(sfd, buf, len+8, 0, (struct sockaddr *) &server,
				sizeof(server));
		//serial_no++;
		pkt->seq = serial_no;
		sprintf(&buf[8], "12347\r\n");
		len=strlen(&buf[8]);
		//sendto(sfd, &buf[8], len, 0, (struct sockaddr *) &server,sizeof(server));

//#ifdef DEBUG
	//	printf("packet send : currtime:%d len:%d\n",curr_t,len);
//#endif
		g_send_pkts[th_id]++;

		target_t = target_t + time_per_pkt;
		curr_t = wait_till(target_t);
		//while(1);
	}
	printf(" %d: SEND End time: %d ms sendpkts: %d \n",th_id, mtime() / 1000, g_send_pkts[th_id]);
}

void send_thread_func(unsigned long *arg){
	unsigned long i = *arg;
	printf("send arg :%d: \n",i);
	if (i<0 || i>20){
		return;
	}
	send_func(i);
	recv_stop = 1;
	send_stop[i] = 99;
}

unsigned char stack[9000];
unsigned char send_stack[10][9000];
main(int argc, char *argv[]) {
	pthread_t recv_thread; /* thread variables */
	unsigned long sbps, rbps;
	int port;

	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(8100);
	port = 8100;

	if (argc < 4) {
		printf("./memc <server_ip>  <total_pkts> <total_records>  <print_pkt>\n");
		return 1;
	}
	inet_aton(argv[1], &server.sin_addr);
	g_send_pkts[0] = g_send_pkts[1] = recv_pkts = 0;

	pkt_size = 100;
	total_pkts = atoi(argv[2]);
	total_thds = 1;
	total_records = atoi(argv[3]);
	print_pkts = atoi(argv[4]);
	int newMaxBuff = 0;
	int len = sizeof(newMaxBuff);

	printf("Memc sock buf size :%d  port :%d\n", newMaxBuff,port);
	newMaxBuff = 1024000;
	setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &newMaxBuff, sizeof(newMaxBuff));
//getsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &newMaxBuff, &len);
	printf("New sock buf size :%d \n", newMaxBuff);
	if (pkt_size < 28) {
		printf(" ERROR: packet size should be atleast 28 \n");
		return 0;
	}

	clone((void *) &recv_thread_func, &stack[4000], 9000, 0, 0);

	unsigned long args[50];
	if (total_thds >= 1){
		int i;
		for (i=0; i<total_thds; i++){
			send_stop[i]=0;
			args[2*i+0]= i;
			args[2*i + 1]= 0;
			printf(" creatin gthread ... \n");
			clone((void *) &send_thread_func, &send_stack[i][4000], 9000, &args[2*i], 0);
		}
	}
	//send_func(0);


	join();
	//pthread_join(recv_thread, NULL);

	sbps = ((g_send_pkts[0]+g_send_pkts[1]) * pkt_size * 8) / (duration * 1000000);
	rbps = (recv_pkts * pkt_size * 8) / (duration * 1000000);
	printf("udp_client pktsize:%d send:%d:%d recved:%d  loss:%d SBit rate:%d Mbps Rbir rate:%d duration:%d\n",
			pkt_size, g_send_pkts[0],g_send_pkts[1], recv_pkts, (g_send_pkts[0]+g_send_pkts[1] - recv_pkts), sbps,
			rbps,duration);
}
