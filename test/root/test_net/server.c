#include<netinet/in.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include<stdio.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include<fcntl.h> 
#include <signal.h>
#define DEBUG 1
int sfd;
int send_on=1;
int testabc=1231;
int recv1;
int recv_stop=0;
struct sockaddr_in server,client;
unsigned long send_pkts,recv_pkts,pkt_size,total_pkts;
void recv_func() {
	char buf[1424] = "";
	int l,ret;
	int rc;

	fd_set readSet;
	FD_ZERO( &readSet);

	struct timeval timeout;

printf(" before while  =%d %d \n",recv_stop,testabc);
printf(" before while  =%d %d \n",recv_stop,testabc);
	while (recv_stop == 0) {
//	while (1) {

		FD_SET( sfd, &readSet);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
rc=2;
printf("before  select :%d \n",rc);
	//	rc = select(sfd + 1, &readSet, NULL, NULL, &timeout);
printf("after select :%d \n",rc);
		if (rc != 0) {
			ret=recvfrom(sfd, buf, 1224, 0, (struct sockaddr *) &client, &l);
			if (ret > 0  && (send_on==1))
                         {
			    ret=sendto(sfd,buf,ret,0,(struct sockaddr *)&client,sizeof(client));
                           if (ret > 0) send_pkts++;
                         }	
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
int duration=30;

void sigTerm(int s){

	printf("send_on:%d pktsize:%d send:%d recved:%d recvMbps: %d \n",send_on,pkt_size,send_pkts,recv_pkts,(recv_pkts*8)/(30*1000));
send_pkts=0;
recv_pkts=0;

}
main(int argc, char *argv[]) 
{ 
	pthread_t recv_thread;  /* thread variables */
printf("recv_pkts:%d send_pkts:%d \n",recv_pkts,send_pkts);
printf("first before while  =%d %d  addr:%p testabcaddr:%p\n",recv_stop,testabc,&recv_stop,&testabc);

	sfd=socket(AF_INET,SOCK_DGRAM,0); 
	bzero(&server,sizeof(server)); 
	server.sin_family=AF_INET; 
	client.sin_family=AF_INET;
	server.sin_port=htons(1300); 
	server.sin_port=1300; 
printf(" recv1:%d before while  =%d %d  addr:%p\n",recv1,recv_stop,testabc,&recv_stop);
	if (argc != 1) {
		printf("./a.out <ip> \n");
		return 1;
	}
	signal(SIGTERM,sigTerm);
	signal(SIGINT,sigTerm);
printf(" before while  =%d %d \n",recv_stop,testabc);
	inet_aton("0.0.0.0",&server.sin_addr);
bind(sfd,&server,sizeof(server));
	send_pkts=recv_pkts=0;

	pkt_size=atoi(argv[2]);
	total_pkts=atoi(argv[3]);
	recv_func();
    recv_stop=1;

	printf("NEW pktsize:%d send:%d recved:%d  loss:%d SBit rate:%d Mbps Rbir rate:%d\n",pkt_size,send_pkts,recv_pkts,(send_pkts-recv_pkts),(send_pkts*pkt_size*8)/(duration*1000000),(recv_pkts*pkt_size*8)/(duration*1000000));
} 
