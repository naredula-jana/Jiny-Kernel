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
#include "file.h"
extern "C"{
#include "common.h"
#include "mm.h"
#include "interface.h"
struct Socket_API *socket_api = 0;
static int direct_sockrecv=0;
extern int network_call_softnet(unsigned char *data, int len);
int lwip_peek_msgs(void *arg);
extern int g_network_ip;
}
/*
 * Posting Thread:
 * #0  do_mbox_post (mbox=0x402cb498, msg=0x402bdb80) at lwip-arch.c:130
#1  0x0000000040125ed4 in sys_mbox_trypost (mbox=0x402cb498, msg=0x402bdb80) at lwip-arch.c:158
#2  0x0000000040129401 in recv_udp (arg=0x402ca738, pcb=0x402b7510, p=0x402eee6c, addr=0x4045dca0, port=49152) at /opt_src/lwip/src/api/api_msg.c:195
#3  0x0000000040141315 in udp_input (p=0x402eee6c, inp=0x7f8ac020) at /opt_src/lwip/src/core/udp.c:330
#4  0x000000004013e497 in ip_input (p=0x402eee6c, inp=0x7f8ac020) at /opt_src/lwip/src/core/ipv4/ip.c:499
#5  0x0000000040149472 in ethernet_input (p=0x402eee6c, netif=0x7f8ac020) at /opt_src/lwip/src/netif/etharp.c:1282
#6  0x0000000040124151 in netfront_input (netif=0x7f8ac020, data=0x7fe90071 "ok", len=0) at lwip-net.c:160
#7  0x00000000401241dc in lwip_netif_rx (data=0x7fe90000 "\002", len=103, private_data=0x0) at lwip-net.c:173
#8  0x0000000040151cec in netRx_BH (arg=0x0) at network.c:167
#9  0x000000004015c302 in schedule_kernelSecondHalf () at task.c:1048
 *
 * Receving thread:
 * #0  do_mbox_fetch (mbox=0x402cb498, msg=0x7f6a3d38) at lwip-arch.c:174
#1  0x0000000040125ff7 in sys_arch_mbox_fetch (mbox=0x402cb498, msg=0x7f6a3d38, timeout=0) at lwip-arch.c:203
#2  0x000000004012c726 in netconn_recv_data (conn=0x402ca738, new_buf=0x7f6a3de8) at /opt_src/lwip/src/api/api_lib.c:366
#3  0x000000004012ca7c in netconn_recv (conn=0x402ca738, new_buf=0x7f6a3de8) at /opt_src/lwip/src/api/api_lib.c:492
#4  0x000000004012488d in lwip_sock_read_from (conn=0x402ca738, buff=0x7f8a24e0 "", len=1224, sockaddr=0x7f8c811c, addr_len=16) at lwip-net.c:291
#5  0x000000004011831c in socket::read (this=0x7f8c8020, offset=0, data=0x7f8a24e0 "", len=1224) at socket.cc:106
#6  0x0000000040119c7a in SYS_recvfrom (sockfd=3, buf=0x7f8a24e0, len=1224, flags=0, dest_addr=0x7f8a398d, addrlen=2137669392) at socket.cc:481


attach_rawpkt -> add_to_queue
remove_from_queue->net_stack:read

 */
