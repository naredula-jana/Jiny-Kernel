//#define DEBUG_ENABLE 1
#include "file.hh"
extern "C"{
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
#include "interface.h"
#include "9p.h"

#define OFFSET_ALIGN(x) ((x/PC_PAGESIZE)*PC_PAGESIZE) /* the following 2 need to be removed */

static p9_client_t client;
static int stat_reads=0;
static int stat_writes=0;

int Jcmd_p9(){
	int i;
	int count=0;

	for (i = 0; i < MAX_P9_FILES; i++) {
		if (client.files[i].fid != 0) {
			count++;
		}
	}
	ut_printf("P9 reads:%d writes:%d  count:%d\n",stat_reads,stat_writes,count);
	return 1;
}
static int p9_mount() {
	unsigned long addr, i;
	uint32_t msg_size;
	int ret;
	static int root_id=132;
	unsigned char version[200];
	ut_log("p9 mount\n");

	client.type = P9_TYPE_TVERSION;
	client.tag = 0xffff;

	addr = p9_write_rpc(&client, "ds", 4090*2, "9P2000.u");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "ds", &msg_size, version);
	}
	ut_log("p9 init read msgsize:%d  version:%s: \n",msg_size,version);
	DEBUG("New cmd:%x size:%x version:%s  pkt_buf:%x\n",ret, msg_size,version,client.pkt_buf);

	client.type = P9_TYPE_TATTACH;
	client.tag = 0x13;
	client.root_fid = root_id;
	root_id = root_id +200;
	addr = p9_write_rpc(&client, "ddss", client.root_fid, ~0, "jana", "");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
	}
	for (i = 0; i < MAX_P9_FILES; i++) {
		client.files[i].fid = 0;
	}
	client.next_free_fid = client.root_fid + 1;
	return JSUCCESS;
}
static int p9_remount=0;

static int p9ClientInit() {
	static int init = 0;
	uint32_t msg_size;
	int ret;
	unsigned char version[200];
	unsigned long addr,i;

	if (init != 0){
		if (client.root_fid == 0){
			if (p9_remount == 0) return JFAIL;
			return p9_mount();
		}
		return JSUCCESS;
	}
	init = 1;

	client.pkt_buf = (unsigned char *) mm_getFreePages(MEM_CLEAR, 2);
	client.pkt_len = PAGE_SIZE*4;
	ut_memset(client.pkt_buf,0,client.pkt_len);

	client.lock = mutexCreate("mutex_p9");
    if (client.lock == 0) return 0;

	return p9_mount();
}

static int p9_open(uint32_t fid, unsigned char *filename, int flags, int arg_mode) {
	unsigned long addr;
	uint8_t mode_b;
	uint32_t perm;
	int i,ret= JFAIL;

	for (i = 0; i < MAX_P9_FILES; i++) {
		if (client.files[i].fid ==fid) {
			if (client.files[i].opened == 1) return JSUCCESS;
			break;
		}
	}
	if (i==MAX_P9_FILES) return JFAIL;
//	mode_b = arg_mode;
	mode_b = 2; /*TODO: every file is opened as RDWR with p9 filesystem, since the open file will use for all the files(stat, read file, write file etc) in vfs */
	if (flags & O_CREAT) {
		client.type = P9_TYPE_TCREATE;
		perm = 0x1ff; /* this is same as 777 */
		if (flags & O_DIRECTORY)
			perm = perm | 0x80000000;
		addr = p9_write_rpc(&client, "dsdbs", fid, filename, perm, mode_b,"");
		DEBUG("p9 Creating the New file :%s: \n",filename);
	} else {
		client.type = P9_TYPE_TOPEN;
		addr = p9_write_rpc(&client, "db", fid, mode_b);
	}

	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
		if (client.recv_type == P9_TYPE_ROPEN || client.recv_type == P9_TYPE_RCREATE) {
			ret = JSUCCESS;
			client.files[i].opened = 1;
		}
		//ut_log(" p9fs OPEN fid:%d  return type :%d \n",fid,client.recv_type);
	}
	return ret;
}

