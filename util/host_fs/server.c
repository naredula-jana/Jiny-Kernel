#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "filecache_schema.h"
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <pthread.h>
//#define DEBUG 1
unsigned char *start_addr;
#define MAX_FDS 50
unsigned long write_one=1;
int sock_fd;
int mypos=-1;
struct {
	int fd,position;
} connections[MAX_FDS];
int curr_index=0;
int guestos_fd=-1;
int guestos_fd2=-1;
int my_pos=0;


#define SHM_SERVER
#include "../../drivers/hostshm/shm_queue.h"
#include "../../drivers/hostshm/shm_queue.c"
#ifdef DEBUG
void printf(const char *format, ...) {

}
#endif
#include "qemu_interface.c"
void init_interrupt()
{
	struct sockaddr_un un;
	sock_fd=socket(PF_FILE, SOCK_STREAM|SOCK_CLOEXEC, 0);
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path, sizeof(un.sun_path), "/tmp/ivshmem_socket" );
	if (connect(sock_fd, (struct sockaddr*) &un, sizeof(un)) < 0) {
		return -1;
	}    
	return 1;  
}
//#define NULL 0
int sleepms(int msecs) {
	struct timeval timeout;
	if (msecs == 0) {
		return 0;
	}
	timeout.tv_sec = msecs / 1000; /* in secs */
	timeout.tv_usec = (msecs % 1000) * 1000; /* in micro secs */
	if ((select(0,NULL,NULL,NULL,&timeout))==-1) {
		perror(" select failed\n");
		return -1;
	}
	return 0;
} /* sleepms()*/
int recv_msg()
{
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	struct cmsghdr *cmsg;
	size_t len;
	size_t msg_size = sizeof(int);
	//char control[CMSG_SPACE(msg_size)];
	char control[1024],data[1024];
	long posn = 0;
	int added,pos,*posp;
	int i,fd;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);
	msg.msg_flags = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	iov[0].iov_base = data;
	iov[0].iov_len = sizeof(data)-1;
	len = recvmsg(sock_fd, &msg, 0);

	added=0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {

		if (cmsg->cmsg_len != CMSG_LEN(sizeof(int)) ||
				cmsg->cmsg_level != SOL_SOCKET ||
				cmsg->cmsg_type != SCM_RIGHTS)
		{
			fd = *((int *)CMSG_DATA(cmsg));
			printf("DELETING the fd :%d \n",fd);	
			continue;
		}
		posp=&data;
		pos=*posp;
		fd = *((int *)CMSG_DATA(cmsg));
		for (i=0; i<curr_index;i++)
		{
			if (connections[i].fd != -1)  continue;
			connections[i].fd=fd;
			connections[i].position=pos;
			printf("ADDING in existing  FD :%d pos:%d \n",fd,pos);	
			added=1;
			break;
		}
		if (i==curr_index)
		{
			curr_index++;	
			connections[i].fd=fd;
			connections[i].position=pos;
			added=1;
			printf("ADDING in NEW : FD :%d pos:%d \n",fd,pos);	
		}
	}
	if (added==0)
	{
		posp=&data;
		pos=*posp;
		for (i=0; i<curr_index;i++)
		{
			if (connections[i].position==pos)
			{
				printf("DELETING the fd :%d \n",connections[i].fd);	
				connections[i].position=-1;
				connections[i].fd=-1;
				if (i==2 && guestos_fd != -1) 
				{
					close(guestos_fd);
				}	
			}
		}
	}else
	{
		if (i==2) /* guest os fd */
		{
			if (connections[2].fd != -1)
			{

				for (i=1; i<3;i++)
					if (connections[i].position != connections[0].position) {
						printf("Guest os FD for interrupt : fd:%d position:%d \n",connections[i].fd,connections[0].position);
						guestos_fd=dup(connections[i].fd);	
						my_pos=connections[0].position;

						guestos_fd2=dup(connections[2].fd);

#if 0
						guestos_fd2=dup(connections[2].fd);
						system("sleep 10");
						printf("generating interrupt on :%d...\n",guestos_fd2);
						write(guestos_fd2 ,&write_one,8);
#endif
					}
			}
		}
	}

}
int send_interrupt()
{
	int ret=0;
#if 0
	send_vm_irq();
	return 1;
#endif
	if (guestos_fd == -1 ) ret= -1;
	else if (write(guestos_fd ,&write_one,8)==8) ret= 1;
	else ret= -2;
	printf("Send interrupt fd:%d ret :%d \n",guestos_fd,ret);
	write(guestos_fd2 ,&write_one,8);
	return ret;
}

