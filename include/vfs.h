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

enum {
	DEVICE_SERIAL1=1,
	DEVICE_SERIAL2=2,
	DEVICE_KEYBOARD=3,
	DEVICE_DISPLAY_VGI=4
};

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
#define F_GETFL         3       /* get file->f_flags */
#define F_SETFL         4       /* set file->f_flags */

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

/* device can support multiple types*/
enum { /* Inode types*/
	NETWORK_FILE=0x1,

	OUT_FILE=0x2,  /* special in/out files */
	IN_FILE=0x4,
	OUT_PIPE_FILE=0x8,
	IN_PIPE_FILE=0x10,
	DEV_NULL_FILE=0x20,
	EVENT_POLL_FILE=0x40,
	SOCKETPAIR_FILE=0x80,

	REGULAR_FILE=0x8000,
	DIRECTORY_FILE=0x4000,
	SYM_LINK_FILE=0xA000
};


struct file {
	unsigned char filename[MAX_FILENAME];
	int type;
	uint64_t offset;
	int flags;
	void *vinode;
};
struct fileStat {
	uint32_t mode;
	uint32_t atime,mtime;
	uint64_t st_size;
	uint64_t inode_no;
	uint32_t type;
	uint32_t blk_size;
};
#define PAGELIST_HASH_SIZE 512
#define get_pagelist_index(offset)  ((offset/PAGE_SIZE)%PAGELIST_HASH_SIZE)

struct dirEntry { /* Do not change the entries , the size of struct is caluclated */
	unsigned long inode_no; /* Inode number */
	unsigned long next_offset; /* Offset to next dirent */
	unsigned short d_reclen; /* Length of this dirent */
	char filename[]; /* Filename (null-terminated) */
};
#if 1
typedef struct fileStat fileStat_t;

#endif
#define fd_to_file(fd) (fd >= 0 && g_current_task->fs->total > fd) ? (g_current_task->fs->filep[fd]) : ((struct file *)0)
int fs_data_sync(int num_pages);

#endif
