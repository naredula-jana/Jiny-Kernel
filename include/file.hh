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

void *operator new(int sz,const char *name);
void jfree_obj(unsigned long addr);
#define STRLEN(s) (sizeof(s)/sizeof(s[0]))
#define jnew_obj(x,y...) new (#x) x(y);

class jobject { /* All objects will be inherited from here */
public:
	int jobject_id; /* currently used only for debugging purpose */
	virtual void print_stats(unsigned char *arg1,unsigned char *arg2)=0;
};

#include "ipc.hh"

class vinode: public jobject {
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
	virtual void print_stats(unsigned char *arg1,unsigned char *arg2)=0;
};
#define MAX_SOCKET_QUEUE_LENGTH 2500
struct sock_queue_struct {
	wait_queue *waitq;
	int producer, consumer;
	struct {
		unsigned char *buf;
		unsigned int len; /* actual data length , not the buf lenegth, buf always constant length */
	} data[MAX_SOCKET_QUEUE_LENGTH];
	spinlock_t spin_lock; /* lock to protect while adding and revoing from queue */
	int stat_processed[MAX_CPUS];
	unsigned long queue_len;
	unsigned long error_full;
};
#define MAX_SOCKETS 100
class jdevice;


#define MAX_NETWORK_STACKS 5
enum {
	SOCK_IOCTL_BIND=1,
	SOCK_IOCTL_CONNECT=2,
	SOCK_IOCTL_WAITFORDATA=3,
	IOCTL_FILE_UNLINK=4
};
enum {
	GENERIC_IOCTL_PEEK_DATA=100  /* check for the presence of data */
};
enum {
	NETDEV_IOCTL_GETMAC=1,
	NETDEV_IOCTL_FLUSH_SENDBUF=2
};
class socket;
typedef struct sock_list_type{
	class socket *list[MAX_SOCKETS];
	int size;
}sock_list_t;
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
	void print_stats(unsigned char *arg1,unsigned char *arg2);

	void init_socket(int type);
	int add_to_queue(unsigned char *buf, int len);
	int remove_from_queue(unsigned char **buf,  int *len);

/* static/class members */
	static vinode *create_new(int type);
	static int delete_sock(socket *sock);
	static int attach_rawpkt(unsigned char *c, unsigned int len);

	static sock_list_t udp_list,tcp_listner_list,tcp_connected_list;
	static network_stack *net_stack_list[MAX_NETWORK_STACKS];
	static jdevice *net_dev;
	static int stat_raw_drop;
	static int stat_raw_default;
	static int stat_raw_attached;
	static class socket *default_socket;
	static void init_socket_layer();
	static void print_all_stats();
	static int default_pkt_thread(void *arg1, void *arg2);
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

	fs_inode(uint8_t *filename, unsigned long mode, struct filesystem *vfs);

	int read(unsigned long offset, unsigned char *data, int len, int flags);
	int write(unsigned long offset, unsigned char *data, int len, int flags);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);
	void print_stats(unsigned char *arg1,unsigned char *arg2);
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
	void print_stats(unsigned char *arg1,unsigned char *arg2);
};

/*******************************************************************************/
#if 1
class filesystem {

public:
	unsigned long stat_byte_reads,stat_read_req,stat_read_errors;
	unsigned long stat_byte_writes,stat_write_req,stat_write_errors;
	unsigned long device_size;
	unsigned long block_size;
	unsigned long filesystem_size; /* file system size will be less then disk size */
	//unsigned long free_space_size;

	virtual int open(fs_inode *inode, int flags, int mode)=0;
	virtual int lseek(struct file *file,  unsigned long offset, int whence)=0;
	virtual long write(fs_inode *inode, uint64_t offset, unsigned char *buff, unsigned long len)=0;
	virtual long read(fs_inode *inode, uint64_t offset,  unsigned char *buff, unsigned long len)=0;
	virtual long readDir(fs_inode *inode, struct dirEntry *dir_ptr, unsigned long dir_max, int *offset)=0;
	virtual int remove(fs_inode *inode)=0;
	virtual int stat(fs_inode *inode, struct fileStat *stat)=0;
	virtual int close(fs_inode *inodep)=0;
	virtual int fdatasync(fs_inode *inodep)=0;
	virtual int setattr(fs_inode *inode, uint64_t size)=0;//TODO : currently used for truncate, later need to expand
	virtual int unmount()=0;
	virtual void set_mount_pnt(unsigned char *mnt_pnt)=0;
	virtual void print_stat()=0;
};

unsigned long fs_registerFileSystem(filesystem *fs, unsigned char *mnt_pnt);
#endif
#define fd_to_file(fd) (fd >= 0 && g_current_task->fs->total > fd) ? (g_current_task->fs->filep[fd]) : ((struct file *)0)

#endif

