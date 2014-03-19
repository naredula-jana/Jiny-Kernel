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
#include "file.hh"
#include "network.hh"
extern "C"{
#include "common.h"
#include "mm.h"
#include "interface.h"

extern int g_network_ip;
}
/*
 attach_rawpkt -> add_to_queue
 remove_from_queue->net_stack:read
 */

int socket::attach_rawpkt(unsigned char *buff, unsigned int len, unsigned char **replace_buf) {
	unsigned char *port;
	socket *sock;
	int i;
	for (i = 0; i < list_size; i++) {
		sock = list[i];
		if (sock == 0) {
			continue;
		}
		struct ether_pkt *pkt = (struct ether_pkt *) (buff + 10);

		if ((pkt->iphdr.protocol == sock->network_conn.protocol)) {
			if ((pkt->udphdr.dest == sock->network_conn.src_port)) {
				if (sock->add_to_queue(buff, len) == JSUCCESS) {
					*replace_buf = (unsigned char *) alloc_page(0);
					return JSUCCESS;
				}
			} else {
				ut_log(" recevied Not match :%x :%x \n", pkt->udphdr.dest, sock->network_conn.src_port);
			}
		}
	}
	stat_raw_drop++;
	return JFAIL;
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
		if (ret == JFAIL)
			ipc_waiton_waitqueue(&queue.waitq, 1000);
	}
	return ret;
}
static void *vptr_socket[7] = { (void *) &socket::read, (void *)&socket::write, (void *)&socket::close, (void *) &socket::ioctl,
		0 };

int socket::read(unsigned long offset, unsigned char *app_data, int app_len) {
	int ret = 0;
	unsigned char *buf = 0;
	int buf_len;

	/* push a packet in to protocol stack  */
	ret = remove_from_queue(&buf, &buf_len);
	if (ret == JSUCCESS) {
		ret = net_stack->read(&network_conn, buf, buf_len, app_data, app_len);
		if (ret > 0) {
			stat_in++;
			stat_in_bytes = stat_in_bytes + ret;
		} else
			stat_err++;
	}

	if (buf > 0) {
		free_page((unsigned long)buf);
	}
	return ret;
}
int socket::write(unsigned long offset, unsigned char *app_data, int app_len) {
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

	ret = net_stack->close(&network_conn);
	ipc_unregister_waitqueue(&queue.waitq);
	delete_sock(this);
	// TODO: need to clear the socket
	ut_free(this);
	return ret;
}

socket *socket::list[MAX_SOCKETS];
int socket::list_size = 0;
int socket::stat_raw_drop = 0;
/* TODO need  a lock */
jdevice *socket::net_dev;
network_stack *socket::net_stack_list[MAX_NETWORK_STACKS];
void socket::print_stats() {
	unsigned char *name = 0;
	int i,len;

	if (net_stack_list[0])
		name = net_stack_list[0]->name;
	ut_printf("Socket total:%d netstack:%s raw_drop:%d\n", list_size, name, stat_raw_drop);

	for (i=0; i<list_size;i++){
		socket *sock=list[i];
		if (sock==0) continue;
		ut_printf("socket: local:%x:%x remote:%x:%x (%d:%d/%d:%d/%d) qeueu full error:%d\n",
				sock->network_conn.src_ip,sock->network_conn.src_port,sock->network_conn.dest_ip,sock->network_conn.dest_port,sock->stat_in
		    ,sock->stat_in_bytes,sock->stat_out,sock->stat_out_bytes,sock->stat_err,sock->queue.error_full);
	}
	return;
}
int socket::delete_sock(socket *sock) {
	int i;
	int found = 0;

	for (i = 0; i < list_size && found == 0; i++) {
		if (list[i] == sock) {
			list[i] = 0;
			found = 1;
			break;
		}
	}
	if (found == 1)
		return JSUCCESS;
	else
		return JFAIL;
}
vinode* socket::create_new(int arg_type) {
	int i;
	int found = 0;

	if (net_stack_list[0] == 0)
		return 0;
//	socket *sock = new socket();
	socket *sock = (socket *)ut_calloc(sizeof(socket));
	if (sock == 0)
		return 0;
	void **p = (void **) sock;
	*p = &vptr_socket[0];

	for (i = 0; i < list_size && found == 0; i++) {
		if (list[i] == 0) {
			list[i] = sock;
			found = 1;
			break;
		}
	}
	if (found == 0) {
		if (list_size < MAX_SOCKETS) {
			list[list_size] = sock;
			list_size++;
			found = 1;
		}
	}

	if (found == 0) {
		ut_free(sock);
		return 0;
	}

	sock->queue.spin_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"socketnetq_lock");
	ipc_register_waitqueue(&sock->queue.waitq, "socket_waitq", WAIT_QUEUE_WAKEUP_ONE);
	sock->net_stack = net_stack_list[0];
	sock->network_conn.net_dev = net_dev;
	sock->network_conn.type = arg_type;
	sock->network_conn.src_ip = g_network_ip;
	sock->network_conn.protocol = IPPROTO_UDP;
	return (vinode *) sock;
}