int socket::attach_rawpkt(unsigned char *c, unsigned int len, unsigned char **replace_buf){
	unsigned char *port;
	socket *sock;
	if (direct_sockrecv!=1){
		return JFAIL;
	}
	sock = socket::list[0];
	if (sock != 0){
		port= (unsigned char *)sock->dest_addr.sin_port;
	}else{
		return JFAIL;
	}
	/* TODO: need to queue to the proper socket based on: src_ip-port,dest_ip-port, protocol. */
	//if ( c[33] == 0x11 && port[0]==c[46] && port[1]==c[47]){ /* udp packets with matching remote port */
		if ( sock->add_to_queue(c,len)==JSUCCESS){
			*replace_buf = (unsigned char *)alloc_page(0);
		}
	//}
	return JFAIL;
}
int socket::add_to_queue(unsigned char *buf, int len) {
	unsigned long flags;
	int ret=JFAIL;
	if (buf == 0 || len == 0)
		return ret;

	//ut_log("Recevied from  network and keeping in queue: len:%d  stat count:%d prod:%d cons:%d\n",len,stat_queue_len,queue.producer,queue.consumer);
	spin_lock_irqsave(&(queue.spin_lock), flags);
	if (queue.data[queue.producer].len == 0) {
		queue.data[queue.producer].len = len;
	//	*replace_outbuf = queue.data[queue.producer].buf;
		queue.data[queue.producer].buf = buf;
		queue.producer++;
		queue.queue_len++;
		if (queue.producer >= MAX_SOCKET_QUEUE_LENGTH)
			queue.producer = 0;
		ret = JSUCCESS;
		ipc_wakeup_waitqueue(&queue.waitq);
		goto last;
	}
	queue.error_full++;
last:
	spin_unlock_irqrestore(&(queue.spin_lock), flags);
	return ret;
}
int socket::remove_from_queue(unsigned char **buf,  int *len) {
	unsigned long flags;
	int ret=JFAIL;
	while (ret == JFAIL) {
		spin_lock_irqsave(&(queue.spin_lock), flags);
		if ((queue.data[queue.consumer].buf != 0)
				&& (queue.data[queue.consumer].len != 0)) {
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
		if (ret == JFAIL)
			ipc_waiton_waitqueue(&queue.waitq, 1000);
	}
	return ret;
}
static void *vptr_socket[7]={(void *)&socket::read,(void *)&socket::write,(void *)&socket::close,
		(void *)&socket::ioctl,0};



int socket::read(unsigned long offset, unsigned char *app_data, int app_len) {
	int ret = 0;
	unsigned char *buf=0;
	int buf_len;

	if (proto_connection == 0) return SYSCALL_FAIL;
	/* push a packet in to protocol stack  */
	while (direct_sockrecv==1){
		ret = remove_from_queue(&buf, &buf_len);

		if (ret == JSUCCESS){
			if (net_stack != 0){
			//	net_stack->read(buf,buf_len,app_data,app_len);
			}else{

			}
//			ret = network_call_softnet(buf,buf_len);
		}

#ifdef LWIP_NONMODULE // TODO : need to replaced with the generic function
		if (lwip_peek_msgs(proto_connection) > 0){
			break;
		}
#endif
	}
	/* read the packet from protocol stack */
	ret = socket_api->read_from( proto_connection, app_data, app_len, (uint32_t *)&dest_addr, sizeof(dest_addr));
	SYSCALL_DEBUG("RECVFROM return with len :%d \n", ret);
	if (ret > 0){
		stat_in++;
		stat_in_bytes = stat_in_bytes + ret;
	}else
		stat_err++;
	if (buf > 0){
		free_page((unsigned long)buf);
	}
	return ret;
}
int socket::write(unsigned long offset, unsigned char *data, int len) {
	int ret = 0;

	if (proto_connection == 0) return SYSCALL_FAIL;
	ret = socket_api->write((void *) proto_connection, data, len, sock_type, dest_addr.addr, dest_addr.sin_port);
	SYSCALL_DEBUG("Write return with len :%d \n", ret);
	if (ret > 0){
		stat_out++;
		stat_out_bytes = stat_out_bytes + ret;
	}else
		stat_err++;
	return ret;
}
vinode* socket::create_new(){
	socket *sock= (socket *) mm_slab_cache_alloc(socket::slab_objects, 0);
	if (sock == 0) return 0;

	int i;
	void **p=(void **)sock;
	*p=&vptr_socket[0];
	socket::list[socket::list_size] = sock;
	socket::list_size++;

	ut_memset((unsigned char *) &sock->queue, 0, sizeof(struct sock_queue_struct));
	for (i=0; i<MAX_SOCKET_QUEUE_LENGTH; i++){
		sock->queue.data[i].buf = 0;
		sock->queue.data[i].len = 0; /* this represent actual data length */
	}
	sock->queue.spin_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"socketnetq_lock");
	ipc_register_waitqueue(&sock->queue.waitq, "socket_waitq",WAIT_QUEUE_WAKEUP_ONE);

	return (vinode *)sock;
}
int socket::close(){
	return JSUCCESS;
}
int socket::ioctl(unsigned long arg1,unsigned long arg2){
	return JSUCCESS;
}

extern "C"{

int Jcmd_sockrecv(){
	if (direct_sockrecv==0){
		direct_sockrecv=1;
	}else{
		direct_sockrecv=0;
	}
	ut_printf(" direct_sockrecv :%d\n",direct_sockrecv);
	return 0;
}


int register_to_socketLayer(struct Socket_API *api) {
	if (api == 0)
		return 0;
	socket_api = api;
	return 1;
}
int unregister_to_socketLayer() {
	socket_api = 0;
	return 1;
}
static int sock_check(int sockfd){
	if (socket_api == 0 || (sockfd<0 || sockfd>MAX_FDS)){
		SYSCALL_DEBUG("ERROR: socket CHECK1 FAILED fd :%x \n",sockfd);
		return JFAIL;
	}
	struct file *file = g_current_task->mm->fs.filep[sockfd];
	if (file==0 || file->type != NETWORK_FILE || file->vinode==0){
		SYSCALL_DEBUG("ERROR: socket CHECK2 FAILED fd :%x \n",sockfd);
		return JFAIL;
	}
	return JSUCCESS;
}

int socket_close(struct file *file) {
	int ret;
	struct socket *inode=(struct socket *)file->vinode;
#if  0
	if (inode->count.counter > 1){
		fs_putInode(inode);
		return 1;
	}
#endif
	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}
	if (inode->proto_connection != 0){
		ret = socket_api->close((void *)inode->proto_connection, inode->sock_type);
	}else{
		ret = SYSCALL_SUCCESS;
	}
//	fs_putInode(inode);

	return ret;
}
#if 0
int socket_read(struct file *file, unsigned char *buff, unsigned long len) {
	int ret;
	struct socket *inode=(struct socket *)file->vinode;

	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}
