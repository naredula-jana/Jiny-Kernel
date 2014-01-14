/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/network.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 */

/*  Description:
 *   - Middle layer between network drivers(like virtio..)  and protocol stacks(lwip, udp responser .. etc)
 *   - It works as a bottom Half for network packets.
 *   - It should come up before drivers and protocol stacks, so that they hook to this layer
 *
packet flows
flow-1)  virtio interrupt-> netif_rx -> addto-queue ----(interrupt context)

flow-2)  remove_from_queue -> protocol_driver_callback-lwip_netif_rx----  (netRx_BH)
#1  0x000000004013acda in tcp_receive (pcb=0x402b81a0) at /opt_src/lwip/src/core/tcp_in.c:1009
#2  0x000000004013a2bc in tcp_process (pcb=0x402b81a0) at /opt_src/lwip/src/core/tcp_in.c:737
#3  0x000000004013957d in tcp_input (p=0x402cf806, inp=0x7ff68020) at /opt_src/lwip/src/core/tcp_in.c:319
#4  0x0000000040132db3 in ip_input (p=0x402cf806, inp=0x7ff68020) at /opt_src/lwip/src/core/ipv4/ip.c:505
#5  0x000000004013dd99 in ethernet_input (p=0x402cf806, netif=0x7ff68020) at /opt_src/lwip/src/netif/etharp.c:1282
#6  0x00000000401245d1 in netfront_input (netif=0x7ff68020, data=0x7ff8504a "\213Q\004\205\322t\003\211B\020\203\071", len=0) at lwip-net.c:189
#7  0x000000004012463d in lwip_netif_rx (data=0x7ff85000 "", len=64, private_data=0x0) at lwip-net.c:224
#8  0x0000000040144931 in netRx_BH (arg=0x0) at network.c:87

flow-3)  low_level_output -> netif_tx -> driver_callback-netdriver_xmit --- ( protcol thread context)
#0  netdriver_xmit (data=0x402cf81e "", len=52, private_data=0x7fff1020) at virtio_net.c:183
#1  0x0000000040144aaa in netif_tx (data=0x402cf81e "", len=52) at network.c:124
#2  0x00000000401243c9 in low_level_output (netif=0x7ff62020, p=0x402cf806) at lwip-net.c:104
#3  0x000000004013d4bf in etharp_arp_input (netif=0x7ff62020, ethaddr=0x7ff62063, p=0x402cf806) at /opt_src/lwip/src/netif/etharp.c:787
#4  0x000000004013dddb in ethernet_input (p=0x402cf806, netif=0x7ff62020) at /opt_src/lwip/src/netif/etharp.c:1291
#5  0x00000000401245d1 in netfront_input (netif=0x7ff62020, data=0x7ffe303e "\211\320\350\374\377\377\377\353\005\350\374\377\377\377\211C<\203{<", len=0) at lwip-net.c:189
#6  0x000000004012463d in lwip_netif_rx (data=0x7ffe3000 "", len=52, private_data=0x0) at lwip-net.c:224
#7  0x0000000040144941 in netRx_BH (arg=0x0) at network.c:87

 flow-2 and flow-3  callbacks are registered with the function registerNetworkHandler
 recv path :  buf allocated in virtio , freed in netRX_BH after copying the content in protocol layer to pbuf
 send path : buf is allocated in netdriver_xmit(virtio)
 */

//#define DEBUG_ENABLE 1
#include "common.h"

#define MAX_QUEUE_LENGTH 600
struct queue_struct {
	wait_queue_t waitq;
	int producer, consumer;
	struct {
		unsigned char *buf;
		unsigned int len; /* actual data length , not the buf lenegth, buf always constant length */
	} data[MAX_QUEUE_LENGTH];
	spinlock_t spin_lock; /* lock to protect while adding and revoing from queue */
	int stat_processed[MAX_CPUS];
	int queue_len;
	int error_full;

};
static struct queue_struct queue;

#define MAX_HANDLERS 10
struct net_handlers {
	void *private;
	int (*callback)(unsigned char *buf, unsigned int len, void *private_data);
	unsigned char *mac;
};
static int count_net_drivers = 0;
static int count_protocol_drivers = 0;
static struct net_handlers net_drivers[MAX_HANDLERS];
static struct net_handlers protocol_drivers[MAX_HANDLERS];
static int poll_devices();

