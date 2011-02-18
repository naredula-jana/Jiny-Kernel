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
fileCache_t *filecache;
#define MAX_FDS 50
unsigned long write_one=1;
int sock_fd;
int mypos=-1;
struct {
	int fd,position;
} connections[MAX_FDS];
int curr_index=0;
int guestos_fd=-1;
void init_interrupt()
{
	struct sockaddr_un un;
	sock_fd=socket(PF_FILE, SOCK_STREAM|SOCK_CLOEXEC, 0);
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path, sizeof(un.sun_path), "/tmp/jana" );
	if (connect(sock_fd, (struct sockaddr*) &un, sizeof(un)) < 0) {
		return -1;
	}    
	return 1;  
}

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
				 if (connections[i].position != connections[0].position) 
					guestos_fd=dup(connections[i].fd);	
			}
		}
	}

}
int send_interrupt()
{
	if (guestos_fd == -1 ) return -1;
	if (write(guestos_fd ,&write_one,8)==8) return 1;
	return -2;
}
void init_filecache()
{
	int i;

	filecache->magic_number=FS_MAGIC;	
	filecache->state=STATE_UPDATE_INPROGRESS;
	filecache->version=FS_VERSION;
	for (i=0; i<MAX_FILES; i++)
	{
		filecache->serverFiles[i].state=STATE_INVALID;
	}
	filecache->server_highindex=0;
	for (i=0; i<MAX_REQUESTS; i++)
	{
		filecache->clientRequests[i].state=STATE_INVALID;
	}
	filecache->client_highindex=0;
	filecache->state=STATE_VALID;
}
int init()
{
	int fd;
	char *p;
	init_interrupt();
	fd=open("/dev/shm/ivshmem",O_RDWR);
	if (fd < 1) return 0;	
	p=mmap(0, 2*1024*1024,PROT_WRITE|PROT_READ,MAP_SHARED,fd,0);
	if(p==0) 
		return 0;
	memset(p,0,2*1024*1024);
	filecache=p;	
	init_filecache();
	return 1;
}
int read_file(int i,int c)
{
	int fd,ret;
	filecache->serverFiles[i].state=STATE_UPDATE_INPROGRESS;
	printf("Reading the file :%s: \n",filecache->clientRequests[c].filename);	
	fd=open(filecache->clientRequests[c].filename,O_RDONLY);
	if (fd > 0)
	{
		lseek(fd,filecache->clientRequests[c].offset,SEEK_SET);
		ret=read(fd,filecache->serverFiles[i].filePtr,MAX_BUF);
		filecache->serverFiles[i].len=ret;
		filecache->serverFiles[i].offset=filecache->clientRequests[c].offset;
		printf(" Reading the file :%s: len:%d: ret:%d :\n",filecache->clientRequests[c].filename,ret,ret);
		ret=RESPONSE_DONE;
	}else
	{
		printf(" Response failed \n");
		ret=RESPONSE_FAILED;
	}
	strcpy(filecache->serverFiles[i].filename,filecache->clientRequests[c].filename);
	filecache->serverFiles[i].state=STATE_VALID;
	filecache->clientRequests[c].server_response=ret;
	send_interrupt();
	return ret;
}
int process_request(int c)
{
	int i,j;

	printf(" Processing the request : %d \n",c);
	j=-1;
	for (i=0; i<filecache->server_highindex; i++)
	{
		if (filecache->serverFiles[i].state==STATE_VALID && strcmp(filecache->serverFiles[i].filename,filecache->clientRequests[c].filename)==0)
		{
			read_file(i,c);
			return ;	
		}else if (filecache->serverFiles[i].state==STATE_INVALID && j==-1)
			j=i;
	}
	if (j==-1)
	{
		j=filecache->server_highindex; /* TODO */
		filecache->server_highindex++;
	} 
	read_file(j,c);
	return ;	
}
void recv_thread(void *arg)
{
  while(1)
        {
                recv_msg();
        }
}
main()
{
	pthread_t thread_id;
	int i;
	int ret;
	if (init() == 0) return;

	pthread_create(&thread_id,NULL,recv_thread,0);

	while(1)
	{
		system("sleep 5");
		ret=send_interrupt();	
		printf(" return from send interrupt : %d \n",ret);
		for (i=0; i<filecache->client_highindex; i++)
		{
			if (filecache->clientRequests[i].state==STATE_VALID)
			{
				process_request(i);
			}	
		}		
	}
}