static uint32_t p9_walk(unsigned char *filename, int flags, unsigned char **create_filename) {
	unsigned long addr;
	static unsigned char names[MAX_P9_FILEDEPTH][MAX_P9_FILELENGTH];
	int ret, len, levels;
	int i, j, empty_fd = -1;
	uint32_t parent_fid, ret_fd = 0;

	i = 0;
	levels = 0;
	j = 0;

	while (i < MAX_P9_FILELENGTH && (filename[i] != '\0')) {
		if (filename[i] == '/') {
			if (j > 0) {
				names[levels][j] = '\0';
				levels++;
			}
			j = 0;
			i++;
			continue;
		}
		names[levels][j] = filename[i];
		j++;
		i++;
	}
	names[levels][j] = '\0';
	j = 0;
	parent_fid = client.root_fid;
	while (j <= levels) { // start to walk
		empty_fd = -1;
		ret_fd = 0;
		for (i = 0; i < MAX_P9_FILES; i++) {
			if (client.files[i].fid != 0) {
				if ((ut_strcmp(client.files[i].name, names[j]) == 0) && (client.files[i].parent_fid == parent_fid)) {
					parent_fid = client.files[i].fid;
					ret_fd = client.files[i].fid;
					if (j == levels) { // leaf level
						goto last;
					}
					break;
				}
				continue;
			}
			if (empty_fd == -1)
				empty_fd = i;
		}
		if (ret_fd == 0) { // walk only if it is not found in cache
			client.type = P9_TYPE_TWALK;
			addr = p9_write_rpc(&client, "ddws", parent_fid, client.next_free_fid, 1, names[j]);
			if (addr != 0) {
				ret = p9_read_rpc(&client, "");
				if ((empty_fd != -1) && (client.recv_type == P9_TYPE_RWALK)) {
					client.files[empty_fd].fid = client.next_free_fid;
					client.next_free_fid++; /* TODO : there will be collision , the logic need to replaced with better one */
					ut_strcpy(client.files[empty_fd].name, names[j]);
					client.files[empty_fd].parent_fid = parent_fid;
					parent_fid = client.files[empty_fd].fid; /* new fd becoms parent for next walk or search */
					if (j == levels) {
						ret_fd = client.files[empty_fd].fid;
						goto last;
					}
				} else {
					if ((flags&O_CREAT) && (j == levels) && (client.recv_type != P9_TYPE_RWALK))
					{ /* Retry it without name */
						addr = p9_write_rpc(&client, "ddw", parent_fid, client.next_free_fid, 0);
						if (addr != 0) {
							ret = p9_read_rpc(&client, "");
							if ((empty_fd != -1) &&  (client.recv_type == P9_TYPE_RWALK)) {
								ret_fd=client.next_free_fid;
								client.files[empty_fd].fid = client.next_free_fid;
								client.files[empty_fd].opened = 0;
								client.next_free_fid++; /* TODO : there will be collision , the logic need to replaced with better one */
								ut_strcpy(client.files[empty_fd].name, names[j]);
								client.files[empty_fd].parent_fid = parent_fid;
							}
							else ret_fd =0;
						}
                        len=ut_strlen(filename)-ut_strlen(names[j]);
                        *create_filename=filename+len;
						goto last;
					}
					ret_fd = 0;
					goto last;
				}
			}
		}
		j++;
	}

	last:
	return ret_fd;
}

