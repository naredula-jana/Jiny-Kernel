/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/socket.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
//#define DEBUG_ENABLE 1
#define JFS 1

extern "C"{
#include "common.h"
#include "mm.h"
#include "interface.h"
}
#include "file.hh"
#include "network.hh"

extern "C"{
extern int net_bh();
int g_conf_eat_udp=0;
int default_sock_queue_len=0;
int g_conf_socket_wakeup=1;
static spinlock_t _netstack_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"netstack");
unsigned long stack_flags;
void netstack_lock(){
	sc_enable_nonpreemptive();
	return;
}
void netstack_unlock(){
	sc_disable_nonpreemptive();
	return;
}

int wait_for_sock_data(vinode *inode, int timeout){
	socket *sock=inode;
	return sock->ioctl(SOCK_IOCTL_WAITFORDATA,timeout/10);
}
}
/*
 attach_rawpkt -> add_to_queue
 remove_from_queue->net_stack:read
 */
int socket::attach_rawpkt(unsigned char *buff, unsigned int len) {
	unsigned char *port;
	socket *sock;
	int i;

	struct ether_pkt *pkt = (struct ether_pkt *) (buff + 10);
	if (pkt->iphdr.protocol == IPPROTO_UDP) {
		if (g_conf_eat_udp == 1){
			goto last;
		}
		for (i=0; i<udp_list.size; i++) {
			sock = udp_list.list[i];
			if (sock == 0) continue;
			if ((pkt->udphdr.dest == sock->network_conn.src_port)) {
				sock->add_to_queue(buff, len);
				return JSUCCESS;
			}
		}
	} else if(pkt->iphdr.protocol == IPPROTO_TCP){
		/* first tcp connected clients, then listners */
		for (i=0; i<tcp_connected_list.size; i++) {
			sock = tcp_connected_list.list[i];
			if (sock == 0) continue;
			/* tcp and udp ports are located in same location in raw packet  , source and dest udp port is used to search for connected tcp sockets */
			if ((pkt->udphdr.dest == sock->network_conn.src_port) && (pkt->udphdr.source == sock->network_conn.dest_port)) {
				sock->add_to_queue(buff, len);
				return JSUCCESS;
			}
		}
		for (i=0; i<tcp_listner_list.size; i++) {
			sock = tcp_listner_list.list[i];
			if (sock == 0) continue;
			if ((pkt->udphdr.dest == sock->network_conn.src_port)) {
				sock->add_to_queue(buff, len);
				return JSUCCESS;
			}
		}

	}

	default_socket->add_to_queue(buff, len);
	stat_raw_default++;
	return JSUCCESS;
#if 1
last:
	jfree_page(buff);
	stat_raw_drop++;
	return JSUCCESS;
#endif
}
int socket::add_to_queue(unsigned char *buf, int len) {
	unsigned long flags;
	int ret = JFAIL;
	if (buf == 0 || len == 0)
		return ret;

	//ut_log("Recevied from  network and keeping in queue: len:%d  stat count:%d prod:%d cons:%d\n",len,stat_queue_len,queue.producer,queue.consumer);
	spin_lock_irqsave(&(queue.spin_lock), flags);
	if (queue.data[queue.producer].len == 0) {
		queue.data[queue.producer].len = len;
		queue.data[queue.producer].buf = buf;
		queue.producer++;
		queue.queue_len++;
		if (queue.producer >= MAX_SOCKET_QUEUE_LENGTH){
			queue.producer = 0;
		}
		ret = JSUCCESS;
		goto last;
	}
	queue.error_full++;

last:
	spin_unlock_irqrestore(&(queue.spin_lock), flags);
	if (ret == JFAIL){
		jfree_page(buf);
		stat_raw_drop++;
	}else{
		if (g_conf_socket_wakeup == 1){
			queue.waitq->wakeup();
		}
		stat_raw_attached++;
	}
	return ret;
}

