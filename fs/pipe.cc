/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/pipe.cc
*/

#include "file.hh"
static  int fs_send_to_pipe(long pipe_index, uint8_t *buf, int len);
static  int fs_recv_from_pipe(long pipe_index, uint8_t *buf, int len);
static  int fs_destroy_pipe(long pipe_index);

#define MAX_PIPES 100
struct {
#define MAX_PIPE_BUFS 20
	struct {
		uint8_t *data;
		int size;
		int start_offset;
	} bufs[MAX_PIPE_BUFS];
	int in_index, out_index;
	int count;
} pipes[MAX_PIPES];
static int init_pipe_done = 0;
static void init_pipes(){
	int i,j;
	for (i = 0; i < MAX_PIPES; i++) {
		pipes[i].in_index = 0;
		pipes[i].out_index = 0;
		pipes[i].count = 0;

		for (j = 0; j < MAX_PIPE_BUFS; j++) {
			pipes[i].bufs[j].data = 0;
			pipes[i].bufs[j].size = 0;
			pipes[i].bufs[j].start_offset = 0;
		}
	}
	init_pipe_done = 1;
	return ;
}
static int fs_create_pipe(struct pipe *wpipe, struct pipe *rpipe) {
	long i ;
	int j, found;
	int ret=-1;

	if (init_pipe_done == 0) {
		init_pipes();
	}
	if (wpipe==0 || rpipe==0) return ret;
	wpipe->file_type = OUT_PIPE_FILE;
	rpipe->file_type = IN_PIPE_FILE;
	mutexLock(g_inode_lock);
	for (i = 0; i < MAX_PIPES; i++) {
		if (pipes[i].count == 0) {
			found = 1;
			pipes[i].in_index = 0;
			pipes[i].out_index = 0;
			pipes[i].count = 2;
			wpipe->pipe_index = i;
			rpipe->pipe_index = i;
			wpipe->count.counter =1;
			rpipe->count.counter =1;
			for (j = 0; j < MAX_PIPE_BUFS; j++) {
				pipes[i].bufs[j].data = 0;
				pipes[i].bufs[j].size = 0;
				pipes[i].bufs[j].start_offset = 0;
			}
			ret = SYSCALL_SUCCESS;
			goto last;
		}
	}
last:
	mutexUnLock(g_inode_lock);

	return ret;
}

int pipe::init(int type) {
	file_type = type;
}
int pipe::read(unsigned long unused, unsigned char *buf, int len, int read_flags){
	int ret;
	ret = fs_recv_from_pipe(this->pipe_index,buf,len);
	update_stat_in(1,ret);
	return ret;
}
int pipe::write(unsigned long unused, unsigned char *buf, int len, int wr_flags){
	int ret;

	ret = fs_send_to_pipe(this->pipe_index,buf,len);
	update_stat_out(1,ret);
	return ret;
}
int pipe::close(){
	count.counter--;
	if (count.counter <= 0){
		fs_destroy_pipe(pipe_index);
		ut_free(this);
		return JSUCCESS;
	}
	return JFAIL;
}
int pipe::ioctl(unsigned long arg1,unsigned long arg2){
	BUG();
	return 0;
}


/*************************** End of pipe ******************************/