int socket::ioctl(unsigned long arg1, unsigned long arg2) {
	return JSUCCESS;
}

extern "C" {
static int sock_check(int sockfd) {
	if ((sockfd < 0 || sockfd > MAX_FDS)) {
		SYSCALL_DEBUG("ERROR: socket CHECK1 FAILED fd :%x \n", sockfd);
		return JFAIL;
	}
	struct file *file = g_current_task->mm->fs->filep[sockfd];
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

	return SYS_fs_open("/dev/sockets", arg_type, 0);
}

int SYS_bind(int fd, struct sockaddr *addr, int len) {
	int ret;
	if (sock_check(fd) == JFAIL || addr == 0) {
		return SYSCALL_FAIL; /* TCP/IP sockets are not supported */
	}

	struct file *file = g_current_task->mm->fs->filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;

	struct socket *sock = (struct socket *) file->vinode;
	ret = 0;
	//ret = socket_api->bind((void *)sock->network_conn.proto_connection, addr, sock->network_conn.type);
	sock->network_conn.src_ip = addr->addr;
	sock->network_conn.src_port = addr->sin_port;
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
	struct file *file = g_current_task->mm->fs->filep[sockfd];
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
	addr->addr = net_htonl(g_network_ip);
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

	file = g_current_task->mm->fs->filep[fd];
	if (file == 0)
		return SYSCALL_FAIL;
	struct socket *inode = (struct socket *) file->vinode;
//	void *conn = socket_api->accept((void *)inode->network_conn.proto_connection);
//	if (conn == 0)
//		return SYSCALL_FAIL;
	int i = SYS_fs_open("/dev/sockets", 0, 0);
	if (i == 0) {
		return SYSCALL_FAIL;
	}

	new_file = g_current_task->mm->fs->filep[i];
	struct socket *new_inode = (struct socket *) new_file->vinode;
	new_inode->network_conn.proto_connection = (unsigned long) 0;
	new_inode->network_conn.src_ip = inode->network_conn.src_ip;
	new_inode->network_conn.src_port = inode->network_conn.src_port;

	new_inode->network_conn.type = SOCK_STREAM;
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
	struct file *file = g_current_task->mm->fs->filep[fd];
	struct socket *socket = (struct socket *) file->vinode;
	if (file == 0)
		return SYSCALL_FAIL;
	ksock_addr.addr = addr->addr;

	if (socket->network_conn.proto_connection == 0)
		return SYSCALL_FAIL;

//	ret = socket_api->connect((void *)socket->network_conn.proto_connection, &(ksock_addr.addr),
//			addr->sin_port);

	SYSCALL_DEBUG("connect ret:%d  addr:%x\n", ret, ksock_addr.addr);
	return ret;

}

unsigned long SYS_sendto(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("SENDTO fd:%x buf:%x len:%d flags:%x dest_addr:%x addrlen:%d\n", sockfd, buf, len, flags, dest_addr, addrlen);

	if (sock_check(sockfd) == JFAIL || g_current_task->mm->fs->filep[sockfd] == 0)
		return 0;
	struct file *file = g_current_task->mm->fs->filep[sockfd];
	struct socket *sock = (struct socket *) file->vinode;

	if (sock->network_conn.type != SOCK_DGRAM) {
		return 0;
	}
	if (dest_addr > 0) {
		sock->network_conn.dest_ip = dest_addr->addr;
		sock->network_conn.dest_port = dest_addr->sin_port;
	}
	return sock->write(0, (unsigned char *) buf, len);
}

int SYS_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, int addrlen) {
	int ret;
	SYSCALL_DEBUG("RECVfrom fd:%d  buf:%x dest_addr:%x\n", sockfd, buf, dest_addr);
	if (sock_check(sockfd) == JFAIL || g_current_task->mm->fs->filep[sockfd] == 0)
		return 0;

	struct file *file = g_current_task->mm->fs->filep[sockfd];
	struct socket *sock = (struct socket *) file->vinode;

	ret = sock->read(0, (unsigned char *) buf, len);
	if (dest_addr > 0) {
		dest_addr->addr = sock->network_conn.dest_ip;
		dest_addr->sin_port = sock->network_conn.dest_port;
	}
	return ret;
}
}