int socket::remove_from_queue(unsigned char **buf, int *len) {
	unsigned long flags;
	int ret = JFAIL;
	while (ret == JFAIL) {
		spin_lock_irqsave(&(queue.spin_lock), flags);
		if ((queue.data[queue.consumer].buf != 0) && (queue.data[queue.consumer].len != 0)) {
			*buf = queue.data[queue.consumer].buf;
			*len = queue.data[queue.consumer].len;
			//	ut_log("netrecv : receving from queue len:%d  prod:%d cons:%d\n",queue.data[queue.consumer].len,queue.producer,queue.consumer);

			queue.data[queue.consumer].len = 0;
			queue.data[queue.consumer].buf = 0;
			queue.consumer++;
			queue.queue_len--;
			//queue.stat_processed[getcpuid()]++;
			if (queue.consumer >= MAX_SOCKET_QUEUE_LENGTH)
				queue.consumer = 0;
			ret = JSUCCESS;
		}
		spin_unlock_irqrestore(&(queue.spin_lock), flags);
		if (ret == JFAIL){
			queue.waitq->wait(500);
		}
	}
	return ret;
}

int socket::read(unsigned long offset, unsigned char *app_data, int app_len, int read_flags) {
	int ret = 0;
	unsigned char *buf = 0;
	int buf_len;

	net_bh();

	/* if the message is peeked, then get the message from the peeked buf*/
	if (peeked_msg_len != 0){
		buf_len = peeked_msg_len;
		if (buf_len > app_len) buf_len=app_len;

		ut_memcpy(app_data,peeked_msg, buf_len);
		peeked_msg_len =0;
		return buf_len;
	}
read_again:
	/* push a packet in to protocol stack  */
	ret = remove_from_queue(&buf, &buf_len);
	if (ret == JSUCCESS) {
		network_connection *conn=&network_conn;
		if (network_conn.type == 0){ /* default socket */
			conn=0;
		}
		ret = net_stack->read(conn, buf, buf_len, app_data, app_len);
		if (ret > 0) {
			stat_in++;
			stat_in_bytes = stat_in_bytes + ret;
		} else{ /* when tcp control packets are consumed, need to look for the data packets */

			stat_in++;
			stat_err++;
			if (buf > 0) {
				free_page((unsigned long)buf);
				buf =0 ;
			}
			if (ret < 0){
				return 0;
			}
			goto read_again;  /* tcp control packets will get observed, nothing ouput */
		}
	}

	if (buf > 0) {
		free_page((unsigned long)buf);
	}
	return ret;
}
int socket::peek(){
	if (peeked_msg_len == 0 ){
		if (peeked_msg == 0){
			peeked_msg = (unsigned long) alloc_page(0);
		}
		peeked_msg_len = read(0,peeked_msg,PAGE_SIZE,0);
	}
	return peeked_msg_len;
}

int socket::write(unsigned long offset, unsigned char *app_data, int app_len, int wr_flags) {
	int ret = 0;
	if (app_data == 0)
		return 0;

	ret = net_stack->write(&network_conn, app_data, app_len);

	if (ret > 0) {
		stat_out++;
		stat_out_bytes = stat_out_bytes + ret;
	} else
		stat_err++;
	return ret;
}
int socket::close() {
	int ret;
	ut_log(" Freeing the socket :%d\n",this->count.counter);
	atomic_dec(&this->count);
	if (this->count.counter > 0){
		ut_log("socket FAILED :%d\n",this->count.counter);
		return JFAIL;
	}
	ret = net_stack->close(&network_conn);
	queue.waitq->unregister();
	if (peeked_msg != 0 ){
		free_page(peeked_msg);
	}

	delete_sock(this);
	// TODO: need to clear the socket
	ut_free(this);
	ut_log("socket freed\n");
	return ret;
}

