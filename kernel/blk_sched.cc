/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   kernel/blk_sched.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/

#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
#include "mach_dep.h"
extern int p9_initFs(void *p);
extern void print_vq(struct virtqueue *_vq);
extern int g_conf_net_send_int_disable;
}
#include "jdevice.h"

extern "C"{
extern int  g_conf_read_ahead_pages;
}
extern wait_queue *disk_thread_waitq;

static unsigned long current_req_no=0;
static unsigned long current_txt_no=0;
static unsigned long stat_diskcopy_txts=0;  /*  for non-pagecache disk pages are copied to user space */
static unsigned long stat_read_ahead_txts=0;
#define MAX_DISK_REQS 100
struct diskio_req{
	struct virtio_blk_req *buf;  /* memory buffer, if this empty then rest of entries are invalid */
	int data_len;
	jdiskdriver *dev;
	int state;
	int read_ahead;
	unsigned long req_no,serial_no;
	int initial_skip;
	unsigned long hits;
	unsigned char *user_buf;
	int user_len;
};
struct diskio_req disk_reqs[MAX_DISK_REQS];
static spinlock_t diskio_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"diskio");

enum {
	STATE_REQ_QUEUED =1,
	STATE_REQ_COMPLETED = 2
};
extern "C" {
void Jcmd_stat_diskio(unsigned char *arg1, unsigned char *arg2){
	int i;
	ut_printf(" current req_no:%d  txt+no: %d diskcopies: %d read_ahead:%d \n",current_req_no,current_req_no,stat_diskcopy_txts,stat_read_ahead_txts);
	for (i=0; i<MAX_DISK_REQS; i++){
		if (disk_reqs[i].buf == 0 && disk_reqs[i].hits==0) continue;
		ut_printf(" %d: buf:%x hits:%d req_no:%d \n",i,disk_reqs[i].buf,disk_reqs[i].hits,disk_reqs[i].req_no);
	}

	ut_printf("  disk devices: \n");
	for (i=0; i<5; i++){
		if (disk_drivers[i]!=0){
			virtio_disk_jdriver *dev=disk_drivers[i];
			//print_vq(dev->queues[0].send);
			dev->queues[0].send->print_stats(0,0);
		}
	}
}
extern int pc_read_ahead_complete(unsigned long addr);
}

static int get_from_diskreq(int i, unsigned long req_no) {
	int ret;
	int initial_skip;
	int tlen;
	struct virtio_blk_req *req;

	ret = 0;
	if (disk_reqs[i].buf == 0) {
		return ret;
	}
	if (disk_reqs[i].req_no == req_no && disk_reqs[i].buf != 0) {
		unsigned long start_time = g_jiffies;
		while (disk_reqs[i].state != STATE_REQ_COMPLETED) {
			disk_reqs[i].dev->waitq->wait(5);
			if ((g_jiffies-start_time) > 300) { /* if it more then 3 seconds */
				return -1;
			}
		}
		initial_skip = disk_reqs[i].initial_skip;
		req = disk_reqs[i].buf;
		if ((VIRTIO_BLK_DATA_SIZE - initial_skip) > disk_reqs[i].user_len) {
			tlen = disk_reqs[i].user_len;
		} else {
			tlen = VIRTIO_BLK_DATA_SIZE - initial_skip;
		}
		if (req->user_data == 0) {
			ut_memcpy(disk_reqs[i].user_buf + (i * VIRTIO_BLK_DATA_SIZE),
					&req->data[initial_skip], tlen);
			stat_diskcopy_txts++;
			mm_putFreePages(disk_reqs[i].buf, 1);
		} else {
			mm_putFreePages(disk_reqs[i].buf, 0);
		}
		ret = ret + tlen;
		if (disk_reqs[i].read_ahead == 1) {
			pc_read_ahead_complete((unsigned long) disk_reqs[i].user_buf);
			stat_read_ahead_txts++;
			disk_reqs[i].read_ahead = 0;
		}
		disk_reqs[i].buf = 0;
	}

	return ret;
}

