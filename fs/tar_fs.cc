/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/tar_fs.cc
*
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*/

//#define TEST_TAR_FS 1


#if TEST_TAR_FS
int fd;
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#define ut_printf printf
#define ut_log printf
#define ut_memcpy memcpy
#define ut_strcmp strcmp
#define ut_strcpy strcpy
#define ut_strstr strstr
#define ut_strlen strlen
#define uint8_t char
#define DEBUG(x...) do { \
} while (0)
#else

#include "file.hh"
#include "jdevice.h"
extern "C"{
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
#include "interface.h"

#if 0
#define DEBUG ut_printf
#else
#define DEBUG(x...) do { \
} while (0)
#endif

}
#endif


#define RECORDSIZE	512
#define NAMSIZ		100
#define TUNMLEN		32
#define TGNMLEN		32
#define SPARSE_IN_HDR	4
#define ISODIGIT(c) (c>='0' && c<='7')
#define ISSPACE(c) (c==' ')

/* The linkflag defines the type of file.  */
#define LF_OLDNORMAL	'\0'	/* normal disk file, Unix compat */
#define LF_NORMAL	'0'	/* normal disk file */
#define LF_LINK		'1'	/* link to previously dumped file */
#define LF_SYMLINK	'2'	/* symbolic link */
#define LF_CHR		'3'	/* character special file */
#define LF_BLK		'4'	/* block special file */
#define LF_DIR		'5'	/* directory */
#define LF_FIFO		'6'	/* FIFO special file */
#define LF_CONTIG	'7'	/* contiguous file */
/* Further link types may be defined later.  */
struct sparse {
	char offset[12];
	char numbytes[12];
};
struct header {
	uint8_t name[NAMSIZ];
	uint8_t mode[8];
	uint8_t uid[8];
	uint8_t gid[8];
	uint8_t size[12];
	uint8_t mtime[12];
	uint8_t chksum[8];
	uint8_t linkflag;
	uint8_t arch_linkname[NAMSIZ];
	uint8_t magic[8];
	uint8_t uname[TUNMLEN];
	uint8_t gname[TGNMLEN];
	uint8_t devmajor[8];
	uint8_t devminor[8];

	/* The following fields were added for GNU and are not standard.  */
	uint8_t atime[12];
	uint8_t ctime[12];
	uint8_t offset[12];
	uint8_t longnames[4];

	/* Some compilers would insert the pad themselves, so pad was
	 once autoconfigured.  It is simpler to always insert it!  */
	uint8_t pad;
	struct sparse sp[SPARSE_IN_HDR];uint8_t isextended;uint8_t realsize[12]; /* true size of the sparse file */
} header;

#define MAX_TAR_FILES 1024
struct tar_file {
	uint8_t name[NAMSIZ];
	uint8_t arch_linkname[NAMSIZ];
	unsigned long length,mtime,ctime,atime;
	unsigned long mode;
	unsigned long device_offset;
	uint8_t type;
	long parent;
	int link_id;
} tar_file;

#if TEST_TAR_FS
class tar_fs {
#else
class tar_fs :public filesystem {
#endif
    unsigned char mnt_point[200];
    int mnt_len;

	int get_file(unsigned char *filename);
	int read_from_device(int offset, unsigned char *buf);
	int scan_device_forfiles();
public:
	struct tar_file files[MAX_TAR_FILES];
	int file_count;
	jdriver *driver;