int socket::ioctl(unsigned long arg1, unsigned long arg2) {
	int ret = SYSCALL_FAIL;

	if (arg1 == SOCK_IOCTL_BIND){
		ret = net_stack->bind(&network_conn, network_conn.src_port);
	}else if (arg1 == SOCK_IOCTL_CONNECT){
		if (network_conn.type == SOCK_DGRAM ){
			ret = SYSCALL_SUCCESS;
		}else{
			ret = net_stack->connect(&network_conn);
		}
	}else if (arg1 == SOCK_IOCTL_WAITFORDATA){
		if ((queue.queue_len == 0)  && (arg2 != 0)){
			queue.waitq->wait(arg2);
			//ut_log(" Woken from the queue : %d\n",queue.queue_len);
		}
		return queue.queue_len;
	}else if (arg1 == GENERIC_IOCTL_PEEK_DATA){
		return queue.queue_len;
	}
	return ret;
}

void socket::print_stats(unsigned char *arg1,unsigned char *arg2){
	ut_printf("socket: count:%d local:%x:%x remote:%x:%x (IO: %d/%d: StatErr: %d Qfull:%d Qlen:%d)  %x\n",
			count.counter,network_conn.src_ip,network_conn.src_port,network_conn.dest_ip,network_conn.dest_port,stat_in
	    ,stat_out,stat_err,queue.error_full,queue.queue_len, &network_conn);
}

sock_list_t socket::udp_list;
sock_list_t socket::tcp_listner_list;
sock_list_t socket::tcp_connected_list;
int socket::stat_raw_drop = 0;
int socket::stat_raw_default = 0;
int socket::stat_raw_attached = 0;
socket *socket::default_socket=0;
/* TODO need  a lock */
jdevice *socket::net_dev;
network_stack *socket::net_stack_list[MAX_NETWORK_STACKS];
static void print_list(sock_list_t *listp){
	int i;
	for (i=0; i<listp->size;i++){
		socket *sock=listp->list[i];
		if (sock==0) continue;
		sock->print_stats(0,0);
	}
	return;
}
void socket::print_all_stats() {
	unsigned char *name = 0;
	int i,len;

	if (net_stack_list[0]){
		name = net_stack_list[0]->name;
	}

	ut_printf("SOCKET  netstack:%s raw_drop:%d raw_attached:%d raw_default: %d\n",  name, stat_raw_drop, stat_raw_attached, stat_raw_default);

	default_socket->print_stats(0,0);
	print_list(&socket::tcp_listner_list);
	print_list(&socket::udp_list);
	print_list(&socket::tcp_connected_list);

	return;
}
static int delete_sock_from_list(sock_list_t *listp,socket *sock){
	int i;
	for (i = 0; i < listp->size; i++) {
		if (listp->list[i] == sock) {
			listp->list[i] = 0;
			return JSUCCESS;
		}
	}
	return JFAIL;
}
int socket::delete_sock(socket *sock) {
	int i;

	arch_spinlock_free(&sock->queue.spin_lock);
	if (delete_sock_from_list(&socket::tcp_listner_list,sock)==JSUCCESS){
		return JSUCCESS;
	}
	if (delete_sock_from_list(&socket::udp_list,sock)==JSUCCESS){
		return JSUCCESS;
	}
	if (delete_sock_from_list(&socket::tcp_connected_list,sock)==JSUCCESS){
		return JSUCCESS;
	}
	return JFAIL;
}
void socket::init_socket(int type){
	//queue.spin_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"socketnetq_lock");
	arch_spinlock_init(&queue.spin_lock, (unsigned char *)"socketnetq_lock" );
	arch_spinlock_link(&_netstack_lock);

	queue.waitq = jnew_obj(wait_queue, "socket_waitq", 0);
	net_stack = net_stack_list[0];
	network_conn.family = AF_INET;
	network_conn.type = type;
	network_conn.src_ip = 0;
	if (type == SOCK_DGRAM){
		network_conn.protocol = IPPROTO_UDP;
		net_stack->open(&network_conn,1);
	}else if (type == SOCK_STREAM){
		network_conn.protocol = IPPROTO_TCP;
		net_stack->open(&network_conn,1);
	} else{
		network_conn.protocol = 0;
	}
	count.counter = 1;
	stat_in = 0;
	stat_in_bytes =0;
	stat_out =0;
	stat_out_bytes =0;
}