int g_network_ip=0x0ad181b0; /* 10.209.129.176  */
static int stat_from_driver = 0;
static int stat_to_driver = 0;
/***********************************************************************
 *  1) for every buf is added will get back a replace buf
 *  2) for every buf removed will given back the replace buf.
 *  Using 1) and 2) techniques allocation and free  is completely avoided from driver to queue and vice versa.
 *  the Cost is the queue should be prefilled with some buffers.
 *
 *    path: netif_rx -> add_to_queue -> netRx_BH
 *
 *
 */
static unsigned char *get_freebuf(){
	/* size of buf is hardcoded to 4096, this is linked to network driverbuffer size */
	return (unsigned char *)alloc_page(0);
}
static int add_to_queue(unsigned char *buf, int len, unsigned char **replace_outbuf) {
	unsigned long flags;
	int ret=0;
	if (buf == 0 || len == 0)
		return ret;

	//ut_log("Recevied from  network and keeping in queue: len:%d  stat count:%d prod:%d cons:%d\n",len,stat_queue_len,queue.producer,queue.consumer);
	spin_lock_irqsave(&(queue.spin_lock), flags);
	if (queue.data[queue.producer].len == 0) {
		queue.data[queue.producer].len = len;
		*replace_outbuf = queue.data[queue.producer].buf;
		queue.data[queue.producer].buf = buf;
		queue.producer++;
		queue.queue_len++;
		if (queue.producer >= MAX_QUEUE_LENGTH)
			queue.producer = 0;
		ret = 1;
		goto last;
	}
	queue.error_full++;
last:
	spin_unlock_irqrestore(&(queue.spin_lock), flags);
	return ret;
}
static int remove_from_queue(unsigned char **buf, unsigned int *len, unsigned char *replace_inbuf) {
	unsigned long flags;
	int ret=0;
	spin_lock_irqsave(&(queue.spin_lock), flags);
	if ((queue.data[queue.consumer].buf != 0)
			&& (queue.data[queue.consumer].len != 0)) {
		*buf = queue.data[queue.consumer].buf;
		*len = queue.data[queue.consumer].len;

		//	ut_log("netrecv : receving from queue len:%d  prod:%d cons:%d\n",queue.data[queue.consumer].len,queue.producer,queue.consumer);

		queue.data[queue.consumer].len = 0;
		queue.data[queue.consumer].buf = replace_inbuf;
		queue.consumer++;
		queue.queue_len--;
		queue.stat_processed[getcpuid()]++;
		if (queue.consumer >= MAX_QUEUE_LENGTH)
			queue.consumer = 0;
		ret  = 1;
	}
	spin_unlock_irqrestore(&(queue.spin_lock), flags);
	return ret;
}
void *g_netBH_lock = 0; /* All BH code will serialised by this lock */
static int stat_netrx_bh_recvs=0;
/*   Release the buffer after calling the callback */
static int netRx_BH(void *arg) {
	unsigned char *data;
	unsigned int len;
	int qret;
	unsigned char *replace_buf = get_freebuf();

	while (1) {
		poll_devices();

		qret = remove_from_queue(&data, &len, replace_buf);
		if (qret ==0 ){
			ipc_waiton_waitqueue(&queue.waitq, 1000);
			qret = remove_from_queue(&data, &len, replace_buf);
		}

		while ( qret == 1) {
			int ret = 0;
			stat_netrx_bh_recvs++;
			if (count_protocol_drivers > 0) {
				ret = protocol_drivers[0].callback(data, len,
						protocol_drivers[0].private);
			}
			if (ret == 0) {

			}
			replace_buf = data;

			qret = remove_from_queue(&data, &len, replace_buf);
		}
	}
	return 1;
}
int net_getmac(unsigned char *mac) {
	unsigned char *driver_mac = net_drivers[0].mac;
	int i;

	if (driver_mac == 0) {
		return JFAIL;
	}
	if (count_net_drivers > 0) {
		for (i = 0; i < 6; i++) {
			mac[i]=driver_mac[i];
		}
		return JSUCCESS;
	}
	return JFAIL;
}
int registerNetworkHandler(int type,
		int (*callback)(unsigned char *buf, unsigned int len,
				void *private_data), void *private_data, unsigned char *mac) {
	if (type == NETWORK_PROTOCOLSTACK) {
		protocol_drivers[count_protocol_drivers].callback = callback;
		protocol_drivers[count_protocol_drivers].private = private_data;
		protocol_drivers[count_protocol_drivers].mac=0;
		count_protocol_drivers++;
	} else {
		net_drivers[count_net_drivers].callback = callback;
		net_drivers[count_net_drivers].private = private_data;
		net_drivers[count_net_drivers].mac = mac;
		count_net_drivers++;
	}
	return 1;
}

