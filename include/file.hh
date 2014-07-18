/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/file.h
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*        file--- vinode->fs_inode->(file,dir)
         file--- vinode->socket
         file--- vinode->pipe
         file--- vinode->device
*/
#ifndef _JINYKERNEL_FILE_HH
#define _JINYKERNEL_FILE_HH
extern "C" {
#include "common.h"
#include "mm.h"
#include "interface.h"
#include "network_stack.hh"
extern void *g_inode_lock; /* protects inode_list */
extern kmem_cache_t *g_slab_filep;
#define ENOSPC          28      /* No space left on device */
#define EAGAIN          11      /* Try again */
#define CTRL_C 3
#define CTRL_D 4

extern int fs_dup_pipe(struct file *fp);
struct file *fs_create_filep(int *fd, struct file *in_fp);
int fs_destroy_filep(int fd);
extern void ut_putchar_vga(unsigned char c, int device);
}

#define PIPE_IMPL 1
class vinode {
public:
	atomic_t count; /* usage count */
	unsigned char filename[MAX_FILENAME];
	int file_type; /* type of file */

	unsigned long stat_out,stat_in,stat_err;
	unsigned long stat_in_bytes,stat_out_bytes;

	void update_stat_in(int in_req,int in_byte);
	void update_stat_out(int out_req,int out_byte);

	/* TODO : preserve the order, this is C++ fix  */
	virtual int read(unsigned long offset, unsigned char *data, int len, int flags)=0;
	virtual int write(unsigned long offset, unsigned char *data, int len, int flags)=0;
	virtual int close()=0;
	virtual int ioctl(unsigned long arg1,unsigned long arg2)=0;
};
#define MAX_SOCKET_QUEUE_LENGTH 1500
struct sock_queue_struct {
	wait_queue_t waitq;
	int producer, consumer;
	struct {
		unsigned char *buf;
		unsigned int len; /* actual data length , not the buf lenegth, buf always constant length */
	} data[MAX_SOCKET_QUEUE_LENGTH];
	spinlock_t spin_lock; /* lock to protect while adding and revoing from queue */
	int stat_processed[MAX_CPUS];
	int queue_len;
	unsigned long error_full;
};
#define MAX_SOCKETS 100
class jdevice;


#define MAX_NETWORK_STACKS 5
enum {
	SOCK_IOCTL_BIND=1,
	SOCK_IOCTL_CONNECT=2,
	SOCK_IOCTL_WAITFORDATA=3
};
class socket: public vinode {
public:
	class network_connection network_conn;
	network_stack *net_stack;
	struct sock_queue_struct queue;
	unsigned char *peeked_msg;
	int peeked_msg_len;

	int read(unsigned long offset, unsigned char *data, int len, int flags);
	int write(unsigned long offset, unsigned char *data, int len, int flags);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);
	int peek();

	void init_socket(int type);
	int add_to_queue(unsigned char *buf, int len);
	int remove_from_queue(unsigned char **buf,  int *len);

/* static/class members */
	static vinode *create_new(int type);
	static int delete_sock(socket *sock);
	static int attach_rawpkt(unsigned char *c, unsigned int len, unsigned char **replace_buf);

	static class socket *list[MAX_SOCKETS];
	static int list_size;
	static network_stack *net_stack_list[MAX_NETWORK_STACKS];
	static jdevice *net_dev;
	static int stat_raw_drop;
	static int stat_raw_default;
	static int stat_raw_attached;
	static class socket *default_socket;
	static void init_socket_layer();
	static void print_stats();
	static void default_pkt_thread(void *arg1, void *arg2);
};
typedef struct hard_link hard_link_t;
typedef struct hard_link{
		unsigned char filename[MAX_FILENAME];
		atomic_t count; /* usage count */
		hard_link_t *next;
	}hard_link_t;

class fs_inode :public vinode {

public:
	atomic_t stat_locked_pages;
	time_t mtime; /* last modified time */
	unsigned long fs_private;
	struct filesystem *vfs;
	unsigned long open_mode;
	long stat_last_offset;

	struct page *fs_genericRead( unsigned long offset);

	int nrpages;	/* total pages */
	struct list_head vma_list;
	struct list_head inode_link;
	struct list_head page_list[PAGELIST_HASH_SIZE];

	int flags; /* short leaved (MRU) or long leaved (LRU) */

	struct fileStat fileStat;
	char fileStat_insync;
	hard_link_t *hard_links;

	int init(uint8_t *filename, unsigned long mode, struct filesystem *vfs);

	int read(unsigned long offset, unsigned char *data, int len, int flags);
	int write(unsigned long offset, unsigned char *data, int len, int flags);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);
	static kmem_cache_t *slab_objects;
};

class pipe :public vinode {

public:
	long pipe_index;
	int init(int type);
	int read(unsigned long unsued, unsigned char *data, int len, int flags);
	int write(unsigned long unused, unsigned char *data, int len, int flags);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);
};

/*******************************************************************************/



#define fd_to_file(fd) (fd >= 0 && g_current_task->fs->total > fd) ? (g_current_task->fs->filep[fd]) : ((struct file *)0)

#endif