int diskio_submit_requests(struct virtio_blk_req **reqs, int req_count, virtio_disk_jdriver *dev, unsigned char *user_buf,int user_len,int initial_skip, int read_ahead){
	int i,k;
	int ret = -1;
	unsigned long flags;
	unsigned long req_no=0;

	k=0;
	if (read_ahead == 1 ){
		if ( req_count > 1){
			return 0;
		}else{
			if (!(user_buf >= pc_startaddr && user_buf<pc_endaddr)) { /* make sure the addr is from pc */
				return 0;
			}
		}
	}

	spin_lock_irqsave(&diskio_lock, flags);
	if (current_req_no == 0){
		current_req_no =1;
	}
	current_req_no++;
	req_no = current_req_no;
	for (i=0; i<MAX_DISK_REQS && k<req_count; i++){
		if (disk_reqs[i].buf!=0) { continue; }
		disk_reqs[i].dev = dev;
		disk_reqs[i].data_len = reqs[k]->len;
		disk_reqs[i].state = 0;
		disk_reqs[i].req_no = req_no;
		//ut_log("%d: user_buf :%x len :%d donotcopy:%d data:%x \n",current_req_no,user_buf,user_len,donot_copy,reqs[k]->user_data);
		disk_reqs[i].serial_no = k;
		if (k==0){
			disk_reqs[i].initial_skip  = initial_skip;
		}else{
			disk_reqs[i].initial_skip  = 0;
		}
		disk_reqs[i].read_ahead = read_ahead;
		disk_reqs[i].user_buf = user_buf;
		disk_reqs[i].user_len = user_len;
		disk_reqs[i].buf = reqs[k];
		disk_reqs[i].hits++;
		ret = i;
		k++;
		current_txt_no++;
	}
	spin_unlock_irqrestore(&diskio_lock, flags);
	if (ret >= 0){
		if (read_ahead == 1){
			int count=0;
			for (i=0; i<MAX_DISK_REQS ; i++){
				if (disk_reqs[i].buf!=0 && disk_reqs[i].state==0) { count++; }
			}
			if (count > (g_conf_read_ahead_pages/2)){
					disk_thread_waitq->wakeup();
			}
			ret = user_len;
		}else{
			disk_thread_waitq->wakeup(); /* check if the buffers are got updated by the disk thread */
			ret = get_from_diskreq(ret, req_no);
		}
	}
	return ret;
}
struct virtio_blk_req *createBuf(int type, unsigned char *user_buf, uint64_t sector, uint64_t data_len) {
	unsigned char *buf = 0;
	struct virtio_blk_req *req;
	int donot_copy = 0;

	if (user_buf >= pc_startaddr && user_buf < pc_endaddr) {
		donot_copy = 1;
		buf = mm_getFreePages(0, 0);
		req = (struct virtio_blk_req *) buf;
		ut_memset(buf, 0, sizeof(struct virtio_blk_req));
		req->user_data = user_buf;
	} else {
		buf = mm_getFreePages(0, 1);
		req = (struct virtio_blk_req *) buf;
		ut_memset(buf, 0, sizeof(struct virtio_blk_req));
		req->user_data = 0;
	}