int unregisterNetworkHandler(int type,
		int (*callback)(unsigned char *buf, unsigned int len,
				void *private_data), void *private_data) {
	int i;
	if (type == NETWORK_PROTOCOLSTACK) {
		for (i = 0; i < count_protocol_drivers; i++) {
			if (protocol_drivers[i].callback == callback) {
				protocol_drivers[i].callback = 0;
				protocol_drivers[count_protocol_drivers].private = 0;
			}
		}
	} else {
		for (i = 0; i < count_protocol_drivers; i++) {
			if (net_drivers[i].callback == callback) {
				net_drivers[i].callback = 0;
				net_drivers[i].private = private_data;
			}
		}
	}
	return 1;
}
static int network_enabled=0;

#define MAX_POLL_DEVICES 5
struct device_under_poll_struct{
	void *private_data;
	int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts);
	int active;
};
static struct device_under_poll_struct device_under_poll[MAX_POLL_DEVICES];

int netif_rx_enable_polling(void *private_data,
		int (*poll_func)(void *private_data, int enable_interrupt,
				int total_pkts)) {
	int i;
	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data == 0 || device_under_poll[i].private_data == private_data) {
			device_under_poll[i].private_data = private_data;
			device_under_poll[i].poll_func = poll_func;
			device_under_poll[i].active = 1;
			ipc_wakeup_waitqueue(&queue.waitq);
			return 1;
		}
	}
	return 0;
}
static int init_polldevices(){
	int i;
	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		device_under_poll[i].private_data =0;
		device_under_poll[i].active = 0;
	}
}
static int poll_devices() {
	int i,j;
	int ret = 0;
	static int poll_underway = 0;

	mutexLock(g_netBH_lock);
	if (poll_underway == 0) {
		poll_underway = 1;
		ret = 1;
	}
	mutexUnLock(g_netBH_lock);
	if (ret == 0)
		return ret;

	for (i = 0; i < MAX_POLL_DEVICES; i++) {
		if (device_under_poll[i].private_data != 0
				&& device_under_poll[i].active == 1) {
			int max_pkts=50;
			int total_pkts=0;
			int pkts=0;
			for (j=0; j<20; j++){
				pkts = device_under_poll[i].poll_func(device_under_poll[i].private_data, 0, max_pkts);
				if (pkts==0 ) break;
				total_pkts = total_pkts+pkts;
			}
			if (total_pkts == 0){
				device_under_poll[i].active = 0;
				device_under_poll[i].poll_func(device_under_poll[i].private_data, 1, max_pkts); /* enable interrupts */
			}else{
				if (total_pkts > 2){ /* if there are more then 2 unprocessed packets wake up any sleeping thread to picku[p */
					ipc_wakeup_waitqueue(&queue.waitq);
				}
			}
		}
	}
	poll_underway = 0;
	return 1;
}
int netif_rx(unsigned char *data, unsigned int len, unsigned char **replace_buf) {
	if (network_enabled==0){
		*replace_buf = data;
		return 0;
	}
	if (add_to_queue(data, len,replace_buf) == 0 && data != 0 && len != 0) {
		*replace_buf = data; /* fail to queue, so the packet is getting dropped and freed  */
	}else{
		ipc_wakeup_waitqueue(&queue.waitq); /* wake all consumers , i.e all the netBH */
	}
	stat_from_driver++;
	if (*replace_buf < (unsigned char *)0x10000){ /* invalid address*/
		BUG();
	}
	return 1;
}
int netif_tx(unsigned char *data, unsigned int len) {
	int ret;

	if (network_enabled==0){
		return 0;
	}
	if (data == 0 || len == 0)
		return 0;
	stat_to_driver++;
	if (count_net_drivers > 0) {
		//	ut_log("netif_tx: sending packet to network len:%d \n",len);
		ret = net_drivers[0].callback(data, len, net_drivers[0].private);
		return 1;
	} else {

	}
	return 0;
}
int init_networking() {
	unsigned long pid;
	int i;

	init_polldevices();
	ut_memset((unsigned char *) &queue, 0, sizeof(struct queue_struct));
	for (i=0; i<MAX_QUEUE_LENGTH; i++){
		queue.data[i].buf = get_freebuf();
		queue.data[i].len = 0; /* this represent actual data length */
		if (queue.data[i].buf == 0){
			BUG();
		}
	}
	queue.spin_lock = SPIN_LOCK_UNLOCKED("netq_lock");
	ipc_register_waitqueue(&queue.waitq, "netRx_BH",WAIT_QUEUE_WAKEUP_ONE);
	//ipc_register_waitqueue(&queue.waitq, "netRx_BH",0);
	g_netBH_lock = mutexCreate("mutex_netBH");
	pid = sc_createKernelThread(netRx_BH, 0, (unsigned char *) "netRx_BH_1");
	pid = sc_createKernelThread(netRx_BH, 0, (unsigned char *) "netRx_BH_2");
	pid = sc_createKernelThread(netRx_BH, 0, (unsigned char *) "netRx_BH_3");
	network_enabled = 1;
	return 0;
}
/*******************************************************************************************
 Socket layer
 ********************************************************************************************/
