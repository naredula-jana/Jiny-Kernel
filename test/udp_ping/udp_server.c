/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   udp_ping/udp_server.c
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
#include <signal.h>
//#define DEBUG 1
int sfd;
int recv_stop = 0;
int send_on = 1;
struct sockaddr_in server, client;
unsigned long send_pkts, recv_pkts, pkt_size, total_pkts;
#define MAX_LOG_PKTS 10000
struct {
	unsigned long sec,usec;
}pkt_log[MAX_LOG_PKTS];

char rbuf[84240] = "";
void recv_func() {
	int l, ret;
	int rc;

	fd_set readSet;
	FD_ZERO(&readSet);

	struct timeval timeout,td;

	while (recv_stop == 0) {
		FD_SET(sfd, &readSet);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		rc = 2;
	//	rc = select(sfd + 1, &readSet, NULL, NULL, &timeout);
		if (rc != 0) {
			ret = recvfrom(sfd, rbuf, 50224, 0, (struct sockaddr *) &client, &l);
			if (recv_pkts <MAX_LOG_PKTS){
				gettimeofday(&td, (struct timezone *) 0);
				pkt_log[recv_pkts].sec = td.tv_sec;
				pkt_log[recv_pkts].usec = td.tv_usec;
			}
			if (ret > 0 && (send_on == 1)) {
				ret = sendto(sfd, rbuf, ret, 0, (struct sockaddr *) &client,
						sizeof(client));
				if (ret > 0)
					send_pkts++;
			}
			recv_pkts++;
#ifdef DEBUG
			printf("MESSAGE FROM CLIENT:%s\n", rbuf);
#endif
		}
	}
}

unsigned long mtime() {
	struct timeval td;
	gettimeofday(&td, (struct timezone *) 0);
	return (td.tv_sec * 1000000 + ((double) td.tv_usec));
}

int delay(int us) {
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = us;
	(void) select(1, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv);
	return (1);
}

unsigned long wait_till(unsigned long ts) {
	unsigned long t = mtime();
	if (t > ts)
		return t;
	delay((ts - t));
	return ts;
}
int duration = 30;

#define MAX_DIFFS 1000
unsigned long usec_diff[MAX_DIFFS];
void sigTerm(int s) {
	int i;

	printf("send_on:%d pktsize:%d send:%d recved:%d recvMbps: %d \n", send_on,
			pkt_size, send_pkts, recv_pkts, (recv_pkts * 8) / (30 * 1000));

	for (i=0; i<MAX_DIFFS; i++){
		usec_diff[i]=0;
	}
	for (i=10; i<MAX_LOG_PKTS ; i++){
		if (pkt_log[i].sec==pkt_log[i-1].sec){
			int diff=pkt_log[i].usec-pkt_log[i-1].usec;
			if (diff < MAX_DIFFS && diff > 0){
				usec_diff[diff]++;
			}
		}
		printf(" %d : sec: %d usec: %d \n",i,pkt_log[i].sec,pkt_log[i].usec-pkt_log[i-1].usec);
	}
	int total=0;
	for (i=0; i<MAX_DIFFS; i++){
		if (usec_diff[i]==0) continue;
		total=total+usec_diff[i];
		if (usec_diff[i] > 10){
			printf(" %d  -> %d  \n",i,usec_diff[i]);
		}
	}
	printf(" Total :%d\n",total);
	send_pkts = 0;
	recv_pkts = 0;

}
main(int argc, char *argv[]) {
	pthread_t recv_thread; /* thread variables */

	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	client.sin_family = AF_INET;
	server.sin_port = htons(1300);
	if (argc != 1) {
		printf("  argc :%d \n",argc);
		printf("./udp_server \n");
		return 1;
	}
	signal(SIGTERM, sigTerm);
	signal(SIGINT, sigTerm);
	inet_aton("0.0.0.0", &server.sin_addr);
	bind(sfd, &server, sizeof(server));
	send_pkts = recv_pkts = 0;

	pkt_size = 0;
	total_pkts = 0;

	printf(" Starting the Server:  \n");
	recv_func();
	recv_stop = 1;

	printf(
			"NEW pktsize:%d send:%d recved:%d  loss:%d SBit rate:%d Mbps Rbir rate:%d\n",
			pkt_size, send_pkts, recv_pkts, (send_pkts - recv_pkts),
			(send_pkts * pkt_size * 8) / (duration * 1000000),
			(recv_pkts * pkt_size * 8) / (duration * 1000000));
} 
