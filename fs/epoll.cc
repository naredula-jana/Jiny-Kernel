/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/epoll.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#include "file.hh"

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

struct epoll_event {
	uint32_t events;
	uint64_t data;
} __attribute__((packed));

int epoll_close(struct file *filep){
	struct epoll_struct *epoll_p;

	if (filep->vinode !=0){
		epoll_p = filep->vinode;
		if (epoll_p->waitq){
			epoll_p->waitq->unregister();
		}
		ut_free(filep->vinode);
		filep->vinode =0;
	}
	return 0;
}

void vinode::epoll_fd_wakeup(void){
	int i;
#if 0
	ut_log(" epoll fd wake up:%d\n",epoll_list[0]);
#endif

	data_available_for_consumption = 1;
	for (i=0; i<MAX_EFDS_PER_FD; i++){
		if (epoll_list[i]==0 ){
			return;
		}
		epoll_list[i]->fd_waiting = 1;
		epoll_list[i]->waitq->wakeup();
#if 0
		ut_log(" epoll fd wakeed up:%d \n",i);
#endif
	}
}

extern "C"{
int socketpair_close(struct file *filep){
	struct epoll_struct *epoll_p;

	if (filep->vinode !=0){

	}
	return 0;
}
int SYS_epoll_create(int unused_flags) {
	int fd = -1;
	struct file *filep;
	struct epoll_struct *epoll_p;

	SYSCALL_DEBUG("epoll_create : \n");
	filep = fs_create_filep(&fd, 0);
	if (filep != 0) {
		filep->type = EVENT_POLL_FILE;
		filep->vinode = ut_calloc(sizeof(struct epoll_struct));
		epoll_p = filep->vinode;
		epoll_p->waitq = jnew_obj(wait_queue, "epoll_waitq", 0);
	}
	return fd;
}

int SYS_epoll_ctl(uint32_t  efd, uint32_t op, uint32_t fd, struct epoll_event *unused_event){
	struct file *efilep,*filep;
	struct epoll_struct *epoll_p;
	int i,k;
	vinode *vinode;

	SYSCALL_DEBUG("epoll_ctl : efd:%d fd:%d op:%d\n", efd,fd,op);
	efilep = fd_to_file(efd);
	if (efilep == 0 || efilep->type!=EVENT_POLL_FILE){
		SYSCALL_DEBUG("ERRO epoll_ctl efd:%d  \n", efd);
		return -1;
	}
	epoll_p = efilep->vinode;
	filep = fd_to_file(fd);
	if (filep == 0 || filep->type==EVENT_POLL_FILE || filep->vinode==0){
		SYSCALL_DEBUG("ERRO epoll_ctl fd:%d  \n", fd);
		return -1;
	}
	vinode=filep->vinode;
	if (op == EPOLL_CTL_ADD){
		for (i=0; i<epoll_p->count; i++){
			if (epoll_p->fds[i] == fd){
				SYSCALL_DEBUG("ERRO epoll_ctl fd:%d  fd already present\n", fd);
				return -1;
			}
		}
		i=epoll_p->count;
		if (i>=(MAX_EFDS_PER_FD-1)){
			SYSCALL_DEBUG("ERRO epoll_ctl fd:%d  no space in efds\n", fd);
			return -1;
		}
		SYSCALL_DEBUG("epoll_ctl added at %d  fd:%d \n",i,fd);
		epoll_p->fds[i] =fd;
		epoll_p->count++;
		for (i=0; i<MAX_EFDS_PER_FD; i++){
			if (vinode->epoll_list[i] == 0){
				vinode->epoll_list[i]  = epoll_p;
				SYSCALL_DEBUG("epoll_ctl added at %d inside inode\n",i);
				break;
			}
		}
		if (i==MAX_EFDS_PER_FD){
			SYSCALL_DEBUG("ERRO epoll_ctl fd:%d  MAX FD reached \n", fd);
			return -1;
		}
	}else if (op == EPOLL_CTL_DEL){
		for (i=0; i<epoll_p->count; i++){
			if (epoll_p->fds[i] == fd){
				int j;
				epoll_p->fds[i] = -1;
				for (k=0; k<MAX_EFDS_PER_FD; k++){
					if (vinode->epoll_list[k] == epoll_p){
						vinode->epoll_list[k] = 0;
						for (j=MAX_EFDS_PER_FD-1; j>k; j--){
							if (vinode->epoll_list[j] != 0 ){
								vinode->epoll_list[k] = vinode->epoll_list[j];
								vinode->epoll_list[j] = 0;
								break;
							}
						}
					}
				}

				return 0;
			}
		}
	}
	return 0;
}
#define EPOLLIN          0x00000001
static int get_fds(struct epoll_struct *epoll_p,struct epoll_event *events, uint32_t maxevents){
	int i,fd,e;
	struct file *efilep;
	struct file *filep;

	e=0;
	epoll_p->fd_waiting = 0;
	for (i=0; i<epoll_p->count && e<maxevents; i++){
		fd = epoll_p->fds[i];
		if (fd == -1) continue;
		filep = fd_to_file(fd);
		vinode *vinode = filep->vinode;
		if (vinode->data_available_for_consumption){
			events[e].data = fd;
			events[e].events = EPOLLIN;
			e++;
		}
	}
#if 0
	ut_log(" epoll: get_fd got sockets:%d\n",e);
#endif
	if (e > 0){
		epoll_p->fd_waiting = 1;
	}
	return e;
}
int SYS_epoll_wait(uint32_t efd, struct epoll_event *events, uint32_t maxevents, uint32_t timeout){
	struct file *efilep;
	struct epoll_struct *epoll_p;
	int fd,i,e=0;
	int ret=0;

	SYSCALL_DEBUG("epoll_wait efd:%d maxevents:%d timeout:%d\n", efd,maxevents,timeout);
	efilep = fd_to_file(efd);
	if (efilep == 0 || efilep->type!=EVENT_POLL_FILE){
		SYSCALL_DEBUG("ERRO epoll_ctl efd:%d  \n", efd);
		return -1;
	}
	epoll_p=efilep->vinode;
	if (epoll_p->fd_waiting == 1){
		ret = get_fds(epoll_p,events,maxevents);
	}
	if (ret <= 0){
		epoll_p->waitq->wait(timeout);
		if (epoll_p->fd_waiting == 1){
			ret = get_fds(epoll_p,events,maxevents);
		}
	}
	SYSCALL_DEBUG("epoll_wait ret:%d \n",ret);
	return ret;
}
}