static struct Socket_API *socket_api = 0;
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

int socket_close(struct file *file) {
	int ret;
	if (file->inode->count.counter > 1){
		fs_putInode(file->inode);
		return 1;
	}
	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}
	if (file->inode->fs_private != 0){
		ret = socket_api->close((void *)file->inode->fs_private, file->inode->u.socket.sock_type);
	}else{
		ret = SYSCALL_SUCCESS;
	}
	fs_putInode(file->inode);
	return ret;
}

int socket_read(struct file *file, unsigned char *buff, unsigned long len) {
	int ret;

	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}

	if (buff==0){
		return socket_api->check_data((void *)file->inode->fs_private);
	}
	ret = socket_api->read((void *)file->inode->fs_private, buff, len);

	SYSCALL_DEBUG("sock read: read bytes :%d \n", ret);
	if (ret > 0)
		file->inode->stat_in++;
	else
		file->inode->stat_err++;
	return ret;
}

int socket_write(struct file *file, unsigned char *buff, unsigned long len) {
	int ret;
	unsigned char *kbuf;
	int klen;

	if (socket_api == 0){
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}

	klen = 3000;
	kbuf = mm_malloc(klen, 0);
	if (len < klen)
		klen = len;
	ut_memcpy(kbuf, buff, klen);
	ret = socket_api->write((void *)file->inode->fs_private, kbuf, klen, file->inode->u.socket.sock_type, 0, 0);
	mm_free(kbuf);
	SYSCALL_DEBUG("sock write: written bytes :%d \n", ret);
	if (ret > 0)
		file->inode->stat_out++;
	else
		file->inode->stat_err++;

	return ret;
}

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
		goto last;
	}

	ret = SYS_fs_open("/dev/sockets", 0, 0);
	if (ret < 0){
		socket_api->close(conn, 0);
		goto last;
	}
	struct file *file = g_current_task->mm->fs.filep[ret];
	if (file == 0) {
		socket_api->close(conn, 0);
		ret = -3;
		goto last;
	}
	file->inode->fs_private = (unsigned long)conn;
	file->inode->u.socket.sock_type = type;