static int p9_get_dir_entries(p9_client_t *client, struct dirEntry *dir_ent, int max_len, int *offset) {
	unsigned char *recv;
	p9_fcall_t pdu;
	int i, ret=0;
	int len,total_len=0;
	uint32_t total_len1,total_len2;
	unsigned char p9_ret,type1, type2;
	uint32_t dummyd;
	uint16_t tag;
	uint64_t dummyi;
	struct dirEntry  *dir_p=dir_ent;
	unsigned char *p;

	recv = client->pkt_buf + 1024;
	p9_pdu_init(&pdu, 0, 0, client, recv, (unsigned long)client->pkt_len-1024);
	ret = p9_pdu_read_v(&pdu, "dbw", &total_len1, &p9_ret, &tag);
	ret = p9_pdu_read_v(&pdu, "d", &total_len2);
	//ut_printf("New readir len1: %d len2:%d tag:%d type:%d  maxbuflen:%d\n",total_len1,total_len2,tag,p9_ret,client->pkt_len-1024);

	if (p9_ret != P9_TYPE_RREADDIR){ /* cannot read directory entries */
		return 0;
	}
	p=(unsigned char *)dir_p;
	for (i = 0; (total_len<max_len) && ret==0; i++) {
		ret = p9_pdu_read_v(&pdu, "bdqqbs", &type1, &dummyd, &dir_p->inode_no, &dummyi,
				&type2, dir_p->filename);
	//	ut_printf(" ret :%d :inode:%d   name :%s:  total_len:%d\n",ret,dir_p->inode_no,dir_p->filename,total_len);
		len = 2 * (sizeof(unsigned long)) + sizeof(unsigned short) + ut_strlen(dir_p->filename) + 2;

		if (dir_p->inode_no==0 || (type2!=4 && type2!=10 && type2!=8)){ /* check for soft link, dir,regular file */
			break;
		}
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
	//ut_printf(" Total_LEn:%d  offset:%d\n",total_len,i);

	return total_len;
}

static uint32_t p9_readdir(uint32_t fid, unsigned char *data,
		uint32_t data_len, int *offset) {
	unsigned long addr;
	int ret;
	struct fileStat fstat, *stat;
	stat = &fstat;

	client.type = P9_TYPE_TREADDIR;
	client.user_data = 0;
	client.userdata_len = 0;

	addr = p9_write_rpc(&client, "dqd", fid, 0, 28000);

	ret=p9_get_dir_entries(&client,(struct dirEntry *)data,data_len, offset);
	return ret;
}
static unsigned char test1[1000];
static unsigned char test2[1000];
static uint32_t p9_read(uint32_t fid, uint64_t offset, unsigned char *data,
		uint32_t data_len, int file_type) {
	unsigned long addr;
	int ret;
	uint32_t read_len = 0;

	if (file_type==SYM_LINK_FILE){
		client.type = P9_TYPE_TREADLINK;
		client.user_data = data;
		client.userdata_len = data_len;

		addr = p9_write_rpc(&client, "d", fid);
		if (addr != 0) {
			test1[0]=0;
			test2[0]=0;
			ret = p9_read_rpc(&client, "sd", data,&read_len);
			//ut_printf(" readlink file name :%s: len: %d  %d ret\n",data,read_len,ret);
		}
	} else {
		client.type = P9_TYPE_TREAD;
		client.user_data = data;
		client.userdata_len = data_len;
		pc_check_valid_addr(data, data_len);
		stat_reads++;
		addr = p9_write_rpc(&client, "dqd", fid, offset, data_len);
		if (addr != 0) {
			ret = p9_read_rpc(&client, "d", &read_len);
			if (client.recv_type != P9_TYPE_RREAD) {
				read_len = 0;
				ut_log("P9 ERROR Read.. fid:%d max_size:%d read_len:%d ret%d recvtype:%d \n",fid,data_len,read_len,ret,client.recv_type);
			}

		}
	}

	return read_len;
}
/* Attribute flags */
#define P9_ATTR_MODE       (1 << 0)
#define P9_ATTR_UID        (1 << 1)
#define P9_ATTR_GID        (1 << 2)
#define P9_ATTR_SIZE       (1 << 3)
#define P9_ATTR_ATIME      (1 << 4)
#define P9_ATTR_MTIME      (1 << 5)
#define P9_ATTR_CTIME      (1 << 6)
#define P9_ATTR_ATIME_SET  (1 << 7)
#define P9_ATTR_MTIME_SET  (1 << 8)

#if 0
typedef struct V9fsIattr
{
    int32_t valid;
    int32_t mode;
    int32_t uid;
    int32_t gid;
    int64_t size;
    int64_t atime_sec;
    int64_t atime_nsec;
    int64_t mtime_sec;
    int64_t mtime_nsec;
} V9fsIattr;
#endif
static uint32_t p9_setattr(uint32_t fid, uint64_t size) {
	unsigned long addr;
	int ret = JFAIL;

	client.type = P9_TYPE_TSETATTR;
	client.user_data = 0;
	client.userdata_len = 0;

	/* TODO : currently only truncate/set size is implemented */
	addr = p9_write_rpc(&client, "dddddqqqqq", fid,P9_ATTR_SIZE,0,0,0,size,0,0,0,0 );
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
		if (client.recv_type == P9_TYPE_RSETATTR) {
			ret = JSUCCESS;
		}
	}
	return ret;
}

static uint32_t p9_write(uint32_t fid, uint64_t offset, unsigned char *data, uint32_t data_len) {
	unsigned long addr;
	int ret;
	uint32_t write_len=0;
	unsigned char *rd;

	client.type = P9_TYPE_TWRITE;
	client.user_data = data;
	client.userdata_len = data_len;
	pc_check_valid_addr(data, data_len);

	stat_writes++;
	addr = p9_write_rpc(&client, "dqd", fid, offset, data_len);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "d", &write_len);
		if (client.recv_type != P9_TYPE_RWRITE) {
			write_len = 0;
			ut_log("ERROR  p9  Write fid:%d data_len:%d offset:%d ret%d recvtype:%d \n",fid,data_len,offset,ret,client.recv_type);
		}
	}

	rd = client.pkt_buf+1024+10;
	rd[20]='\0';

	if (write_len > data_len){ /* something wrong */
		ut_printf("ERROR P9 write datalen :%d  write_rpc ret:%d : offset:%d write_len:%d\n",data_len,ret,offset,write_len);
		write_len =0;
	}
	return write_len;
}

