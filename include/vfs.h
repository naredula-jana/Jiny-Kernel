/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   include/vfs.h
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#ifndef __VFS_H__
#define __VFS_H__
#include "common.h"
#include "mm.h"
#include "task.h"

#define MAX_FILENAME 200
#define HOST_SHM_ADDR 0xd0000000
#define HOST_SHM_CTL_ADDR 0xd1000000

#define O_ACCMODE       00000003
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002
#ifndef O_CREAT
#define O_CREAT         00000100        /* not fcntl */
#endif
#ifndef O_EXCL
#define O_EXCL          00000200        /* not fcntl */
#endif
#ifndef O_NOCTTY
#define O_NOCTTY        00000400        /* not fcntl */
#endif
#ifndef O_TRUNC
#define O_TRUNC         00001000        /* not fcntl */
#endif
#ifndef O_APPEND
#define O_APPEND        00002000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK      00004000
#endif
#ifndef O_DSYNC
#define O_DSYNC         00010000        /* used to be O_SYNC, see below */
#endif
#ifndef FASYNC
#define FASYNC          00020000        /* fcntl, for BSD compatibility */
#endif
#ifndef O_DIRECT
#define O_DIRECT        00040000        /* direct disk access hint */
#endif
#ifndef O_LARGEFILE
#define O_LARGEFILE     00100000
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY     00200000        /* must be a directory */
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW      00400000        /* don't follow links */
#endif
#ifndef O_NOATIME
#define O_NOATIME       01000000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC       02000000        /* set close_on_exec */
#endif


#define SEEK_SET        0       /* seek relative to beginning of file */
#define SEEK_CUR        1       /* seek relative to current file position */
#define SEEK_END        2       /* seek relative to end of file */
#define SEEK_DATA       3       /* seek to the next data */
#define SEEK_HOLE       4       /* seek to the next hole */

#define F_DUPFD         0       /* dup */
#define F_GETFD         1       /*getfd flags */
#define F_SETFD         2       /*setfd for close on exec */
#define FD_CLOSEXEC      1

enum {
POSIX_FADV_DONTNEED=4
};

enum {  /* inode flags */
INODE_SHORTLIVED=0x1,
INODE_LONGLIVED=0x2,
INODE_EXECUTING=0x4
};
extern unsigned long g_hostShmLen;

struct file;
struct inode;

enum { /* Inode types*/
	NETWORK_FILE=2,

	OUT_FILE=3,  /* special in/out files */
	IN_FILE=4,
	OUT_PIPE_FILE=5,
	IN_PIPE_FILE=6,
	DEV_NULL_FILE=7,

	REGULAR_FILE=0x8000,
	DIRECTORY_FILE=0x4000,
	SYM_LINK_FILE=0xA000
};
enum {
	DEVICE_SERIAL=1,
	DEVICE_KEYBOARD=2,
	DEVICE_DISPLAY_VGI=3
};

struct file {
	unsigned char filename[MAX_FILENAME];
	int type;
	uint64_t offset;
	int flags;

	struct inode *inode;
	void *private_pipe;

};
struct fileStat {
	uint32_t mode;
	uint32_t atime,mtime;
	uint64_t st_size;
	uint64_t inode_no;
	uint32_t type;
	uint32_t blk_size;
};
#define PAGELIST_HASH_SIZE 40
#define get_pagelist_index(offset)  ((offset/PAGE_SIZE)%PAGELIST_HASH_SIZE)
struct inode {
	atomic_t count; /* usage count */
	int nrpages;	/* total pages */
	atomic_t stat_locked_pages;

	int flags; /* short leaved (MRU) or long leaved (LRU) */
	time_t mtime; /* last modified time */
	unsigned long fs_private;
	struct filesystem *vfs;

	int type;

	union {
		struct {
			unsigned long open_mode;
			char stat_insync;
			struct fileStat stat;
		}file;
		struct {
			int sock_type;
			unsigned long local_addr;
			unsigned short local_port;
		}socket;
	}u;

	int stat_out,stat_in,stat_err;
	long stat_last_offset;

	unsigned char filename[MAX_FILENAME];
	struct list_head page_list[PAGELIST_HASH_SIZE];
	struct list_head vma_list;	
	struct list_head inode_link;	
};



struct dirEntry { /* Do not change the entries , the size of struct is caluclated */
	unsigned long inode_no; /* Inode number */
	unsigned long next_offset; /* Offset to next dirent */
	unsigned short d_reclen; /* Length of this dirent */
	char filename[]; /* Filename (null-terminated) */
};

typedef struct fileStat fileStat_t;
struct filesystem {
	int (*open)(struct inode *inode, int flags, int mode);
	int (*lseek)(struct file *file,  unsigned long offset, int whence);
	long (*write)(struct inode *inode, uint64_t offset, unsigned char *buff, unsigned long len);
	long (*read)(struct inode *inode, uint64_t offset,  unsigned char *buff, unsigned long len);
	long (*readDir)(struct inode *inode, struct dirEntry *dir_ptr, unsigned long dir_max, int *offset);
	int (*remove)(struct inode *inode);
	int (*stat)(struct inode *inode, struct fileStat *stat);
	int (*close)(struct inode *inodep);
	int (*fdatasync)(struct inode *inodep);
	int (*setattr)(struct inode *inode, uint64_t size);//TODO : currently used for truncate, later need to expand
};

#define fd_to_file(fd) (fd >= 0 && g_current_task->mm->fs.total > fd) ? (g_current_task->mm->fs.filep[fd]) : ((struct file *)0)


#endif