int init()
{
	int fd;
	unsigned char *p;
	init_interrupt();
	fd=open("/dev/shm/ivshmem",O_RDWR);
	if (fd < 1) 
	{
		printf(" ERROR in opening shared memory \n");
		return 0;	
	}
	p=mmap(0, 2*1024*1024,PROT_WRITE|PROT_READ,MAP_SHARED,fd,0);
	if(p==0) 
		return 0;
	start_addr=p;


	return 1;
}
int open_file(Request_t *recv_req,Request_t *send_req)
{
	int fd,ret;
	unsigned char *p;
	struct stat stat;
#if 1
	printf("open the file :%s: flags:\n",recv_req->filename);
	if (recv_req->flags == FLAG_CREATE)
		fd=open(recv_req->filename,O_WRONLY|O_CREAT|O_EXCL, 0644);
	else
		fd=open(recv_req->filename,O_RDONLY);
	if (fd > 0)
	{
		send_req->response_len=0;
		ret=fstat(fd,&stat);
		if (ret ==0)
		{

			send_req->response_len=stat.st_size;
			send_req->mtime_sec=stat.st_mtim.tv_sec;
			send_req->mtime_nsec=stat.st_mtim.tv_nsec;
		}


		printf("open file :%s: len:%d:  :\n",recv_req->filename,send_req->response_len);

		ret=RESPONSE_DONE;
	}else
	{
		printf(" Response failed \n");
		ret=RESPONSE_FAILED;
	}
	close(fd);	
	send_req->response=ret;
#endif
	send_interrupt();
	return ret;
}
int rw_file(Request_t *recv_req,Request_t *send_req)
{
	int fd,ret;
	unsigned char *p;
#if 1
	printf("RW the file :%s: \n",recv_req->filename);
	if (recv_req->type == REQUEST_READ)
		fd=open(recv_req->filename,O_RDONLY);
	else
		fd=open(recv_req->filename,O_WRONLY);
	if (fd > 0)
	{
		lseek(fd,recv_req->file_offset,SEEK_SET);
		printf(" file offset :%d \n",recv_req->file_offset);

		if (recv_req->type == REQUEST_READ)
			ret=read(fd,send_req->data,recv_req->request_len);
		else
			ret=write(fd,recv_req->data,recv_req->request_len);
		send_req->response_len=ret;
		printf("New  Reading the file :%s: len:%d:  offset:%x :\n",recv_req->filename,ret,recv_req->file_offset);
		ret=RESPONSE_DONE;
	}else
	{
		printf(" Response failed \n");
		ret=RESPONSE_FAILED;
	}
	send_req->response=ret;
#endif
	send_interrupt();
	if (fd > 0)
		close(fd);
	return ret;
}

int process_request(Request_t *recv_req,Request_t *send_req)
{

	switch (recv_req->type)
	{
		case REQUEST_OPEN : open_file(recv_req,send_req); break;
		case REQUEST_READ : 
		case REQUEST_WRITE : rw_file(recv_req,send_req); break;
		default:  printf("unable to process the request\n");break;
	}
	send_req->guestos_pos =my_pos;
	return ;	
}
void recv_thread(void *arg)
{
	while(1)
	{
		recv_msg();
	}
}
int sleep_on_fd() {
	fd_set rfds;
	struct timeval tv;

	int retval;
	if (guestos_fd2 == -1)
		return;
	/* Watch on guest os fd. */
	FD_ZERO(&rfds);
	FD_SET(guestos_fd2, &rfds);

	/* Wait up to five seconds. */
	tv.tv_sec = 7;
	tv.tv_usec = 0;

	retval = select(guestos_fd2 + 1, &rfds, NULL, NULL, &tv);
	if (retval == 1) {
		unsigned char buf[200];
		read(guestos_fd2, buf, 100);
	}
	/* Don't rely on the value of tv now! */
	return retval;
}
main() {
	pthread_t thread_id1, thread_id2;
	int i;
	int requests, ret;
	struct server_queue *sq;
	struct buf_desc rbufd, sbufd;
	int served = 0;

	init();
	sq = server_attach_shmqueue(start_addr, SHM_SIZE);
	if (sq == 0) {
		printf("Fail to attach the shm \n");
	}
	//pthread_create(&thread_id1,NULL,qemu_thread,0);
	pthread_create(&thread_id2, NULL, recv_thread, 0);
	printf(" .. Ready to Process \n");
	while (1) {
		int *p1, *p2;
		Request_t *recv_req, *send_req;
		if (server_get_buf(sq, &rbufd, SEND) == 0) {
			sleep_on_fd();
			//sleepms(50);
			continue;
		}

		while (server_get_buf(sq, &sbufd, RECV) == 0) {
			sleepms(10);
			printf("Waiting for empty buffer to send  \n");
			continue;
		}
		p1 = rbufd.buf;
		p2 = sbufd.buf;
		served++;
		recv_req = p1;
		send_req = p2;
		printf("Recvied :%d  p2:%d served:%d\n", *p1, *p2, served);
		process_request(recv_req, send_req);

		server_put_buf(sq, &rbufd);
		server_put_buf(sq, &sbufd);
	}

}


