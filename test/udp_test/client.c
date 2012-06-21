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
int recv_stop=0;
struct sockaddr_in server,client;
int send_pkts,recv_pkts,pkt_size,total_pkts;
void recv_func() {
	char buf[1424] = "";
	int l;
	int rc;

	fd_set readSet;
	FD_ZERO( &readSet);

	struct timeval timeout;

	while (recv_stop == 0) {

		FD_SET( sfd, &readSet);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		rc = select(sfd + 1, &readSet, NULL, NULL, &timeout);
		if (rc != 0) {
			recvfrom(sfd, buf, 1024, 0, (struct sockaddr *) &client, &l);
			recv_pkts++;
#ifdef DEBUG
			printf("MESSAGE FROM CLIENT:%s\n", buf);
#endif
		}
	}
}

unsigned long mtime(){
	struct timeval td;
	gettimeofday(&td, (struct timezone *)0);
	return( td.tv_sec*1000000 + ((double)td.tv_usec));
}

int delay( int us )
{
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = us;
    (void)select( 1, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv );
    return(1);
}

unsigned long wait_till(unsigned long ts){
  unsigned long t=mtime();
  if (t > ts) return t;
  delay((ts-t));
  return ts;
}

send_func(){
	int i;
	char buf[1424]="";
	struct timeval td;
	unsigned long start_time,end_time,time_per_pkt,target_t,curr_t;/* time in milliseconds */

	start_time= mtime();
	end_time=start_time+10*1000000;  /* 10 seconds */

	time_per_pkt=(end_time-start_time)/total_pkts;
	target_t=start_time;
	curr_t=start_time;
	while(curr_t<end_time && send_pkts<total_pkts) {
		sendto(sfd,buf,pkt_size-28,0,(struct sockaddr *)&server,sizeof(server));
#ifdef DEBUG
		printf("packet send : currtime:%d\n",curr_t);
#endif
		send_pkts++;
		target_t=target_t+time_per_pkt;
		curr_t=wait_till(target_t);
	}
}

main(int argc, char *argv[]) 
{ 
	pthread_t recv_thread;  /* thread variables */

	sfd=socket(AF_INET,SOCK_DGRAM,0); 
	bzero(&server,sizeof(server)); 
	server.sin_family=AF_INET; 
	server.sin_port=htons(1300); 
	if (argc != 4) {
		printf("./a.out <ip> <pkt_size> <total_pkts>\n");
		return 1;
	}
	inet_aton(argv[1],&server.sin_addr); 

	send_pkts=recv_pkts=0;

	pkt_size=atoi(argv[2]);
	total_pkts=atoi(argv[3]);

	if (pkt_size<28) {
		printf(" ERROR: packet size should be atleast 28 \n");
		return 0;
	}
	pthread_create(&recv_thread, NULL, (void *) &recv_func, 0);
    send_func();
    recv_stop=1;
	pthread_join(recv_thread, NULL);
	printf("pktsize:%d send:%d recved:%d  loss:%d Bit rate:%d Mbps\n",pkt_size,send_pkts,recv_pkts,(send_pkts-recv_pkts),(recv_pkts*pkt_size*8)/1000000);
} 