extern "C"{
#undef memset
void memset(uint8_t *dest, uint8_t val, long len){ /* user by new by the compiler*/
	uint8_t *temp = (uint8_t *)dest;
	long i;
	DEBUG("memset NEW dest :%x val :%x LEN addr:%x len:%x temp:%x \n",dest,val,&len,len,&temp);/* TODO */
	for ( i=len; i != 0; i--) *temp++ = val;
	return ;
}
}

static int add_sock(sock_list_t *listp,socket *sock){
	int i;
	int found=0;
	for (i = 0; i < listp->size; i++) {
		if (listp->list[i] == 0) {
			listp->list[i] = sock;
			found = 1;
			break;
		}
	}
	if (found == 0) {
		if (listp->size < MAX_SOCKETS) {
			listp->list[listp->size] = sock;
			listp->size++;
			found = 1;
		}else{
			return JFAIL;
		}
	}

	return JSUCCESS;
}
vinode* socket::create_new(int arg_type) {
	sock_list_t *list;
	if (net_stack_list[0] == 0)
		return 0;
	socket *sock = jnew_obj(socket);

	if (arg_type == SOCK_STREAM){
		list = &socket::tcp_listner_list;
	}else if (arg_type == SOCK_STREAM_CHILD){
		arg_type = SOCK_STREAM;
		list = &socket::tcp_connected_list;
	}else {
		list = &socket::udp_list;
	}
	if (add_sock(list, sock) == JFAIL){
		jfree_obj((unsigned long)sock);
		return NULL;
	}

	sock->init_socket(arg_type);
	return (vinode *) sock;
}
int socket::default_pkt_thread(void *arg1, void *arg2){
	while(1){
		default_socket->read(0,0,0,0);
		default_sock_queue_len = default_socket->queue.consumer;
	}
	return 1;
}
void socket::init_socket_layer(){
	int pid;

	default_socket = jnew_obj(socket);
	default_socket->init_socket(0);
	pid = sc_createKernelThread(socket::default_pkt_thread, 0, (unsigned char *) "socket_default",0);
}


