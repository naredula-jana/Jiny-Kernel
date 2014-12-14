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

#include "file.hh"
#include "jdevice.h"
extern "C"{
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
#include "interface.h"
#define DEBUG(x...) do { \
} while (0)

}

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
	struct sparse sp[SPARSE_IN_HDR];
	uint8_t isextended;
	uint8_t realsize[12]; /* true size of the sparse file */
} header;

#define MAX_TAR_FILES 1024

struct tar_file {
	uint8_t name[NAMSIZ];
	uint8_t arch_linkname[NAMSIZ];
	unsigned long length,mtime,ctime,atime;
	unsigned long mode;
	unsigned long device_offset;
	uint8_t type;
	int link_id;

	/* meta data , not on the disk */
	long parent;

#define FLAG_METADATA_DIRTY 0x1
#define FLAG_DUMMY_FILE 0x2
#define FLAG_REGULAR_FILE 0x4
#define FLAG_SPECIAL_FILE 0x8
	uint8_t inmem_flags; /* various flags*/
	unsigned long dummyfile_len; /* this includes the header also */
} tar_file;


class tar_fs :public filesystem {
    unsigned char mnt_point[200];
    int mnt_len;
    unsigned char magic[8];
	struct tar_file files[MAX_TAR_FILES];
	int file_count;
	jdriver *driver;
	unsigned long extent_size;
	unsigned char *dummy_prefix;

	void lock();
	void unlock();

    struct tar_file *get_file(unsigned char *filename);

	int read_from_device(int offset, unsigned char *buf, int len);
	int write_to_device(int offset, unsigned char *buf, int len);
	int scan_device_forfiles();
	int create_new_file(unsigned char *filename_arg, int flags);
	int remove_file(struct tar_file *filep);
	int regularised_filename(unsigned char *filename,unsigned char *new_filename);
	int store_to_header(struct tar_file *file, struct header *hdr, struct header *dummy_hdr);
	unsigned long ciel_length(unsigned long length);
	int sync_file_metadata(struct tar_file *filep);
	struct tar_file *get_dir_entry(unsigned char *dirname, struct tar_file *next);
	int read_file_contents(unsigned char * filename, unsigned char *buf, int len, int offset);
	int write_file_contents(unsigned char * filename, unsigned char *buf, int len, int offset);

public:
	int init(jdriver *driver_arg, unsigned long extent_size_arg,unsigned char *prefixx);
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
	 void print_stat();

};
#define BLK_SIZE 512
#define CHKBLANKS "        "  /* 8 blanks no null */

static void to_oct(long value,  unsigned  char *where, int digs);
static void compute_cksum(struct header *header) {
	register int i, sum;
	register char *p;

	ut_memcpy(header->chksum, CHKBLANKS, sizeof(header->chksum));
	sum = 0;
	p = (unsigned char *) header;
	for (i = sizeof(struct header); --i >= 0;)
		/* We can't use unsigned char here because of old compilers, e.g. V7.  */
		sum += 0xFF & *p++;

	to_oct((long) sum, header->chksum, 8);
	header->chksum[6] = '\0'; /* zap the space */

	return;
}

static void to_oct (long value,   unsigned char *where , int digs)
{
  --digs;			/* Trailing null slot is left alone */
  where[--digs] = ' ';		/* put in the space, though */

  /* Produce the digits -- at least one.  */
  do
    {
      where[--digs] = '0' + (char) (value & 7);	/* one octal digit */
      value >>= 3;
    }
  while (digs > 0 && value != 0);

  /* Leading spaces, if necessary.  */
  while (digs > 0)
    where[--digs] = ' ';
}
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