static int fs_recv_from_pipe(long pipe_index, uint8_t *buf, int len) {
	long i = pipe_index;
	int in, max_size;
	int ret = -1;

	if (i < 0 || i >= MAX_PIPES)
		return -1;
restart:
	mutexLock(g_inode_lock);
	in = pipes[i].in_index;
	if (pipes[i].bufs[in].data != 0) {
		max_size = pipes[i].bufs[in].size - pipes[i].bufs[in].start_offset;
		if (len < max_size)
			max_size = len;
		ut_memcpy(buf, pipes[i].bufs[in].data + pipes[i].bufs[in].start_offset,
				max_size);
		pipes[i].bufs[in].start_offset = pipes[i].bufs[in].start_offset
				+ max_size;
		if (pipes[i].bufs[in].start_offset == pipes[i].bufs[in].size) {
			mm_putFreePages((unsigned long) pipes[i].bufs[in].data, 0);
			pipes[i].bufs[in].data = 0;
			pipes[i].bufs[in].start_offset = 0;
			pipes[i].bufs[in].size = 0;
			pipes[i].in_index = (pipes[i].in_index + 1) % MAX_PIPE_BUFS;
		}
		ret = max_size;
	} else {
		ret=0;
		//ret = -EAGAIN;
		mutexUnLock(g_inode_lock);
		if (pipes[i].count > 1) { /* wait if there is more then thread operating pipe */
			sc_sleep(2);
			goto restart;
		}
		return ret;
	}
	mutexUnLock(g_inode_lock);

	//ut_printf(" recv from :%s: pipe-%d : %d\n",g_current_task->name, i,ret);
	return ret;
}
static int fs_send_to_pipe( long pipe_index, uint8_t *buf, int len) {
	int max_size = PAGE_SIZE ;
	int ret;
	int consumed_len=0;
	int left_len=len;

	long i = pipe_index;
	if (i < 0 || i >= MAX_PIPES)
		return -1;

restart:
	mutexLock(g_inode_lock);
	int out = pipes[i].out_index;
	if (pipes[i].bufs[out].data == 0) {
		pipes[i].bufs[out].data = (uint8_t *)alloc_page(0);
		pipes[i].bufs[out].size = 0;
		pipes[i].bufs[out].start_offset = 0;
	} else {
#if 0
		ret = -ENOSPC;
		goto last;
#else
		mutexUnLock(g_inode_lock);
		if (pipes[i].count > 1) { /* wait if there is more then one thread operating pipe */
			sc_sleep(2);
			goto restart;
		}
		return 0;
#endif
	}

	if (left_len < max_size)
		max_size = left_len;
	ut_memcpy(pipes[i].bufs[out].data, buf+consumed_len, max_size);
	pipes[i].bufs[out].size = max_size;
	pipes[i].bufs[out].start_offset = 0;
	pipes[i].out_index = (pipes[i].out_index + 1) % MAX_PIPE_BUFS;
	ret=max_size;
	consumed_len = consumed_len+max_size;
	left_len = left_len-max_size;

//last:
	mutexUnLock(g_inode_lock);
	if (left_len>0){
		goto restart;
	}
	//ut_printf(" sending in :%s: to pipe-%d : %d\n",g_current_task->name, i,ret);

	return consumed_len;
}
static int fs_destroy_pipe(long pipe_index) {
	int ret = -1;
	int j;
	long i =pipe_index;


	if (i < 0 || i >= MAX_PIPES)
		return -1;

	mutexLock(g_inode_lock);
	pipes[i].count--;
	ret = SYSCALL_SUCCESS;
	if ( pipes[i].count == 0 ) {
		for (j = 0; j < MAX_PIPE_BUFS; j++) {
			if (pipes[i].bufs[j].data != 0)
				mm_putFreePages((unsigned long) pipes[i].bufs[j].data, 0);
			pipes[i].bufs[j].data = 0;
			pipes[i].bufs[j].size = 0;
			pipes[i].bufs[j].start_offset = 0;
		}
	}
	mutexUnLock(g_inode_lock);

	return ret;
}
extern "C" {

unsigned long SYS_pipe(int *fds) {
	struct file *wfp, *rfp;
	int ret;
	class pipe *wpipe,*rpipe;

	SYSCALL_DEBUG("pipe  fds:%x \n", fds);
	if (fds == 0)
		return -1;

	wfp = fs_create_filep(&fds[1], 0);
	if (wfp != 0) {
		rfp = fs_create_filep(&fds[0], 0);
		if (rfp == 0) {
			fs_destroy_filep(fds[1]);
			//mm_slab_cache_free(g_slab_filep, wfp);
			return -1;
		}
	} else {
		return -1;
	}
#if 0
	wpipe = (struct pipe *)ut_calloc(sizeof(struct pipe));
	rpipe = (struct pipe *)ut_calloc(sizeof(struct pipe));
#endif
	wpipe = jnew_obj(pipe);
	rpipe = jnew_obj(pipe);

	wpipe->init(OUT_PIPE_FILE);
	rpipe->init(IN_PIPE_FILE);
	wfp->type = OUT_PIPE_FILE;
	wfp->vinode =wpipe;
	rfp->type = IN_PIPE_FILE;
	rfp->vinode =rpipe;
	ret = fs_create_pipe(wpipe, rpipe);
	if (ret != SYSCALL_SUCCESS) {
		fs_destroy_filep(fds[0]);
		fs_destroy_filep(fds[1]);
		fds[0] = -1;
		fds[1] = -1;
		ut_free(wpipe);
		ut_free(rpipe);
	}


	if (ret == SYSCALL_SUCCESS){
		SYSCALL_DEBUG("pipe  fds: fd1: %d fd2 :%d  \n", fds[0],fds[1]);
	}

	return ret;
}
}