last:
	SYSCALL_DEBUG("socket ret :%x (%d) \n",ret,-ret);
	if (ret < 0) ret = SYSCALL_FAIL;
	return ret;
}
static int sock_check(int sockfd){
	if (socket_api == 0 || (sockfd<0 || sockfd>MAX_FDS)){
		SYSCALL_DEBUG("ERROR: socket CHECK1 FAILED fd :%x \n",sockfd);
		return JFAIL;
	}
	struct file *file = g_current_task->mm->fs.filep[sockfd];
	if (file==0 || file->type != NETWORK_FILE || file->inode==0){
		SYSCALL_DEBUG("ERROR: socket CHECK2 FAILED fd :%x \n",sockfd);
		return JFAIL;
	}
	return JSUCCESS;
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

	ret = socket_api->bind((void *)file->inode->fs_private, addr, file->inode->u.socket.sock_type);
	file->inode->u.socket.local_addr = addr->addr;
	file->inode->u.socket.local_port = addr->sin_port;
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

	struct file *file = g_current_task->mm->fs.filep[sockfd];
	if (file == 0 || file->inode->u.socket.sock_type==0){
		SYSCALL_DEBUG("FAIL  getsockname %d \n", sockfd);
		return SYSCALL_FAIL;
	}
	addr->family = AF_INET;
	addr->addr = file->inode->u.socket.local_addr ;
	addr->addr = net_htonl(g_network_ip);
	addr->sin_port = file->inode->u.socket.local_port;
	//ret = socket_api->get_addr(file->inode->fs_private, addr);

	SYSCALL_DEBUG("getsocknameret %d ip:%x port:%x ret:%d\n", sockfd, addr->addr, addr->sin_port,ret);
	return ret;
}
int SYS_accept(int fd) {
	struct file *file,*new_file;

	SYSCALL_DEBUG("accept %d \n", fd);
	if (sock_check(fd) == JFAIL )
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */

	if (socket_api->accept == 0)
		return 0;
	file = g_current_task->mm->fs.filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;
	void *conn = socket_api->accept((void *)file->inode->fs_private);
	if (conn == 0)
		return SYSCALL_FAIL;
	int i = SYS_fs_open("/dev/sockets", 0, 0);
	if (i == 0) {
		socket_api->close(conn,0);
		return SYSCALL_FAIL;
	}
	new_file = g_current_task->mm->fs.filep[i];
	new_file->inode->fs_private = (unsigned long)conn;
	new_file->inode->u.socket.local_addr = file->inode->u.socket.local_addr;
	new_file->inode->u.socket.local_port = file->inode->u.socket.local_port;

	new_file->inode->u.socket.sock_type = SOCK_STREAM;
	new_file->flags = file->flags;

	if (i > 0)
		file->inode->stat_out++;
	else
		file->inode->stat_err++;
	SYSCALL_DEBUG("accept retfd %d \n", i);
	return i;
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
	if (file == 0)
		return SYSCALL_FAIL;
	ksock_addr.addr = addr->addr;
	if (file->inode->fs_private == 0) return SYSCALL_FAIL;

	ret = socket_api->connect((void *)file->inode->fs_private, &(ksock_addr.addr),
			addr->sin_port);
	SYSCALL_DEBUG("connect ret:%d  addr:%x\n", ret, ksock_addr.addr);
	return ret;

}
unsigned long SYS_sendto(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *dest_addr, int addrlen) {
	int ret;

	SYSCALL_DEBUG("SENDTO fd:%x buf:%x len:%d flags:%x dest_addr:%x addrlen:%d\n", sockfd, buf, len, flags, dest_addr, addrlen);

	if (sock_check(sockfd) == JFAIL
			|| g_current_task->mm->fs.filep[sockfd] == 0)
		return 0;
	struct file *file = g_current_task->mm->fs.filep[sockfd];

	if (file->inode->u.socket.sock_type != SOCK_DGRAM){
		return 0;
	}
	ret = socket_api->write((void *)file->inode->fs_private, buf, len, file->inode->u.socket.sock_type,
			dest_addr->addr, dest_addr->sin_port);
	if (ret > 0)
		file->inode->stat_out++;
	else
		file->inode->stat_err++;
	return ret;
}

int SYS_recvfrom(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("RECVfrom fd:%d  buf:%x dest_addr:%x\n", sockfd, buf, dest_addr);
	if (sock_check(sockfd) == JFAIL  || g_current_task->mm->fs.filep[sockfd] == 0)
		return 0;
	struct file *file = g_current_task->mm->fs.filep[sockfd];

	ret = socket_api->read_from((void *)file->inode->fs_private, buf, len, dest_addr, addrlen);

	SYSCALL_DEBUG("RECVFROM return with len :%d \n", ret);
	if (ret > 0)
		file->inode->stat_in++;
	else
		file->inode->stat_err++;

	return ret;
}

void Jcmd_network(unsigned char *arg1, unsigned char *arg2) {
	if (socket_api == 0)
		return;
	socket_api->network_status(arg1, arg2);
	ut_printf(" queue full error:%d  producer:%d consumer:%d  from_net:%d to_net:%d rxBhRecvs:%d\n",
			queue.error_full, queue.producer, queue.consumer, stat_from_driver,
			stat_to_driver,stat_netrx_bh_recvs);
	return;
}