void tar_fs::lock(){

}
void tar_fs::unlock(){

}
int tar_fs::store_to_header(struct tar_file *file, struct header *hdr, struct header *dummy_hdr) {
	int i;
	ut_memset((unsigned char *) hdr, 0, sizeof(struct header));

	ut_strcpy(hdr->name, file->name);
	hdr->linkflag = file->type;
	ut_memcpy(hdr->arch_linkname, file->arch_linkname, NAMSIZ);
	to_oct(0, hdr->offset, 12);
	to_oct(file->length, hdr->size, 13); /* this is to avoid the space at the first place*/
	hdr->size[11] = '\0';
	for (i=0; i<11; i++){
		if (hdr->size[i]==' ')hdr->size[i]='0';
	}
 	to_oct(file->atime, hdr->atime, 12);
	to_oct(file->mtime, hdr->mtime, 12);
	to_oct(file->ctime, hdr->ctime, 12);
	to_oct(file->mode, hdr->mode, 8);

	to_oct(0, hdr->uid, 8);
	to_oct(0, hdr->gid, 8);
	to_oct(0, hdr->chksum, 8);
	to_oct(0, hdr->devmajor, 8);
	to_oct(0, hdr->devminor, 8);
	ut_memcpy(hdr->magic,magic,8);
	if (dummy_hdr != 0) {
			ut_memcpy((unsigned char *)dummy_hdr,(unsigned char *)hdr,sizeof(struct header));
			ut_snprintf(dummy_hdr->name,NAMSIZ,"%s_%d", (unsigned char *) dummy_prefix,file->device_offset);
			dummy_hdr->linkflag = LF_NORMAL;
			to_oct(file->dummyfile_len-block_size, dummy_hdr->size, 13);
			dummy_hdr->size[11]='\0';
			for (i=0; i<11; i++){
				if (dummy_hdr->size[i]==' ')dummy_hdr->size[i]='0';
			}
			compute_cksum(dummy_hdr);
	}

	compute_cksum(hdr);
	return JSUCCESS;
}
int tar_fs::regularised_filename(unsigned char *filename,unsigned char *new_filename){
	int i,j,len;
	unsigned char *p;

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

	/* verify if the name is as initial mnt point */
	p=(unsigned char *)ut_strstr((const char *)new_filename,(const char *)mnt_point);
	if (p!=new_filename) return JFAIL;
	return JSUCCESS;
}
int tar_fs::scan_device_forfiles() {
	int i,j;
	int offset = 0;
	int completed = 0;
	long file_size;
	int blks;
	int len;
	struct header file;

	ut_log(" Tar fs Scan started :%s ..\n",mnt_point);
	ut_strcpy(files[0].name,".");
	files[0].length = 0;
	files[0].device_offset = 0;
	files[0].type = LF_DIR;
	files[0].inmem_flags = 0 | FLAG_SPECIAL_FILE;
	file_count = 1;
	stat_byte_reads=0;
	stat_byte_writes=0;
	device_size = driver->ioctl(IOCTL_DISK_SIZE,0);

#if 0 /* this is Hack to interprete the ISO filesystem for openstack, ISO file contain only one file starting at 0x18000 offset */
	if (ut_strcmp(mnt_point, "/data") == 0  && device_size<500000) {
		completed = 1;
		file_count = 3;
		ut_strcpy(files[1].name, "cloud_userfile");
		files[1].length = 0x800; /* 2048*/
		files[1].device_offset = 0x18000;
		files[1].type = LF_NORMAL;
		files[1].parent = 0;
		files[1].link_id = -1;
		files[1].inmem_flags = 0;

		ut_strcpy(files[2].name, "cloud_openstack");
		files[2].length = 0x800;
		files[2].device_offset = 0x18800;
		files[2].type = LF_NORMAL;
		files[2].parent = 0;
		files[2].link_id = -1;
		files[2].inmem_flags = 0;
	}
#endif
	while (completed == 0) {
		if (read_from_device(offset, (unsigned char *) &file,sizeof(struct header)) != sizeof(struct header)) {
			completed = 1;
			break;
		}
		if (file.name[0] == 0) {
			completed = 1;
			break;
		}

		file_size = from_oct(12, file.size);
	//	ut_printf("%d blk:%d   offset:%d  SIZE :%s:%d filename :%s \n", i,
	//			(offset / BLK_SIZE),  offset, file.size, file_size,file.name);

		blks = ciel_length(file_size)/BLK_SIZE;

		if (ut_strstr(file.name,dummy_prefix) != 0  && file_count>1){
			files[file_count-1].inmem_flags = files[file_count-1].inmem_flags | FLAG_DUMMY_FILE;
			files[file_count-1].dummyfile_len = files[file_count-1].dummyfile_len + (blks+1) * BLK_SIZE;
			offset = offset + BLK_SIZE + (blks * BLK_SIZE);
			filesystem_size = files[file_count-1].device_offset + (files[file_count-1].length+files[file_count-1].dummyfile_len+BLK_SIZE);
			continue;
		}
		files[file_count].inmem_flags = FLAG_REGULAR_FILE;
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
		filesystem_size = files[file_count].device_offset + (blks*BLK_SIZE);
		if (device_size < 200000){
			ut_log("%d : %s  fs_size:%d offset:%d \n",file_count,files[file_count].name,filesystem_size,files[file_count].device_offset);
		}
		/* remove "/" at the end of every directory */
		if (files[file_count].type == LF_DIR) {
			len = ut_strlen(files[file_count].name);
			if (len > 1 && files[file_count].name[len - 1] == '/') {
				files[file_count].name[len - 1] = 0;
			}
		}
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

		file_count++;
		offset = offset + BLK_SIZE + (blks * BLK_SIZE);
		file.name[0] = 0;
	}
	/* copy some of the time to the root dircory */
	files[0].mtime = files[file_count-1].mtime;
	files[0].ctime = files[file_count-1].ctime;
	files[0].atime = files[file_count-1].atime;
	files[0].mode = files[file_count-1].mode;
	ut_memcpy(magic,file.magic,8);
	//list_files();
	for (i=1; i<file_count; i++){
		if (files[i].parent == -1 && ut_strstr(&files[i].name[1],"/")==0){
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
	ut_log(" Tar fs Scan Ended: filecount: %d\n",file_count);

	return file_count;
}
void tar_fs::print_stat() {
	int i;

	ut_printf("    total files : %d extentsize: %d\n", file_count,extent_size);
	for (i = 0; i < file_count; i++) {
		ut_printf("    %x(%d) blk:%d(offset:%d) length:%d :%s--%s  parent:%x type:%x dummy_len:%d off:%d flags:%x\n", i,i,
				files[i].device_offset/block_size, files[i].device_offset, files[i].length,
				mnt_point,files[i].name,files[i].parent,files[i].type,files[i].dummyfile_len,files[i].device_offset,files[i].inmem_flags);
	}
	return;
}
int tar_fs::init(jdriver *driver_arg,unsigned long extent_size_arg, unsigned char *dummy_prefix_arg) {
	file_count = 0;

	if (driver_arg == 0) return JFAIL;

	driver = driver_arg;
	file_count = -1;
	block_size = BLK_SIZE;
	extent_size = ciel_length(extent_size_arg); /* make multiples of block_size*/
	dummy_prefix = dummy_prefix_arg;
	return JSUCCESS;
}


int tar_fs::create_new_file(unsigned char *filename_arg, int flags){
	int ret=JFAIL;
	unsigned char filename[NAMSIZ];
	unsigned char *p;
	int prefix_len;

	if (device_size < (filesystem_size + (2*block_size) + extent_size)){
		ut_log("ERROR: No space in the filesystem device size:%d  fs size:%d\n",device_size,filesystem_size);
		return ret;
	}
	if (regularised_filename(filename_arg,filename) == JFAIL){
		return ret;
	}
	prefix_len=mnt_len;
	if (filename[mnt_len]=='/'){
		prefix_len=mnt_len+1;
	}

	ut_memcpy((unsigned char *)&files[file_count],(unsigned char *)&files[0],sizeof(struct tar_file));
	ut_memcpy(files[file_count].name,&filename[prefix_len],ut_strlen(&filename[prefix_len]));

    files[file_count].length = 0;
    files[file_count].device_offset = filesystem_size +  block_size;

	if (flags & O_DIRECTORY){
		files[file_count].type = LF_DIR;
	    files[file_count].inmem_flags = FLAG_METADATA_DIRTY | FLAG_REGULAR_FILE;
	}else{
		files[file_count].type = LF_NORMAL;
	    files[file_count].inmem_flags = FLAG_METADATA_DIRTY | FLAG_DUMMY_FILE | FLAG_REGULAR_FILE;
	    files[file_count].dummyfile_len = block_size + extent_size;
	}

    filesystem_size = filesystem_size + (2*block_size) + extent_size;
	file_count = file_count+1;
	sync_file_metadata(&files[file_count-1]);

	ret=JSUCCESS;
	ut_printf(" New file created :%s\n",filename);
	return ret;
}
int tar_fs::remove_file(struct tar_file *filep){
	int ret = JFAIL;

	if (filep->inmem_flags | FLAG_REGULAR_FILE){
		filep->inmem_flags = filep->inmem_flags & (~FLAG_REGULAR_FILE) ;
		filep->inmem_flags = filep->inmem_flags | FLAG_DUMMY_FILE;
		filep->dummyfile_len = filep->dummyfile_len + ciel_length(filep->length) + block_size;
		filep->length = 0;
		filep->inmem_flags = filep->inmem_flags | FLAG_METADATA_DIRTY;
		sync_file_metadata(filep);
		ret = JSUCCESS;
	}
	return ret;

}
int tar_fs::sync_file_metadata(struct tar_file *filep){
	struct header *hdr,*dummy_hdr=0;
	int ret,blks;

	if (!(filep->inmem_flags & FLAG_METADATA_DIRTY)){
		return JSUCCESS;
	}
	if ((filep->inmem_flags & FLAG_DUMMY_FILE) && filep->dummyfile_len>0){
		dummy_hdr = mm_malloc(sizeof(struct header),0);
	}
	hdr = mm_malloc(sizeof(struct header),0);
	store_to_header(filep, hdr,dummy_hdr);
	if (filep->inmem_flags & FLAG_REGULAR_FILE){
		ret = write_to_device(filep->device_offset-block_size,(unsigned char *)hdr,sizeof(struct header));
		//ut_printf("writing meta block :%d: \n",filep->device_offset-block_size);
	}

	if (dummy_hdr != 0){
		blks = ciel_length(filep->length) / block_size;
		if (!(filep->inmem_flags & FLAG_REGULAR_FILE)){
			blks = -1;
		}
		ret = write_to_device(filep->device_offset+(blks * block_size),(unsigned char *)dummy_hdr,sizeof(struct header));
	//	ut_printf("writing dummy meta block :%d: \n",filep->device_offset+(blks * block_size));
		mm_free(dummy_hdr);
	}
	mm_free(hdr);
	filep->inmem_flags = filep->inmem_flags & (~FLAG_METADATA_DIRTY);
	//ut_printf(" Meta Data of a file is Synced len: %s -> %d: offset:%d\n",filep->name,filep->length,filep->device_offset);
	return JSUCCESS;
}

struct tar_file *tar_fs::get_file(unsigned char *filename_arg) {
	int i,len;
	unsigned char *p;
	unsigned char filename[NAMSIZ];
	int prefix_len;

	if (file_count == -1){
		scan_device_forfiles();
	}
	if (regularised_filename(filename_arg,filename) == JFAIL){
		return NULL;
	}

	len = ut_strlen((const char *)filename);
	if (len > 1 && filename[len-1]=='.' && filename[len-2]=='/'){
		filename[len-2]=0;
		len =len-2;
	}

	if (len == mnt_len){
		return &files[0];
	}
	prefix_len=mnt_len;
	if (filename[mnt_len]=='/'){
		prefix_len=mnt_len+1;
	}
	for (i = 0; i < file_count; i++) {
		if (ut_strcmp((const char *) &filename[prefix_len], (const char *) files[i].name)== 0) {
			return &files[i];
		}
	}
	//ut_printf(" ERROR:  tarfs cannot find the file  :%s: \n",filename);
	return NULL;
}

struct tar_file *tar_fs::get_dir_entry(unsigned char *dirname_arg, struct tar_file *next) {
	int i,len,dir_len,prefix_len;
	long parent=-1;
	unsigned char dirname[NAMSIZ];
	unsigned char *p;

	if (regularised_filename(dirname_arg,dirname)==JFAIL){
		return 0;
	}

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
			if (&files[i] > next) {
				return &files[i];
			}
		}
	}

	return 0;
}
int tar_fs::read_from_device(int offset, unsigned char *buf, int len) {
	int ret;

	if (file_count == -1){
		scan_device_forfiles();
	}
	ret = driver->read(buf,len,offset);

	if (ret > 0){
		stat_byte_reads = stat_byte_reads + ret;
		stat_read_req++;
	}else{
		stat_read_errors++;
	}
	return ret;
}
int tar_fs::write_to_device(int offset, unsigned char *buf, int len) {
	int ret;

	if (file_count == -1){
		scan_device_forfiles();
	}

	ret = driver->write(buf,len,offset);
	if (ret > 0){
		stat_byte_writes = stat_byte_reads + ret;
		stat_write_req++;
	}else{
		stat_write_errors++;
	}
	return ret;
}
int tar_fs::read_file_contents(unsigned char *filename, unsigned char *buf, int len_arg, int offset) {
	int ret_len=0;
	int tmp_ret;
	struct tar_file *filep = get_file(filename);

	if (filep == NULL)
		return 0;
	if (filep->length <= offset)
		return -1;
	if (filep->length <= (offset + len_arg))
		len_arg = filep->length - offset;

	tmp_ret=1;
	while (tmp_ret > 0 && (len_arg-ret_len)>0){
		tmp_ret = read_from_device(filep->device_offset+offset+ret_len,buf+ret_len,(len_arg-ret_len));

		if (tmp_ret > 0){
			ret_len = ret_len + tmp_ret;
		}else{
			break;
		}
	}

	return ret_len;
}
unsigned long tar_fs::ciel_length(unsigned long length){
	unsigned long ret;

	ret = length/block_size;
	if ((ret*block_size) != length){
		ret = (ret+1)* block_size;
	}else{
		ret = ret* block_size;
	}
	return ret;
}
int tar_fs::write_file_contents(unsigned char *filename, unsigned char *buf, int len_arg, int offset) {
	int ret_len=0;
	int tmp_ret;
	unsigned long new_size,allowed_size,allowed_length;
	struct tar_file *filep = get_file(filename);

	if (filep == 0)
		return 0;

	new_size = filep->length;
	allowed_length = len_arg;
	if ((filep->length <= (offset+len_arg)) ){
		long blen = ciel_length(filep->length);
		if (filep->dummyfile_len ==0 && ((blen+filep->device_offset) == filesystem_size)){ /* if this is a last file */
			if ((filesystem_size+extent_size) < device_size){ /* refill the last file */
				filesystem_size = filesystem_size + extent_size;
				filep->dummyfile_len = filep->dummyfile_len + extent_size;
				filep->inmem_flags = filep->inmem_flags | FLAG_METADATA_DIRTY;
			}
		}
		if  ((blen+filep->dummyfile_len) > offset){
			new_size = offset + len_arg;
			allowed_size = blen+filep->dummyfile_len;
			if (allowed_size < new_size){
				allowed_length = allowed_size - offset;
			}
		}else{
			ut_printf(" Error in writing the filename :%s: \n",filename);
			return len_arg; // TODO : need to zero , sync should return to user
		}
	}

	tmp_ret=1;
	ret_len=0;
	while (tmp_ret > 0 && (allowed_length-ret_len)>0){
		tmp_ret = write_to_device(filep->device_offset+offset+ret_len,buf+ret_len,(allowed_length-ret_len));

		if (tmp_ret > 0){
			ret_len = ret_len + tmp_ret;
		}else{
			break;
		}
	}
	if (ret_len > 0 && (new_size!=filep->length)){
		unsigned long old_len = ciel_length(filep->length);
		if ((offset + ret_len) > filep->length) {
			filep->length = offset + ret_len;
			if (filep->length > old_len) {
				filep->dummyfile_len = filep->dummyfile_len - ciel_length(filep->length - old_len);
			}
			filep->inmem_flags = filep->inmem_flags | FLAG_METADATA_DIRTY;
			sync_file_metadata(filep);
		}

	}
	return ret_len;
}