static uint32_t p9_remove(uint32_t fid) {
	unsigned long addr;
	int i,ret=JFAIL;

	client.type = P9_TYPE_TREMOVE;
	client.user_data = 0;
	client.userdata_len = 0;

	addr = p9_write_rpc(&client, "d", fid);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
		if (client.recv_type == P9_TYPE_RREMOVE) {
			ret = JSUCCESS;
			for (i = 0; i < MAX_P9_FILES; i++) {
				if (client.files[i].fid ==fid) { /* remove the fid as the file is sucessfully removed */
					ut_memset((unsigned char *)&client.files[i],0,sizeof(p9_file_t));
					break;
				}
			}
		}
	}
	return ret;
}
static uint32_t p9_close(uint32_t fid) {
	unsigned long addr;
	int i, ret = JFAIL;

	client.type = P9_TYPE_TCLUNK;
	client.user_data = 0;
	client.userdata_len = 0;

	addr = p9_write_rpc(&client, "d", fid);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
		if (client.recv_type == P9_TYPE_RCLUNK) {
			ret = JSUCCESS;
			for (i = 0; i < MAX_P9_FILES; i++) {
				if (client.files[i].fid == fid) { /* remove the fid as the file is sucessfully closed */
					ut_memset((unsigned char *)&client.files[i],0,sizeof(p9_file_t));
					break;
				}
			}
		}
	}
	return ret;
}

static uint32_t p9_stat(uint32_t fid, struct fileStat *stat) {
	unsigned long addr;
	int ret=JFAIL;
	uint32_t dummyd;
	uint16_t dummyw3;
	unsigned char type;
	uint32_t ret_zero,stat_size;

	client.type = P9_TYPE_TSTAT;
	client.user_data = 0;
	client.userdata_len = 0;

	addr = p9_write_rpc(&client, "d", fid);
	if (addr != 0) {
		//"wwdbdqdddqssss?sddd"
		//Q=bdq
		//"wwdQdddqsssssddd"
		ret = p9_read_rpc(&client, "wwwdbdqdddq", &ret_zero, &stat_size,
				&dummyw3, &stat->blk_size, &type, &dummyd, &stat->inode_no,
				&stat->mode, &stat->atime, &stat->mtime, &stat->st_size);

	//	ut_log(" fid:%d ret_zero:%d stat size :%x:  inode:%d:  size:%d recv_type:%d\n",
	//		fid, ret_zero, stat_size, stat->inode_no, stat->st_size,
	//			client.recv_type);

		stat->type = 0;
		if (type == 0) { /* Regular file */
			stat->type = REGULAR_FILE;
		} else if (type == 0x80) { /* directory */
			stat->type = DIRECTORY_FILE;
		} else if (type == 2) { /* soft link */
			stat->type = SYM_LINK_FILE;
		} else {
			return JFAIL;
		}
		if (client.recv_type == P9_TYPE_RSTAT) {
			ret = JSUCCESS;
		}
	}

	return ret;
}
static void update_size(void *inode,uint64_t offset, int len ) {
	if (inode !=0 && len > 0) {
		if (fs_get_size(inode) < (offset+len))
			fs_set_size(inode, offset+len);
	}
	return ;
}
}

extern void *p9_dev;
/* This is central switch where the call from vfs routed to the p9 functions */
static int p9Request(unsigned char type, void *inode, uint64_t offset, unsigned char *data, int data_len, int flags, int mode) {
	uint32_t fid;
	unsigned char *createFilename;
	int ret = JFAIL ;

	if (inode == 0 || p9_dev == 0)
		return ret;

	fid = fs_get_private(inode);
	mutexLock(client.lock);
	if (type == REQUEST_OPEN) {
		fid = p9_walk(fs_get_filename(inode), flags, &createFilename);
		DEBUG("P9 open filename :%s: flags:%x mode:%x  createfilename :%s:\n",inode->filename,flags,mode,createFilename);
		if (fid > 0) {

			fs_set_private(inode, fid);
			ret = p9_open(fid, createFilename, flags, mode);
		} else {
            ret = JFAIL;
		}
	} else if (type == REQUEST_READ) {

		ret = p9_read(fid, offset, data, data_len,fs_get_inode_type(inode));
		update_size(inode,offset,ret);
	} else if (type == REQUEST_WRITE) {

		ret = p9_write(fid, offset, data, data_len);
		update_size(inode,offset,ret);
	} else if (type == REQUEST_REMOVE) {

		ret = p9_remove(fid);
	} else if (type == REQUEST_STAT) {
		struct fileStat *fp = (struct fileStat *)data;

		ret = p9_stat(fid, fp);
		if (ret==JSUCCESS){

			ut_memcpy(fs_get_stat(inode,fp->type),data,sizeof(struct fileStat));
		}
	} else if (type == REQUEST_CLOSE) {

		ret = p9_close(fid);
	}else if (type == REQUEST_READDIR) {

		ret = p9_readdir(fid,data,data_len, offset);
	}else if (type == REQUEST_SETATTR) {

		ret = p9_setattr(fid,offset);
		if (ret == JSUCCESS){
			fs_set_offset(inode,offset);
		}
	}


    mutexUnLock(client.lock);
    return ret;
}