/* API calls */
extern "C" {
static int sock_check(int sockfd) {
	if ((sockfd < 0 || sockfd > MAX_FDS)) {
		SYSCALL_DEBUG("ERROR: socket CHECK1 FAILED fd :%x \n", sockfd);
		return JFAIL;
	}
	struct file *file = g_current_task->fs->filep[sockfd];
	if (file == 0 || file->type != NETWORK_FILE || file->vinode == 0) {
		SYSCALL_DEBUG("ERROR: socket CHECK2 FAILED fd :%x \n", sockfd);
		return JFAIL;
	}
	return JSUCCESS;
}

/*
 * socket(AF_INET,SOCK_STREAM,0);
 */
int SYS_socket(int family, int arg_type, int z) {
	void *conn;
	int ret = -1;
	int type = arg_type;

	SYSCALL_DEBUG("socket : family:%x type:%x arg3:%x\n", family, type, z);

	if (arg_type > 2 ){
		SYSCALL_DEBUG("socket : type not supported\n");
		return -1;
	}
	return SYS_fs_open("/dev/sockets", arg_type, 0);
}

int SYS_bind(int fd, struct sockaddr *addr, int len) {
	int ret;
	if (sock_check(fd) == JFAIL || addr == 0) {
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}

	struct file *file = g_current_task->fs->filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;

	struct socket *sock = (struct socket *) file->vinode;
	ret = 0;
	sock->network_conn.src_ip = addr->addr;
	sock->network_conn.src_port = addr->sin_port;

	sock->ioctl(SOCK_IOCTL_BIND,0);
	SYSCALL_DEBUG("Bindret :%x (%d) port:%x\n", ret, ret, addr->sin_port);

	return ret;
}
#define AF_INET         2       /* Internet IP Protocol         */
static uint32_t net_htonl(uint32_t n) {
	return ((n & 0xff) << 24) | ((n & 0xff00) << 8) | ((n & 0xff0000UL) >> 8) | ((n & 0xff000000UL) >> 24);
}

int SYS_getsockname(int sockfd, struct sockaddr *addr, int *addrlen) {
	int ret = SYSCALL_SUCCESS;

	SYSCALL_DEBUG("getsockname %d \n", sockfd);
	if (sock_check(sockfd) == JFAIL || addr == 0) {
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}
#if JFS
	struct file *file = g_current_task->fs->filep[sockfd];
	struct socket *inode;
	if (file == 0) {
		SYSCALL_DEBUG("FAIL  getsockname %d \n", sockfd);
		return SYSCALL_FAIL;
	} else {
		inode = (struct socket *) file->vinode;
		if (inode->network_conn.type == 0) {
			SYSCALL_DEBUG("FAIL  getsockname %d \n", sockfd);
			return SYSCALL_FAIL;
		}
	}
	addr->family = AF_INET;
	//addr->addr = net_htonl(g_network_ip);
	addr->sin_port = inode->network_conn.src_ip;
	addr->sin_port = inode->network_conn.src_port;
	//ret = socket_api->get_addr(file->inode->fs_private, addr);

	SYSCALL_DEBUG("getsocknameret %d ip:%x port:%x ret:%d\n", sockfd, addr->addr, addr->sin_port, ret);
#endif
	return ret;
}

int SYS_accept(int fd) {
	struct file *file, *new_file;
#if JFS
	SYSCALL_DEBUG("accept %d \n", fd);
	if (sock_check(fd) == JFAIL)
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */

	file = g_current_task->fs->filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;
	struct socket *inode = (struct socket *) file->vinode;

/* create child socket */
	int i = SYS_fs_open("/dev/sockets", SOCK_STREAM_CHILD, 0);
	if (i == 0) {
		return SYSCALL_FAIL;
	}

	new_file = g_current_task->fs->filep[i];
	struct socket *new_inode = (struct socket *) new_file->vinode;
	new_inode->network_conn.proto_connection = (unsigned long) 0;
	new_inode->network_conn.src_ip = inode->network_conn.src_ip;
	new_inode->network_conn.src_port = inode->network_conn.src_port;

	new_inode->network_conn.type = SOCK_STREAM;
	new_file->flags = file->flags;

	inode->network_conn.child_connection = &(new_inode->network_conn);

	inode->read(0,0,0,0);

	if (i > 0){
		inode->stat_out++;
	}else{
		inode->stat_err++;
	}
	SYSCALL_DEBUG("accept retfd %d \n", i);

	return i;
#endif
}

int SYS_listen(int fd, int length) {
	SYSCALL_DEBUG("listen fd:%d len:%d\n", fd, length);
	if (sock_check(fd) == JFAIL)
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	return SYSCALL_SUCCESS;
}

int SYS_connect(int fd, struct sockaddr *addr, int len) {
	int ret;
	struct sockaddr ksock_addr;

	SYSCALL_DEBUG("connect %d  addr:%x port:%d len:%d\n", fd, addr->addr, addr->sin_port, len);
	if (sock_check(fd) == JFAIL)
		return 0; /* TCP/IP sockets are not supported */

	if (fd > MAX_FDS || fd < 0)
		return 0;
	struct file *file = g_current_task->fs->filep[fd];
	struct socket *socket = (struct socket *) file->vinode;
	if (file == 0)
		return SYSCALL_FAIL;
	ksock_addr.addr = addr->addr;
	socket->network_conn.dest_ip = addr->addr;
	socket->network_conn.dest_port = addr->sin_port;

//	if (socket->network_conn.proto_connection == 0)
//		return SYSCALL_FAIL;

	ret = socket->ioctl(SOCK_IOCTL_CONNECT,0);

	SYSCALL_DEBUG("connect ret:%d  addr:%x\n", ret, ksock_addr.addr);
	return ret;

}

unsigned long SYS_sendto(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("SENDTO fd:%x buf:%x len:%d flags:%x dest_addr:%x addrlen:%d\n", sockfd, buf, len, flags, dest_addr, addrlen);
	//ut_log("SENDTO fd:%x buf:%x len:%d flags:%x dest_addr:%x addrlen:%d\n", sockfd, buf, len, flags, dest_addr, addrlen);

	//return len;
	if (sock_check(sockfd) == JFAIL || g_current_task->fs->filep[sockfd] == 0)
		return 0;
	struct file *file = g_current_task->fs->filep[sockfd];
	struct socket *sock = (struct socket *) file->vinode;

	if (sock->network_conn.type != SOCK_DGRAM) {
		return 0;
	}
	if (dest_addr > 0) {
		if (dest_addr->addr != 0){
			sock->network_conn.dest_ip = dest_addr->addr;
		}else{
			sock->network_conn.dest_ip = sock->network_conn.src_ip;
		}
		sock->network_conn.dest_port = dest_addr->sin_port;
	}
	unsigned long *tmp_sock = (unsigned long *)sock;
	if (*tmp_sock == 0){ /* TODO: safe check need to remove later, somehow the object data is getting corrupted */
		BUG();
	}
	return sock->write(0, (unsigned char *) buf, len, 0);
}
int SYS_sendmsg(){
	return 0;
}
int SYS_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("RECVfrom fd:%d  buf:%x dest_addr:%x\n", sockfd, buf, dest_addr);
	//ut_log("RECVfrom fd:%d  buf:%x dest_addr:%x\n", sockfd, buf, dest_addr);
	if (sock_check(sockfd) == JFAIL || g_current_task->fs->filep[sockfd] == 0)
		return 0;

	struct file *file = g_current_task->fs->filep[sockfd];
	struct socket *sock = (struct socket *) file->vinode;

	ret = sock->read(0, (unsigned char *) buf, len, 0);
	SYSCALL_DEBUG(" Recv from ret  :%d\n",ret);
	if (dest_addr > 0) {
		dest_addr->addr = sock->network_conn.dest_ip;
		dest_addr->sin_port = sock->network_conn.dest_port;
	}
	return ret;
}


