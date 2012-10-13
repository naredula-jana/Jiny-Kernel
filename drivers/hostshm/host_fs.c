/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/hostshm/host_fs.c
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#define DEBUG_ENABLE 1
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
#include "interface.h"
#include "../util/host_fs/filecache_schema.h"
#include "shm_queue.h"
#define OFFSET_ALIGN(x) ((x/PC_PAGESIZE)*PC_PAGESIZE) /* the following 2 need to be removed */


struct client_queue *cq;
static int HfsClientInit() {
	static int init = 0;
	uint32_t msg_size;
	int ret;
	unsigned char version[200];
	unsigned long addr,i;


	return 1;
}

static int Hfs_open(uint32_t fid, unsigned char *filename, int flags, int arg_mode) {
	unsigned long addr;
	uint8_t mode_b;
	uint32_t perm;
	int i,ret=-1;



	if (flags & O_RDONLY)
		mode_b = 0;
	else if (flags & O_WRONLY)
		mode_b = 1;
	else if (flags & O_RDWR)
		mode_b = 2;

	return ret;
}


//TODO: handling locking in entire Hfs using new mutex calls
static uint32_t Hfs_walk(unsigned char *filename, int flags, unsigned char **create_filename) {

}



static uint32_t Hfs_write(uint32_t fid, uint64_t offset, unsigned char *data, uint32_t data_len) {
	unsigned long addr;
	int ret;
	uint32_t write_len=0;
	unsigned char *rd;



	return write_len;
}

static uint32_t Hfs_remove(uint32_t fid) {
	unsigned long addr;
	int i,ret=0;



	return ret;
}
static uint32_t Hfs_close(uint32_t fid) {
	unsigned long addr;
	int i, ret = 0;


	return ret;
}
static uint32_t Hfs_stat(uint32_t fid, struct fileStat *stat) {
	unsigned long addr;
	int i,ret=0;
	uint64_t dummyq;
	uint32_t dummyd;
	uint16_t dummyw;
	uint8_t dummyb;


	return ret;
}
static void update_size(struct inode *inode,uint64_t offset, int len ) {
	if (inode !=0 && len > 0) {
		if (inode->file_size < (offset+len))
			inode->file_size = offset+len;
	}
	return ;
}
static int peer_pos=-1;
static void generate_interrupt_to_peer() {
	int *p,k;
	if (peer_pos == -1)
		return;
	p = HOST_SHM_CTL_ADDR;
	p = p + 3;
	k=peer_pos << 16;
	*p = k;
	ut_printf("Generating inteerupt nnio addr:%x val:%x peer_pos:%x\n", p,k, peer_pos);
}

struct filesystem Hfs_fs;
extern void *Hfs_dev;
extern queue_t g_hfs_waitqueue;
/* This is central switch where the call from vfs routed to the Hfs functions */
static int HfsRequest(unsigned char type, struct inode *inode, uint64_t offset,
		unsigned char *data, int data_len, int flags, int mode) {
	uint32_t fid;
	unsigned char *createFilename;
	int ret = -1;
	Request_t *request;
	if (inode == 0)
		return ret;
	struct buf_desc buf;

	buf.len = data_len;
	client_get_buf(cq, &buf, SEND);
	request = buf.buf;
	request->type = type;
	request->file_offset = offset;
	request->request_len = data_len;
	ut_strcpy(request->filename, inode->filename);
	client_put_buf(&buf);

	generate_interrupt_to_peer();

	while (client_get_buf(cq, &buf, RECV) == 0) {
		sc_wait(&g_hfs_waitqueue, 5);
	}
	request = buf.buf;
	peer_pos = request->guestos_pos;
//	mutexLock(client.lock);
	if (type == REQUEST_OPEN) {
		if (request->response == RESPONSE_DONE) {
			ret = request->response_len;
		}else{
			ret = -1;
		}
	} else if (type == REQUEST_READ) {
		if (request->response == RESPONSE_DONE) {
			ret = data_len;
			if (ret > request->response_len)
				ret = request->response_len;
			if (ret > 0)
			ut_memcpy(data, request->data, ret);
			update_size(inode, offset, ret);
		}
	} else if (type == REQUEST_WRITE) {
		//	update_size(inode,offset,ret);
	}
	client_put_buf(&buf);
	last:
	//   mutexUnLock(client.lock);
	return ret;
}

static int HfsOpen(struct inode *inodep, int flags, int mode) {
	HfsClientInit();
	return HfsRequest(REQUEST_OPEN, inodep, 0, 0, 0, flags, mode);
}

static int HfsLseek(struct file *filep, unsigned long offset, int whence) {
	filep->offset = offset;
	return 1;
}

static int HfsFdatasync(struct inode *inodep) {

	return 1;
}
static int HfsWrite(struct inode *inodep, uint64_t offset, unsigned char *data, unsigned long  data_len) {
	HfsClientInit();
    return  HfsRequest(REQUEST_WRITE, inodep, offset, data, data_len, 0, 0);
}

static int HfsRead(struct inode *inodep, uint64_t offset, unsigned char *data, unsigned long  data_len) {
	HfsClientInit();
    return  HfsRequest(REQUEST_READ, inodep, offset, data, data_len, 0, 0);
}

static int HfsRemove(struct inode *inodep) {
	HfsClientInit();
    return  HfsRequest(REQUEST_REMOVE, inodep, 0, 0, 0, 0, 0);
}

static int HfsStat(struct inode *inodep, struct fileStat *statp) {
	HfsClientInit();
    return  HfsRequest(REQUEST_STAT, inodep, 0, statp, 0, 0, 0);
}

static int HfsClose(struct inode *inodep) {
	HfsClientInit();
    return  HfsRequest(REQUEST_CLOSE, inodep, 0, 0, 0, 0, 0);
}

int init_HostFs() {
	Hfs_fs.open = HfsOpen;
	Hfs_fs.read = HfsRead;
	Hfs_fs.close = HfsClose;
	Hfs_fs.write = HfsWrite;
	Hfs_fs.remove = HfsRemove;
	Hfs_fs.stat = HfsStat;
	Hfs_fs.fdatasync = HfsFdatasync; //TODO
	Hfs_fs.lseek = HfsLseek; //TODO

	fs_registerFileSystem(&Hfs_fs);
	cq=shm_client_init();
	return 1;
}