class p9_fs :public filesystem {
public:
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
		int abc[200];
};
 int p9_fs::open(fs_inode *inodep, int flags, int mode) {
	 //struct inode *inodep = (struct inode *)inodep_arg;
	if (p9ClientInit() == JFAIL) return JFAIL;
	return p9Request(REQUEST_OPEN, inodep, 0, 0, 0, flags, mode);
}

int p9_fs::lseek(struct file *filep, unsigned long offset, int whence) {
	filep->offset = offset;
	return 1;
}

int p9_fs::fdatasync(fs_inode *inodep) {

	return 1;
}
long p9_fs::write(fs_inode *inodep, uint64_t offset, unsigned char *data, unsigned long  data_len) {
	long ret;
	p9ClientInit();
    ret = (long) p9Request(REQUEST_WRITE, inodep, offset, data, data_len, 0, 0);
    if (ret > 0){
    	fs_set_offset(inodep,offset);
    	//inodep->stat_last_offset = offset;
    	//inodep->stat_out++;
    }else{
    	//inodep->stat_err++;
    }
    return ret;
}

long p9_fs::read(fs_inode *inodep, uint64_t offset, unsigned char *data, unsigned long  data_len) {
	if (p9ClientInit() == JFAIL) return JFAIL;
    return  (long)p9Request(REQUEST_READ, inodep, offset, data, data_len, 0, 0);
}

long p9_fs::readDir(fs_inode *inodep, struct dirEntry *dir_ptr, unsigned long dir_max, int *offset) {
	if (p9ClientInit() == JFAIL) return JFAIL;
    return  (long)p9Request(REQUEST_READDIR, inodep, offset, (unsigned char *)dir_ptr, dir_max, 0, 0);
}

int p9_fs::remove(fs_inode *inodep) {
	if (p9ClientInit() == JFAIL) return JFAIL;
    return  p9Request(REQUEST_REMOVE, inodep, 0, 0, 0, 0, 0);
}

int p9_fs::stat(fs_inode *inodep, struct fileStat *statp) {
	if (p9ClientInit() == JFAIL) return JFAIL;
    return  p9Request(REQUEST_STAT, inodep, 0, (unsigned char *)statp, 0, 0, 0);
}

int p9_fs::close(fs_inode *inodep) {
	if (p9ClientInit() == JFAIL) return JFAIL;
    return  p9Request(REQUEST_CLOSE, inodep, 0, 0, 0, 0, 0);
}

int p9_fs::setattr(fs_inode *inodep,uint64_t size) {
	p9ClientInit();
    return  p9Request(REQUEST_SETATTR, inodep, size, 0, 0, 0, 0);
}
int p9_fs::unmount(){
	int i;
	for (i = 0; i < MAX_P9_FILES; i++) {
		if (client.files[i].fid != 0) {
			p9_close(client.files[i].fid);
		}
		ut_memset((unsigned char *)&client.files[i],0,sizeof(p9_file_t));
	}
	p9_close(client.root_fid);
	client.root_fid = 0;
	ut_log("p9 UNmount\n");
	return JSUCCESS;
}
void p9_fs::print_stat(){

}
void p9_fs::set_mount_pnt(unsigned char *mnt_pnt){
/*TODO: nothing todo in p9fs */
}
static class p9_fs *p9_fs_obj;
extern "C"{
int p9_initFs(void *p9driver) {
//	p9ClientInit(); /* TODO need to include here */

	p9_fs_obj = jnew_obj(p9_fs);
	fs_registerFileSystem(p9_fs_obj,(unsigned char *)"p9_fs");
	p9_dev = p9driver;

	return 1;
}
}