/*
recvmsg(3, {msg_name(16)={sa_family=AF_INET, sin_port=htons(52402), sin_addr=inet_addr("127.0.0.1")}, msg_iov(1)=[{NULL, 0}], msg_controllen=32, {cmsg_len=28, cmsg_level=SOL_IP, cmsg_type=, ...}, msg_flags=MSG_TRUNC}, MSG_PEEK) = 0
;*/
enum
  {
    MSG_OOB     = 0x01, /* Process out-of-band data.  */
    MSG_PEEK        = 0x02, /* Peek at incoming messages.  */
    MSG_DONTROUTE   = 0x04 /* Don't use local routing.  */
  };

int SYS_recvmsg(int sockfd, struct msghdr *msg, int flags){
	int i,ret;

	ut_printf(" RecvMSG fd :%d  msg :%x flags:%x \n",sockfd,msg,flags);
	if (msg == 0){
		return 0;
	}
	ut_printf("  name: %x namelen:%x msg_iov:%x iovlen:%d control:%x controllen:%x flags:%x\n",
			msg->msg_name,msg->msg_namelen, msg->msg_iov, msg->msg_iovlen, msg->msg_control,msg->msg_controllen,msg->msg_flags);

	if (sock_check(sockfd) == JFAIL || g_current_task->fs->filep[sockfd] == 0)
		return 0;

	struct file *file = g_current_task->fs->filep[sockfd];
	struct socket *sock = (struct socket *) file->vinode;

	if (flags & MSG_PEEK){
		ret = sock->peek();
		if (ret > 0){
			struct sockaddr *p = msg->msg_name;
			p->sin_port = sock->network_conn.dest_port;
			p->addr = sock->network_conn.dest_ip;
			p->family = sock->network_conn.family;
			return 0;
		}
	}
	for ( i=0; i<msg->msg_iovlen; i++){
		ut_printf("%d: IOV base :%x len: %x \n",msg->msg_iov[i].iov_base,msg->msg_iov[i].iov_len);
		//if (msg->msg_iov[i].iov_base == 0) return -1;
	}
	return 0;
}
}