#if JFS
	if (buff==0){
		return socket_api->check_data((void *)inode->fs_private);
	}
	ret = socket_api->read((void *)inode->fs_private, buff, len);

	SYSCALL_DEBUG("sock read: read bytes :%d \n", ret);
	if (ret > 0)
		inode->stat_in++;
	else
		inode->stat_err++;
#endif
	return ret;
}

int socket_write(struct file *file, unsigned char *buff, unsigned long len) {
	int ret;
	unsigned char *kbuf;
	int klen;
	struct socket *inode=(struct socket *)file->vinode;

	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}

	klen = 3000;
	kbuf = mm_malloc(klen, 0);
	if (len < klen)
		klen = len;
	ut_memcpy(kbuf, buff, klen);
#if JFS
	ret = socket_api->write((void *)inode->fs_private, kbuf, klen, inode->u.socket.sock_type, 0, 0);
	mm_free(kbuf);
	SYSCALL_DEBUG("sock write: written bytes :%d \n", ret);
	if (ret > 0)
		inode->stat_out++;
	else
		inode->stat_err++;
#endif
	return ret;
}
#endif
/*
 * socket(AF_INET,SOCK_STREAM,0);
 */
int SYS_socket(int family, int arg_type, int z) {
	void *conn;
	int ret=-1;
	int type=arg_type;

	SYSCALL_DEBUG("socket : family:%x type:%x arg3:%x\n", family, type, z);
	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}

	if (socket_api->open) {
		conn = socket_api->open(type);
		if (conn == 0){
		}
	} else {
		ret = -1;
		//goto error;
		return ret;
	}

	ret = SYS_fs_open("/dev/sockets", 0, 0);
	if (ret < 0){
		socket_api->close(conn, 0);
		//goto error;
		return ret;
	}

	struct file *file = g_current_task->mm->fs.filep[ret];

	if (file == 0) {
		socket_api->close(conn, 0);
		ret = -3;
		//goto error;
		return ret;
	}

	struct socket *sock=(struct socket *)file->vinode;
	sock->proto_connection = (void *)conn;
	sock->sock_type = type;

//error:
	SYSCALL_DEBUG("socket ret :%x (%d) \n",ret,-ret);
	if (ret < 0) ret = SYSCALL_FAIL;
	return ret;
}

int SYS_bind(int fd, struct sockaddr *addr, int len) {
	int ret;
	if (sock_check(fd) == JFAIL || addr == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}
	SYSCALL_DEBUG("Bind fd:%d ip:%x port:%x len:%d\n", fd, addr->addr, addr->sin_port, len);
	if (socket_api->bind == 0)
		return SYSCALL_FAIL;

	struct file *file = g_current_task->mm->fs.filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;
#if 0  // TODO : remove later , only for testing ftp app
	if (fd !=3 ){
		static int test_port=3001;
		addr->sin_port=test_port;
		test_port++;
	}
#endif

	struct socket *sock=(struct socket *)file->vinode;
	ret = socket_api->bind((void *)sock->proto_connection, addr, sock->sock_type);
	sock->local_addr = addr->addr;
	sock->local_port = addr->sin_port;
	SYSCALL_DEBUG("Bindret :%x (%d) port:%x\n",ret,ret,addr->sin_port);

	return ret;
}
#define AF_INET         2       /* Internet IP Protocol         */
static uint32_t net_htonl(uint32_t n)
{
  return ((n & 0xff) << 24) |
    ((n & 0xff00) << 8) |
    ((n & 0xff0000UL) >> 8) |
    ((n & 0xff000000UL) >> 24);
}

