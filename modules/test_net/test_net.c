/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   test_file.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 */
void create_thread(void (*func)(void *arg));
void start_udp_client();
void start_udp_server();

#ifdef _KERNEL
#include "lwip/sockets.h"
typedef struct {
	unsigned long tv_sec;
	unsigned long tv_usec;
}ntime_t;
#define AF_INET         2
#define SOCK_DGRAM      2
#define socket SYS_socket
#define sendto SYS_sendto
#define gettimeofday SYS_gettimeofday
#define bind SYS_bind
#define recvfrom SYS_recvfrom
#define printf ut_printf
#define close SYS_fs_close
static void init_module(unsigned char *arg1, unsigned char *arg2){
	printf(" Starting the net test module ver=1.0\n");
	create_thread(start_udp_server);

}
static void clean_module(){
	printf(" Stopping the net test module \n");
}
#else
#include<netinet/in.h>
//#include <sys/types.h>
#include <sys/socket.h>
#include<stdio.h>
typedef struct {
	unsigned long tv_sec;
	unsigned long tv_usec;
}ntime_t;


main(int argc, char *argv[]){
    if (argc != 2) {
        printf("./a.out s/c \n");
        return 1;
    }
    if (argv[1][0]=='c'){
    	start_udp_client();
    }else{
    	start_udp_server();
    }

}
#endif


static unsigned char *tmp_arg[100];
void create_thread(void (*func)(void *arg)){
#ifdef _KERNEL
	int ret;

	tmp_arg[0] = 0;
	tmp_arg[1] = 0;
	tmp_arg[2] = 0;

	ret = sc_createKernelThread(func, &tmp_arg, "Test_net_thread");
#else
	int i=fork();

	if (i==0){
		func();
	}else{
		return i;
	}
#endif
}
int dummy_firstvar;
static ntime_t start_time,end_time;
int stat_server_recv_pkt=0;
int stat_server_recv_bytes=0;
int stat_client_send_pkt=0;
int stat_client_send_bytes=0;

static struct sockaddr_in server, client;
static int server_stop = 0;
static int sfd,cfd;
static char buf[1424] = "";
static int test_debug=0;
int debug_udp_test(){
	if (test_debug == 1){
		test_debug=0;
	}else{
		test_debug=1;
	}
	printf(" changed debug to:%d\n",test_debug);
}
int print_udp_stats(){
   printf(" server recv pkts: %d starttime:%d  endtime:%d  duration:%d\n",stat_server_recv_pkt,start_time.tv_sec,end_time.tv_sec,(end_time.tv_sec-start_time.tv_sec));
}
 int udp_recv_mode=0;
static void server_recv_func() {
	int ret,len;
	//test_debug=1;
	printf(" Starting the server_recv_func  sfd:%d\n",sfd);
	while (server_stop == 0){
		ret = recvfrom(sfd, buf, 1224, 0, (struct sockaddr *) &client, &len);
		//ret = recvfrom(sfd, buf, 1224, 0, (struct sockaddr *) 0, 0);
		if (stat_server_recv_pkt==0){
			gettimeofday(&start_time,0);
		}
		if (test_debug == 1 && ((stat_server_recv_pkt%10000) == 0)){
			printf("recevied the packets  :%d\n",stat_server_recv_pkt);
		}
		if (ret > 0){
			stat_server_recv_pkt++;
			if (udp_recv_mode == 1){
				ret = sendto(sfd, buf, ret, 0, (struct sockaddr *) &client,
						sizeof(client));
			}
			gettimeofday(&end_time,0);
		}else{
			printf("ERROR in recvfrom :%x\n",ret);
			return;
		}
	}
	close(sfd);
	return;
}
void start_udp_server(){
#if _KERNEL
	printf("Starting High priority  udp server\n");
#else
	printf("Starting the udp server\n");
#endif
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	printf(" return of socket :%x \n",sfd);
	server.sin_family = AF_INET;
	server.sin_port = 1300; /* 0x0514 */
	server.sin_port = 0x0514;
	bind(sfd, &server, sizeof(server));
	server_recv_func();

	printf(" duration:sec %x:%x(%d)  usec:%x:%x  \n",end_time.tv_sec,start_time.tv_sec,(end_time.tv_sec-start_time.tv_sec), end_time.tv_usec,start_time.tv_usec);
}


void start_udp_client(){
	int i,ret,n;
#if _KERNEL
	printf("Starting the High priority udp client...\n");
#else
	printf("Starting the udp client...\n");
#endif
	cfd = socket(AF_INET, SOCK_DGRAM, 0);
	printf(" return of socket :%x \n",cfd);
	server.sin_family = AF_INET;
	server.sin_port = 1300;
	server.sin_port = 0x0514;
	server.sin_addr.s_addr = 0xb080d10a; /* 10.209.128.176 */
	n=2000000;
//	n=200;
	gettimeofday(&start_time,0);
#if 1
	for (i=0; i<n; i++){
		ret = sendto(cfd, buf, 50, 0, (struct sockaddr *) &server,
				sizeof(client));
		if (test_debug == 1){
		printf("%d sendto return :%d \n",i,ret);
		}
	}
#endif
	gettimeofday(&end_time,0);
#ifdef _KERNEL
	Jcmd_lsmod("stat",0);
#endif
	printf(" client send pkts: %d starttime:%d  endtime:%d  duration:%d\n",n,start_time.tv_sec,end_time.tv_sec,(end_time.tv_sec-start_time.tv_sec));
}