/* list of virtual functions */
long tar_fs::read(fs_inode *inodep, uint64_t offset, unsigned char *buff, unsigned long len_arg) {
	int ret_len=0;

	DEBUG(" inside the tarfs read: offset:%d len_arg:%d  filename:%s:\n", offset, len_arg,inodep->filename);
	ret_len = read_file_contents(inodep->filename, buff, len_arg,  offset);
	DEBUG(" read from tarfs ret: %d buff:%x \n", ret_len,buff);

	return ret_len;
}
long tar_fs::write(fs_inode *inodep, uint64_t offset, unsigned char *buff, unsigned long len_arg) {
	int ret_len=0;

	DEBUG(" inside the tarfs write: offset:%d len_arg:%d  filename:%s:\n", offset, len_arg,inodep->filename);
	ret_len = write_file_contents(inodep->filename, buff, len_arg,  offset);
	DEBUG(" write from tarfs ret: %d buff:%x \n", ret_len,buff);
	return ret_len;
}

long tar_fs::readDir(fs_inode *inodep, struct dirEntry *dir_p, unsigned long dir_max, int *offset) {
	int len,i,s;
	unsigned char *p;
	int next=-3; /* -3 for . -2 for .. */
	int total_len=0;
	struct tar_file *dir_filep,*next_dirp;

	next_dirp=0;

	DEBUG(" INSIDE... the tarfs READDIR :%s: %d:\n", inodep->filename,*offset);
	if (*offset > 0)
		return 0;
	struct tar_file *filep=get_file(inodep->filename);
	if (filep == 0){
		return total_len;
	}
	p=(unsigned char *)dir_p;
	for (i = 0; (total_len<dir_max); i++) {
		if (next >= -1){
			dir_filep = get_dir_entry(inodep->filename,next_dirp);
			//next = d;
			next_dirp = dir_filep;
			if (dir_filep == 0) break;
			dir_p->inode_no = dir_filep->device_offset;
			s = get_filename_start(dir_filep->name);
			ut_sprintf(dir_p->filename,"%s",(const unsigned char *)&dir_filep->name[s]);
		}else{
			if (next ==-3){
				ut_strcpy(dir_p->filename,".");
				dir_p->inode_no = filep->device_offset;
			}else if (next ==-2){
				int parent=filep->parent;
				ut_strcpy(dir_p->filename,"..");
				if (parent > -1){
					dir_p->inode_no = files[parent].device_offset;
				}else{
					dir_p->inode_no = filep->device_offset;
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
}
int tar_fs::lseek(struct file *filep, unsigned long offset, int whence) {
	filep->offset = offset;
	return JSUCCESS;
}

int tar_fs::open(fs_inode *inodep, int flags, int mode) {
	struct tar_file *filep = get_file(inodep->filename);
	int ret = JFAIL;

	if (filep != 0) {
		DEBUG(" tarfs open success: %s\n", inodep->filename);
		return JSUCCESS;
	} else {
		if (flags & O_CREAT) {
			ret = create_new_file(inodep->filename, flags);
		}
	}
	if (ret == JFAIL){
		ut_printf("ERROR tarfs open Fail :%s: \n", inodep->filename);
	}
	return ret;
}
int tar_fs::close(fs_inode *inode) {
	return JSUCCESS;
}
int tar_fs::remove(fs_inode *inode) {
	int ret = JFAIL;
	struct tar_file *filep=get_file(inode->filename);
	if (filep==0){
		return ret;
	}
	ret = remove_file(filep);
	return ret;
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

	struct tar_file *filep = get_file(inodep->filename);
	if (filep != 0) {
		fs.st_size = filep->length;
		fs.inode_no = filep->device_offset; /* this acts as unique id for a regular file */
		fs.atime = filep->atime;
		fs.mtime =filep->mtime;
		//fs.ctime =filep->ctime;
		fs.mode =filep->mode;
		if (filep->type == LF_NORMAL || filep->type == LF_LINK ){
			fs.type = REGULAR_FILE;
		}else if (filep->type == LF_DIR){
			fs.type = DIRECTORY_FILE;
		}
		stat->type = fs.type;
		stat->inode_no = filep->device_offset;
#if 0
		if (filep->type == LF_LINK && filep->link_id>= 0){
			j=filep->link_id ;
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
//unsigned long g_conf_tarfs_extent_size=20000 ; /* 20k */
unsigned long g_conf_tarfs_extent_size=512*8000 ; /* 512k */
unsigned char *g_conf_tarfs_dummy_prefix = ".dummy_file";
int init_tarfs(jdriver *driver_arg) {
	tarfs_obj = jnew_obj(tar_fs);
	if (tarfs_obj->init(driver_arg, g_conf_tarfs_extent_size, g_conf_tarfs_dummy_prefix) == JFAIL) return JFAIL;
	if (driver_arg->device->pci_device.pci_header.device_id == VIRTIO_PCI_SCSI_DEVICE_ID){
		return fs_registerFileSystem(tarfs_obj, (unsigned char *)"tar_fs1");
	}else{
		return fs_registerFileSystem(tarfs_obj, (unsigned char *)"tar_fs");
	}
}
}