int SYS_getsockname(int sockfd, struct sockaddr *addr, int *addrlen){
	int ret=SYSCALL_SUCCESS;
	SYSCALL_DEBUG("getsockname %d \n", sockfd);
	if (sock_check(sockfd) == JFAIL || addr == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}
#if JFS
	struct file *file = g_current_task->mm->fs.filep[sockfd];
	struct socket *inode;
	if (file == 0){
		SYSCALL_DEBUG("FAIL  getsockname %d \n", sockfd);
		return SYSCALL_FAIL;
	}else{
		inode=(struct socket *)file->vinode;
		if (inode->sock_type==0){
			SYSCALL_DEBUG("FAIL  getsockname %d \n", sockfd);
			return SYSCALL_FAIL;
		}
	}

	addr->family = AF_INET;
	addr->addr = inode->local_addr ;
	addr->addr = net_htonl(g_network_ip);
	addr->sin_port = inode->local_port;
	//ret = socket_api->get_addr(file->inode->fs_private, addr);

	SYSCALL_DEBUG("getsocknameret %d ip:%x port:%x ret:%d\n", sockfd, addr->addr, addr->sin_port,ret);
#endif
	return ret;
}

int SYS_accept(int fd) {
	struct file *file,*new_file;
#if JFS
	SYSCALL_DEBUG("accept %d \n", fd);
	if (sock_check(fd) == JFAIL )
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */

	if (socket_api->accept == 0)
		return 0;
	file = g_current_task->mm->fs.filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;
	struct socket *inode=(struct socket *)file->vinode;
	void *conn = socket_api->accept((void *)inode->proto_connection);
	if (conn == 0)
		return SYSCALL_FAIL;
	int i = SYS_fs_open("/dev/sockets", 0, 0);
	if (i == 0) {
		socket_api->close(conn,0);
		return SYSCALL_FAIL;
	}

	new_file = g_current_task->mm->fs.filep[i];
	struct socket *new_inode=(struct socket *)new_file->vinode;
	new_inode->proto_connection = (unsigned long)conn;
	new_inode->local_addr = inode->local_addr;
	new_inode->local_port = inode->local_port;

	new_inode->sock_type = SOCK_STREAM;
	new_file->flags = file->flags;

	if (i > 0)
		inode->stat_out++;
	else
		inode->stat_err++;
	SYSCALL_DEBUG("accept retfd %d \n", i);

	return i;
#endif
}

int SYS_listen(int fd, int length) {
	SYSCALL_DEBUG("listen fd:%d len:%d\n", fd, length);
	if (sock_check(fd) == JFAIL )
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	return SYSCALL_SUCCESS;
}

int SYS_connect(int fd, struct sockaddr *addr, int len) {
	int ret;
	struct sockaddr ksock_addr;

	SYSCALL_DEBUG("connect %d  addr:%x port:%d len:%d\n", fd, addr->addr, addr->sin_port, len);
	if (sock_check(fd) == JFAIL )
		return 0; /* TCP/IP sockets are not supported */

	if (socket_api->connect == 0)
		return 0;
	if (fd > MAX_FDS || fd < 0)
		return 0;
	struct file *file = g_current_task->mm->fs.filep[fd];
	struct socket *socket=(struct socket *)file->vinode;
	if (file == 0)
		return SYSCALL_FAIL;
	ksock_addr.addr = addr->addr;

	if (socket->proto_connection == 0) return SYSCALL_FAIL;

	ret = socket_api->connect((void *)socket->proto_connection, &(ksock_addr.addr),
			addr->sin_port);

	SYSCALL_DEBUG("connect ret:%d  addr:%x\n", ret, ksock_addr.addr);
	return ret;

}

unsigned long SYS_sendto(int sockfd,  void *buf, size_t len, int flags,
		 struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("SENDTO fd:%x buf:%x len:%d flags:%x dest_addr:%x addrlen:%d\n", sockfd, buf, len, flags, dest_addr, addrlen);

	if (sock_check(sockfd) == JFAIL
			|| g_current_task->mm->fs.filep[sockfd] == 0)
		return 0;
	struct file *file = g_current_task->mm->fs.filep[sockfd];
	struct socket *sock=(struct socket *)file->vinode;

	if (sock->sock_type != SOCK_DGRAM){
		return 0;
	}
	if (dest_addr > 0){
		sock->dest_addr.addr = dest_addr->addr ;
		sock->dest_addr.sin_port = dest_addr->sin_port;
	}
	return sock->write(0,(unsigned char *)buf,len);
}

int SYS_recvfrom(int sockfd,  void *buf, size_t len, int flags,
		 struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("RECVfrom fd:%d  buf:%x dest_addr:%x\n", sockfd, buf, dest_addr);
	if (sock_check(sockfd) == JFAIL  || g_current_task->mm->fs.filep[sockfd] == 0)
		return 0;

	struct file *file = g_current_task->mm->fs.filep[sockfd];
	struct socket *sock=(struct socket *)file->vinode;

	ret = sock->read(0,(unsigned char *) buf,  len);
	if (dest_addr > 0){
		dest_addr->addr = sock->dest_addr.addr;
		dest_addr->sin_port = sock->dest_addr.sin_port;
	}
	return ret;
}
}