	int init(jdriver *driver_arg);
	void list_files();
	int get_dir_entry(unsigned char *dirname, int next);
	int read_file_contents(unsigned char * filename, unsigned char *buf,
			int len, int offset);
#if TEST_TAR_FS
#else
	 int open(fs_inode *inode, int flags, int mode);
	 int lseek(struct file *file,  unsigned long offset, int whence);
	 long write(fs_inode *inode, uint64_t offset, unsigned char *buff, unsigned long len);
	 long read(fs_inode *inode, uint64_t offset,  unsigned char *buff, unsigned long len);
	 long readDir(fs_inode *inode, struct dirEntry *dir_ptr, unsigned long dir_max, int *offset);
	 int remove(fs_inode *inode);
	 int stat(fs_inode *inode, struct fileStat *stat);
	 int close(fs_inode *inodep);
	 int fdatasync(fs_inode *inodep);
	 int setattr(fs_inode *inode, uint64_t size);//TODO : currently used for truncate, later need to expand
	 int unmount();
	 void set_mount_pnt(unsigned char *mnt_pnt);
#endif
};
#define BLK_SIZE 512
static long from_oct(int digs, char *where) {
	register long value;

	while (ISSPACE(*where)) { /* skip spaces */
		where++;
		if (--digs <= 0)
			return -1; /* all blank field */
	}

	value = 0;
	while (digs > 0 && ISODIGIT(*where)) {
		/* Scan til nonoctal.  */
		value = (value << 3) | (*where++ - '0');
		--digs;
	}

	if (digs > 0 && *where && !ISSPACE(*where))
		return -1; /* ended on non-space/nul */

	return value;
}
int tar_fs::scan_device_forfiles() {
	int i,j;
	int offset = 0;
	int completed = 0;
	long file_size;
	int blks;
	int len;
	struct header file;

	ut_log(" Tar fs Scan started ..\n");
	ut_strcpy(files[0].name,".");
	files[0].length = 0;
	files[0].device_offset = 0;
	files[0].type = LF_DIR;
	file_count = 1;
	while (completed == 0) {
		if (read_from_device(offset, (unsigned char *) &file) == 0) {
			completed = 1;
			break;
		}
		if (file.name[0] == 0) {
			completed = 1;
			break;
		}
		file_size = from_oct(12, file.size);
		DEBUG("%d blk:%d filename :%s  offset:%d  SIZE :%s:%d \n", i,
				(offset / BLK_SIZE), file.name, offset, file.size, file_size);
		blks = file_size / BLK_SIZE;
		if ((blks * 512) < file_size)
			blks++;

		ut_memcpy(files[file_count].name, file.name, NAMSIZ);
		ut_memcpy(files[file_count].arch_linkname, file.arch_linkname, NAMSIZ);
		files[file_count].length = file_size;
		files[file_count].device_offset = offset + BLK_SIZE;
		files[file_count].type = file.linkflag;
		files[file_count].mtime = from_oct(12, file.mtime);
		files[file_count].atime = from_oct(12, file.atime);
		files[file_count].ctime = from_oct(12, file.ctime);
		files[file_count].mode = from_oct(8, file.mode);
		files[file_count].parent = -1;
		files[file_count].link_id = -1;
		/* remove "/" at the end of every directory */
		if (files[file_count].type == LF_DIR) {
			len = ut_strlen(files[file_count].name);
			if (len > 1 && files[file_count].name[len - 1] == '/') {
				files[file_count].name[len - 1] = 0;
			}
		}
#if 1
		if (files[file_count].type == LF_LINK) {
			for (i=0; i<file_count; i++){
				if (ut_strcmp(file.arch_linkname,files[i].name) == 0){
		//		files[file_count].link_id = i;
					files[file_count].device_offset = files[i].device_offset;
					files[file_count].length = files[i].length;
					break;
				}
			}
		}
#endif
		file_count++;
		offset = offset + BLK_SIZE + (blks * BLK_SIZE);
		file.name[0] = 0;
	}
	//list_files();
	for (i=1; i<file_count; i++){
		if (files[i].parent == -1 && ut_strstr(&files[i].name[len+1],"/")==0){
			files[i].parent = 0;
		}
		if (files[i].type == LF_DIR){
					len = ut_strlen(files[i].name);
					if (len>1 && files[i].name[len-1]=='/'){
						files[i].name[len-1]=0;
					}
		}
		if (files[i].type != LF_DIR) continue;
		len = ut_strlen(files[i].name);
		for (j=0; j<file_count; j++){
			if (i==j || files[j].parent != -1) continue;
			if (ut_strstr(files[j].name,(const char *)files[i].name) == files[j].name  && ut_strstr(&files[j].name[len+1],"/")==0 ){
				files[j].parent = i;
			}
		}
	}

//	list_files();
	ut_log(" Tar fs Scan Ended\n");
	return file_count;
}
void tar_fs::list_files() {
	int i;

	ut_printf(" toal files : %d\n", file_count);
	for (i = 0; i < file_count; i++) {
		ut_printf("%x(%d) blk:%d length:%d :%s--%s  parent:%x type:%x link :%s: \n", i,i,
				files[i].device_offset / BLK_SIZE, files[i].length,
				mnt_point,files[i].name,files[i].parent,files[i].type,files[i].arch_linkname);
	}
	return;
}
int tar_fs::init(jdriver *driver_arg) {
	file_count = 0;

#if TEST_TAR_FS
	fd = open((const char *) "./test.tar", 0);
#else
	if (driver_arg == 0) return JFAIL;
#endif

	driver = driver_arg;
	file_count = -1;
	return JSUCCESS;
}
int tar_fs::read_from_device(int offset, unsigned char *buf) {
	int ret;

#if TEST_TAR_FS
	lseek(fd, offset, SEEK_SET);
	ret = read(fd, buf, sizeof(struct header));
#else
	if (file_count == -1){
		scan_device_forfiles();
	}
	//ret = disk_drivers[0]->read(buf,sizeof(struct header),offset);
	ret = driver->read(buf,sizeof(struct header),offset);
#endif
	if (ret == sizeof(struct header))
		return 1;
	return 0;
}
static void regularised_filename(unsigned char *filename,unsigned char *new_filename){
	int i,j,len;

	j=0;
	for (i=0; i<filename[i]!=0 && i<NAMSIZ; i++){
		if (filename[i]=='/' && filename[i+1]=='/' ){
			i++; /* eat the next char */
		}
		new_filename[j]=filename[i];
		j++;
	}
	new_filename[j]=0;
	len = ut_strlen((const char *)new_filename);
	if (len >1 && new_filename[len-1]=='/'){
		//new_filename[len-1]=0;
	}
	return;
}
int tar_fs::get_file(unsigned char *filename_arg) {
	int i,len;
	unsigned char *p;
	unsigned char filename[NAMSIZ];
	int prefix_len;
	regularised_filename(filename_arg,filename);

	if (file_count == -1){
		scan_device_forfiles();
	}

	/* verify if the name is as initial mnt point */
	p=(unsigned char *)ut_strstr((const char *)filename,(const char *)mnt_point);
	if (p!=filename) return -1;

	len = ut_strlen((const char *)filename);
	if (len > 1 && filename[len-1]=='.' && filename[len-2]=='/'){
		filename[len-2]=0;
		len =len-2;
	}

	if (len == mnt_len){
		return 0;
	}
	prefix_len=mnt_len;
	if (filename[mnt_len]=='/'){
		prefix_len=mnt_len+1;
	}
	for (i = 0; i < file_count; i++) {
		if (ut_strcmp((const char *) &filename[prefix_len], (const char *) files[i].name)== 0) {
			return i;
		}
	}
	ut_printf(" ERROR:  tarfs cannot find the file  :%s: \n",filename);
	return -1;
}

int tar_fs::get_dir_entry(unsigned char *dirname_arg, int next) {
	int i,len,dir_len,prefix_len;
	long parent=-1;
	unsigned char dirname[NAMSIZ];
	unsigned char *p;

	regularised_filename(dirname_arg,dirname);

	/* verify if the name is as initial mnt point */
	p=(unsigned char *)ut_strstr((const char *)dirname,(const char *)mnt_point);
	if (p!=dirname) return -1;

	/* find the prefix length */
	dir_len = ut_strlen((const char *)dirname);

	if (dirname[mnt_len]=='\0' || (dirname[mnt_len]=='.' && (dir_len==(mnt_len+1))) ){
		parent = 0;
	}

	prefix_len=mnt_len;
	if (dirname[mnt_len]=='/'){
		prefix_len=mnt_len+1;
	}
	/* find the dir entry */
	for (i = 0; i < file_count; i++) {
		if (files[i].type != LF_DIR) continue;
		if (ut_strcmp((const char *) files[i].name, (const char *) &dirname[prefix_len]) == 0) {
			parent = i;
			break;
		}
	}
	if (parent ==-1) return -2;
	/* find the childs of dir entry */
	for (i = 0; i < file_count; i++) {
		if (files[i].parent == parent) {
			if (i > next) {
				return i;
			}
		}
	}

	return -3;
}

int tar_fs::read_file_contents(unsigned char *filename, unsigned char *buf, int len_arg, int offset) {
	int ret_len=0;
	int tmp_ret;
	int i = get_file(filename);

	if (i == -1)
		return 0;
	if (files[i].length <= offset)
		return -1;
	if (files[i].length <= (offset + len_arg))
		len_arg = files[i].length - offset;
#if TEST_TAR_FS
	lseek(fd, files[i].device_offset + offset, SEEK_SET);
	ret_len = read(fd, buf, len_arg);
#else
	tmp_ret=1;
	while (tmp_ret > 0 && (len_arg-ret_len)>0){
		tmp_ret= disk_drivers[0]->read(buf+ret_len,(len_arg-ret_len),files[i].device_offset+offset+ret_len);
		//ut_printf(" tmp read from tarfs read: %d tmp_ret:%d offset:%d\n",ret_len,tmp_ret,offset);
		if (tmp_ret > 0){
			ret_len = ret_len + tmp_ret;
		}else{
			break;
		}
	}
#endif
	return ret_len;
}

#if TEST_TAR_FS
main() {
	int ret;
	unsigned char buf[5000];
	int next;

	tar_fs fs;
	fs.init((unsigned char*)"/jiny/");
	fs.list_files();
	ret = fs.read_file_contents((unsigned char *)"/aaa/a.c", buf, 4096, 0);
	buf[ret]=0;
	ut_printf(" read ret : %d :%s:\n",ret,buf);

	while(1){
		next = fs.get_dir_entry((unsigned char *)"/jiny/aaa/",next);
		if (next <= 0){ break; }
		ut_printf("  child of direcotry :%s:\n",fs.files[next].name);
	}
}
#else
/* list of virtual functions */
long tar_fs::read(fs_inode *inodep, uint64_t offset, unsigned char *buff, unsigned long len_arg) {
	int ret_len=0;

	DEBUG(" inside the tarfs read: offset:%d len_arg:%d  filename:%s:\n", offset, len_arg,inodep->filename);
	ret_len = read_file_contents(inodep->filename, buff, len_arg,  offset);

	DEBUG(" read from tarfs read: %d buff:%x \n", ret_len,buff);
	//BRK;
	return ret_len;

}
static int get_filename_start(unsigned char *name){
	int i;
	int ret=0;
	for (i=0; name[i]!=0; i++){
		if (name[i]=='/'){
			ret=i+1;
		}
	}
	return ret;
}
long tar_fs::readDir(fs_inode *inodep, struct dirEntry *dir_p, unsigned long dir_max, int *offset) {
#if 1
	int len, i, d,s;
	unsigned char *p;
	int next=-3; /* -3 for . -2 for .. */
	int total_len=0;
	int self;

	DEBUG(" INSIDE... the tarfs READDIR :%s: %d:\n", inodep->filename,*offset);
	if (*offset > 0)
		return 0;
	self = get_file(inodep->filename);
	if (self < 0){
		return total_len;
	}
	p=(unsigned char *)dir_p;
	for (i = 0; (total_len<dir_max); i++) {
		if (next >= -1){
			d = get_dir_entry(inodep->filename,next);
			next = d;
			if (d < 0) break;
			dir_p->inode_no = files[d].device_offset;
			s = get_filename_start(files[d].name);
			ut_sprintf(dir_p->filename,"%s",(const unsigned char *)&files[d].name[s]);
		}else{
			if (next ==-3){
				ut_strcpy(dir_p->filename,".");
				dir_p->inode_no = files[self].device_offset;
			}else if (next ==-2){
				int parent=files[self].parent;
				ut_strcpy(dir_p->filename,"..");
				if (parent > -1){
					dir_p->inode_no = files[parent].device_offset;
				}else{
					dir_p->inode_no = files[self].device_offset;
				}
			}
			next++;
		}

		len = 2 * (sizeof(unsigned long)) + sizeof(unsigned short) + ut_strlen(dir_p->filename) + 2;

		DEBUG("%d :inode:%d   name :%s:  total_len:%d len:%d offset:%d\n",d,dir_p->inode_no,dir_p->filename,total_len,len,*offset);
		if ( offset && i< *offset) continue;
		dir_p->d_reclen = (len / 8) * 8;
		if ((dir_p->d_reclen) < len)
			dir_p->d_reclen = dir_p->d_reclen + 8;
		p = p + dir_p->d_reclen;
		total_len = total_len + dir_p->d_reclen;
		dir_p->next_offset = p;
		dir_p=(struct dirEntry  *)p;
	}
	if (offset){
		*offset=i;
	}
	return total_len;
#endif
}
int tar_fs::lseek(struct file *filep, unsigned long offset, int whence) {
	filep->offset = offset;
	return JSUCCESS;
}
long tar_fs::write(fs_inode *inodep, uint64_t offset, unsigned char *buff, unsigned long len) {

	ut_printf("ERROR: TAR FS write currently not supported\n");
	return 0;
}
int tar_fs::open(fs_inode *inodep, int flags, int mode) {
	int i = get_file(inodep->filename);

	if (i >= 0) {
		DEBUG(" tarfs open success: %s\n", inodep->filename);
		return JSUCCESS;
	} else {
		ut_printf("ERROR tarfs open Fail: %s\n", inodep->filename);
		return JFAIL;
	}
}
int tar_fs::close(fs_inode *inode) {
	return JSUCCESS;
}
int tar_fs::remove(fs_inode *inode) {
	return 0;
}
int tar_fs::fdatasync(fs_inode *inode) {
	return JSUCCESS;
}
int tar_fs::setattr(fs_inode *inode, uint64_t size) { //TODO : currently used for truncate, later need to expand
	return 0;
}
int tar_fs::unmount() {
	return 0;
}
int tar_fs::stat(fs_inode *inodep, struct fileStat *stat) {
	struct fileStat fs;
	int ret = JFAIL;
	int j;

	int i = get_file(inodep->filename);
	if (i>=0) {
		fs.st_size = files[i].length;
		fs.inode_no = files[i].device_offset; /* this acts as unique id for a regular file */
		fs.atime =files[i].atime;
		fs.mtime =files[i].mtime;
		//fs.ctime =files[i].ctime;
		fs.mode =files[i].mode;
		if (files[i].type == LF_NORMAL || files[i].type == LF_LINK ){
			fs.type = REGULAR_FILE;
		}else if (files[i].type == LF_DIR){
			fs.type = DIRECTORY_FILE;
		}
		stat->type = fs.type;
		stat->inode_no = files[i].device_offset;
#if 0
		if (files[i].type == LF_LINK && files[i].link_id>= 0){
			j=files[i].link_id ;
			fs.st_size = files[j].length;
			fs.inode_no = files[j].device_offset;
			stat->inode_no = files[j].device_offset;
		}
#endif
		ret = JSUCCESS;
		ut_memcpy((unsigned char *) fs_get_stat(inodep, fs.type), (unsigned char *) &fs, sizeof(struct fileStat));
	}

	DEBUG(" Return tarfs STAT :%s: type :%x  inode_no:%x return:%d \n",inodep->filename,fs.type,stat->inode_no,ret);
	return ret;
}
void tar_fs::set_mount_pnt(unsigned char *mnt_pnt_arg){
	ut_strcpy(mnt_point,mnt_pnt_arg);
	mnt_len=ut_strlen(mnt_point);
}
static class tar_fs *tarfs_obj;
extern "C" {
int init_tarfs(jdriver *driver_arg) {
	tarfs_obj = jnew_obj(tar_fs);
	if (tarfs_obj->init(driver_arg) == JFAIL) return JFAIL;
	return fs_registerFileSystem(tarfs_obj, (unsigned char *)"tar_fs");
}
}
#endif
