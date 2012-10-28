//#define DEBUG_ENABLE 1
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/shm_queue.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#ifndef SHM_SERVER
#include "common.h"
#include "task.h"
#include "interface.h"

#include "shm_queue.h"
int client_add_buf(struct client_queue *q, int desc_id, int type);
unsigned long g_data_offset;
static struct shm_queue* create_shmqueue(unsigned long start_addr, int shm_len, int *desc_length ) {
	struct shm_queue *shmq = start_addr;
	int i, len = 0;
	int buf_size = SHM_PAGE_SIZE+1024;

	len = len + sizeof(struct shm_queue);
	if (((len/SHM_PAGE_SIZE)*SHM_PAGE_SIZE) < len) len=((len/SHM_PAGE_SIZE)+1)*SHM_PAGE_SIZE;
	memset(shmq, 0, sizeof(struct shm_queue));

	for (i = 0; i < MAX_BUF &&  (g_data_offset+buf_size)<shm_len; i++) {
		shmq->desc[i].addr_offset = g_data_offset;
		shmq->desc[i].len = buf_size;
		g_data_offset = g_data_offset + buf_size;
		len =len+buf_size;
	}
    *desc_length=i;
	return shmq;
}

struct client_queue*  client_create_shmqueue(unsigned long shm_addr, int shm_len){
	int i;
	struct client_queue *client_q = ut_malloc(sizeof(struct client_queue));
int desc_length;
	g_data_offset = 8*SHM_PAGE_SIZE;

	client_q->recv_q.shm_queue = create_shmqueue(shm_addr,shm_len,&desc_length);
	client_q->recv_q.desc_head=0;
	client_q->recv_q.desc_list_length=0;
	client_q->recv_q.used_id=0;

	client_q->send_q.shm_queue = create_shmqueue(shm_addr+4*SHM_PAGE_SIZE,shm_len,&desc_length);
	client_q->send_q.desc_head=0;
	client_q->send_q.desc_list_length=desc_length;
	client_q->send_q.used_id=0;
	client_q->shm_start_addr = (unsigned char *)shm_addr;

	/* Fille the Recv queue with empty buffers */
	for (i = 0; i < MAX_BUF ; i++) {

		client_add_buf(client_q,i,RECV);
	}

	return client_q;
}

int client_add_buf(struct client_queue *q, int desc_id, int type) {
	if (type == RECV) {
		int id = q->recv_q.shm_queue->avail.idx;
		q->recv_q.shm_queue->avail.ring[id%MAX_BUF] = desc_id;
		q->recv_q.shm_queue->avail.idx++;
	} else {
		int id = q->send_q.shm_queue->avail.idx;
		q->send_q.shm_queue->avail.ring[id % MAX_BUF] = desc_id;
		q->send_q.shm_queue->avail.idx++;
	}
	return 1;
}

int client_recv_buf(struct client_queue *q) {
	int desc_id;

	if (q->recv_q.used_id != q->send_q.shm_queue->used.idx) {
		desc_id = q->recv_q.shm_queue->used.ring[q->recv_q.used_id].id;
		q->recv_q.used_id++;
		return (desc_id % MAX_BUF);
	}
	return -1;
}

static int get_buf(struct client_queue *q, int type) {
	int desc_id;
	if (type == SEND) {
		if (q->send_q.desc_list_length > 0) {
			q->send_q.desc_list_length--;
			desc_id = (q->send_q.desc_head)%MAX_BUF ;

			q->send_q.desc_head++;
			return desc_id;
		} else {
			if (q->send_q.used_id != q->send_q.shm_queue->used.idx) {
				desc_id = q->send_q.shm_queue->used.ring[q->send_q.used_id%MAX_BUF].id;
				q->send_q.used_id++;
				return desc_id;
			}
		}
	} else {
		if (q->recv_q.used_id != q->recv_q.shm_queue->used.idx) {
			desc_id = q->recv_q.shm_queue->used.ring[q->recv_q.used_id%MAX_BUF].id;
			q->recv_q.used_id++;
			return desc_id;
		}
	}
	return -1;
}

int client_get_buf(struct client_queue *q, struct buf_desc *buf, int type){
	int id = get_buf(q,type);

	if (id == -1) return 0;

	buf->descr_id = id;
	buf->buf = q->shm_start_addr;
	if (type == SEND){
	    buf->buf = buf->buf+q->send_q.shm_queue->desc[id].addr_offset;
		buf->len = q->send_q.shm_queue->desc[id].len;
	}else {
		buf->buf = buf->buf+q->recv_q.shm_queue->desc[id].addr_offset;
		buf->len = q->recv_q.shm_queue->desc[id].len;
	}
	buf->q = q;
	buf->type=type;
	return 1;
}

int client_put_buf(struct buf_desc *buf){
	client_add_buf(buf->q, buf->descr_id, buf->type);
	return 1;
}


struct client_queue *shm_client_init(){

	unsigned char *p;
	struct client_queue *cq;

    p=(unsigned char *)HOST_SHM_ADDR;

	cq = client_create_shmqueue((unsigned long)p,SHM_SIZE);
	return cq;
}

int cl_read(struct client_queue *q, struct buf_desc *bufd) {
	if (client_get_buf(q, bufd, RECV) == 1) {
		return bufd->len;
	} else {
		return 0;
	}
}
int cl_read_free(struct buf_desc *bufd) {
	return client_put_buf(bufd);
}
int cl_write_alloc(struct client_queue *q, struct buf_desc *bufd) {
	return client_get_buf(q, bufd, SEND);
}
int cl_write(struct buf_desc *bufd) {
	return client_put_buf(bufd);
}
#else
/********************************************************************************/

/********************** API calls ***********************************/

struct server_queue* server_attach_shmqueue(unsigned long shm_addr, int shm_len) {
	struct server_queue *server_q = malloc(sizeof(struct server_queue));

   server_q->recv_q.shm_queue = shm_addr;
   server_q->recv_q.avail_id=0;
   server_q->send_q.shm_queue = shm_addr + 4*SHM_PAGE_SIZE;
   server_q->send_q.avail_id=0;

   server_q->shm_start_addr = shm_addr;

   return server_q;
}
int server_put_buf(struct server_queue *q, struct buf_desc *buf) {
	struct sr_queue *sq;
	int type = buf->type;

	if (type == SEND) {
		sq = &q->send_q;
	} else {
		sq = &q->recv_q;
	}
	int id = sq->shm_queue->used.idx% MAX_BUF;
	sq->shm_queue->used.ring[id].id = buf->descr_id;
	sq->shm_queue->used.idx++;
}
int server_get_buf(struct server_queue *q, struct buf_desc *buf, int type) {
	struct sr_queue *sq;

	if (type == SEND) {
		sq = &q->send_q;
	} else {
		sq = &q->recv_q;
	}

	if (sq->avail_id != sq->shm_queue->avail.idx) {
		int id = sq->avail_id % MAX_BUF;
		sq->avail_id++;
		buf->descr_id = sq->shm_queue->avail.ring[id];

		buf->buf = q->shm_start_addr;
		buf->buf = buf->buf + sq->shm_queue->desc[buf->descr_id].addr_offset;
		buf->len = sq->shm_queue->desc[buf->descr_id].len;

		buf->type = type;
		return 1;
	}
	return 0;
}
#endif