	req->sector = sector;
	if (type == DISK_READ) {
		req->type = VIRTIO_BLK_T_IN;
	} else {
		req->type = VIRTIO_BLK_T_OUT;
		if (donot_copy == 0) {
			ut_memcpy(&req->data[0], user_buf, data_len);
		}
	}
	req->status = 0xff;
	req->len = data_len;
	return req;
}
#define MAX_REQS 10
int disk_io(int type, unsigned char *buf, int len, int offset, int read_ahead, jdiskdriver *driver) {
	struct virtio_blk_req *reqs[MAX_REQS], *tmp_req;
	int sector;
	int i, req_count, data_len, curr_len, max_reqs;
	int initial_skip, blks;
	unsigned long addr, flags;
	int qlen, ret;
	ret = 0;
	int curr_offset;


	sector = offset / driver->blk_size;
	initial_skip = offset - sector * driver->blk_size;

	data_len = len + initial_skip;
	curr_offset = offset - initial_skip;
	curr_len = data_len;
	max_reqs = 5;

	for (req_count = 0; req_count < max_reqs && curr_len > 0; req_count++) {
		int req_len = curr_len;
		if (req_len > VIRTIO_BLK_DATA_SIZE) {
			req_len = VIRTIO_BLK_DATA_SIZE;
		}
		if ((req_len + curr_offset) >= driver->disk_size) {
			req_len = driver->disk_size - curr_offset;
		}

		blks = req_len / driver->blk_size;
		if ((blks * driver->blk_size) != req_len) {
			req_len = (blks + 1) * driver->blk_size;
		}
		reqs[req_count] = createBuf(type, buf + (req_count * VIRTIO_BLK_DATA_SIZE), sector, req_len);
		curr_offset = curr_offset + VIRTIO_BLK_DATA_SIZE;
		curr_len = curr_len - VIRTIO_BLK_DATA_SIZE;
	}
	ret = diskio_submit_requests(reqs, req_count, driver, buf, len, initial_skip,
			read_ahead);

	return ret;
}
static int extract_reqs_from_devices(virtio_disk_jdriver *dev) {
	int ret = 0;
	int loop = 10;
	int i, qlen;
	struct struct_mbuf mbuf_list[64];
	unsigned char *req;
	int k;

	ret = dev->burst_recv(&mbuf_list[0], 63);
	for (k=0;  k < ret; k++) {
		loop--;
		req = mbuf_list[k].buf;
		qlen = mbuf_list[k].len;

		if (req == 0 ) {
			BUG();
		}
		for (i = 0; i < MAX_DISK_REQS; i++) {
			if (disk_reqs[i].buf == 0) {
				continue;
			}
			if (disk_reqs[i].buf == req) {
				disk_reqs[i].state = STATE_REQ_COMPLETED;
				break;
			}
		}
		if ( i==MAX_DISK_REQS ){
			BUG();
		}
	}
	return ret;
}
static struct  struct_mbuf disk_mbufs[MAX_DISK_REQS];
extern "C" {
 int g_conf_disk_bulkio  __attribute__ ((section ("confdata"))) =1;
}
int diskio_thread(void *arg1, void *arg2) {
	int i,j,k;
	int pending_req;
	int progress =0;
	int intr_disabled=0;
	int bufs_space=0;

	for (i=0; i<MAX_DISK_REQS; i++){
		disk_reqs[i].buf=0;
	}
	disk_thread_waitq = jnew_obj(wait_queue, "disk_thread_waitq", 0);
	while(1){
		if (progress == 0 ){
			if (intr_disabled == 1){
				//disk_thread_waitq->wait(1);
				sc_schedule(); /* give chance other threads to run */
			}else{
				disk_thread_waitq->wait(50);
			}
		}

		progress =0;
		pending_req=0;
		intr_disabled = 0;
		for (i=0; i<MAX_DISK_REQS; i++){
			if (disk_reqs[i].buf == 0) { continue; }
			pending_req++;
			if (disk_reqs[i].dev->interrupts_disabled == 1){
				intr_disabled = 1;
			}
			if (disk_reqs[i].state == 0) {
				bufs_space = disk_reqs[i].dev->MaxBufsSpace();
				if (g_conf_disk_bulkio == 0){
					bufs_space = 1;
				}
				k = 0;
				for (j = i; j < MAX_DISK_REQS && k < bufs_space; j++) {
					if (disk_reqs[j].buf != 0 && disk_reqs[j].state == 0 && disk_reqs[j].dev == disk_reqs[i].dev) {
						disk_mbufs[k].buf = (unsigned char *) disk_reqs[j].buf;
						disk_mbufs[k].len = disk_reqs[j].data_len;
						disk_reqs[j].state = STATE_REQ_QUEUED;
						progress++;
						k++;
					}
				}
				if (k > 0) {
					disk_reqs[i].dev->burst_send(&disk_mbufs[0], k);
				}
			}
			if (disk_reqs[i].state == STATE_REQ_QUEUED){
				progress = progress +extract_reqs_from_devices(disk_reqs[i].dev);
			}
			if (disk_reqs[i].state == STATE_REQ_COMPLETED){
				if ((disk_reqs[i].read_ahead == 1) && disk_reqs[i].state == STATE_REQ_COMPLETED){
					get_from_diskreq(i,disk_reqs[i].req_no);
					progress++;
				}else{
					disk_reqs[i].dev->waitq->wakeup();
				}
			}
		} /* end of for loop */
	}
}
