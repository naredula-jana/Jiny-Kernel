#define DEBUG_ENABLE 1
#include "vfs.h"
#include "mm.h"
#include "common.h"
#include "task.h"
#include "interface.h"
#include "9p.h"

#define OFFSET_ALIGN(x) ((x/PC_PAGESIZE)*PC_PAGESIZE) /* the following 2 need to be removed */

static p9_client_t client;
static int p9ClientInit() {
	static int init = 0;
	uint32_t msg_size;
	int ret;
	unsigned char version[200];
	unsigned long addr,i;

	if (init != 0)
		return 1;

	client.pkt_buf = (unsigned char *) mm_getFreePages(MEM_CLEAR, 0);
	client.pkt_len = 4098;
	init = 1;

	client.type = P9_TYPE_TVERSION;
	client.tag = 0xffff;

	addr = p9_write_rpc(&client, "ds", 0x2040, "9P2000.u");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "ds", &msg_size, version);
	}
	DEBUG("New cmd:%x size:%x version:%s \n",ret, msg_size,version);

	client.type = P9_TYPE_TATTACH;
	client.tag = 0x13;
	client.root_fid = 132;
	addr = p9_write_rpc(&client, "ddss", client.root_fid, ~0, "jana", "");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
	}
	for (i=0; i<MAX_P9_FILES; i++) {
		client.files[i].fid=0;
	}
	client.next_free_fid = client.root_fid+1;
	return 1;
}
static int p9_open(uint32_t fid, int mode) {
	unsigned long addr;
	uint8_t mode_b;
	int ret;

	client.type = P9_TYPE_TOPEN;
	if (mode ==1) mode_b=1;
	addr = p9_write_rpc(&client, "db", fid, mode_b);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
	}
	return ret;
}
static uint32_t p9_walk(unsigned char *filename) {
	unsigned long addr;
	int ret;
	int i, empty_fid = -1;

	for (i = 0; i < MAX_P9_FILES; i++) {
		if (client.files[i].fid != 0) {
			if (ut_strcmp(client.files[i].name, filename) == 0) {
				return client.files[i].fid;
			}
			continue;
		}
		if (empty_fid == -1)
			empty_fid = i;
	}
	client.type = P9_TYPE_TWALK;
	addr = p9_write_rpc(&client, "ddws", client.root_fid, client.next_free_fid, 1, filename);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
		if (empty_fid != -1) {
			client.files[empty_fid].fid = client.next_free_fid;
			client.next_free_fid++; /* TODO : there will be collision , the logic need to replaced with better one */
			ut_strcpy(client.files[empty_fid].name, filename);
			return client.files[empty_fid].fid;
		}
	}
	return 0;
}

static uint32_t p9_read(uint32_t fid, uint64_t offset, unsigned char *data, uint32_t data_len) {
	unsigned long addr;
	int ret;
	uint32_t read_len;

	client.type = P9_TYPE_TREAD;
	client.user_data = data;
	client.userdata_len = data_len;

	addr = p9_write_rpc(&client, "dqd", fid, offset, data_len);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "d", &read_len);
	}
	data[10] = 0;
	DEBUG("read len :%d  new DATA  :%s:\n",read_len,data);
	return read_len;
}

static uint32_t p9_write(uint32_t fid, uint64_t offset, unsigned char *data, uint32_t data_len) {
	unsigned long addr;
	int ret;
	uint32_t write_len;
	unsigned char *rd;

	client.type = P9_TYPE_TWRITE;
	client.user_data = data;
	client.userdata_len = data_len;

	addr = p9_write_rpc(&client, "dqd", fid, offset, data_len);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "d", &write_len);
	}
	data[10] = 0;
	rd = client.pkt_buf+1024+10;
	 rd[20]='\0';
	DEBUG("write len :%d  write_rpc ret:%d : \n",data_len,ret);
	return write_len;
}

struct filesystem p9_fs;
static int p9Request(unsigned char type, struct inode *inode, uint64_t offset, unsigned char *data, int data_len, int mode) {
	uint32_t fid;

	if (inode == 0)
		return -1;
	if (type == REQUEST_OPEN) {
		fid = p9_walk(inode->filename);
		if (fid > 0) {
			inode->fs_private = fid;
			return p9_open(fid, mode);
		} else {

		}
	} else if (type == REQUEST_READ) {
		fid = inode->fs_private;
		return p9_read(fid, offset, data, data_len);
	} else if (type == REQUEST_WRITE) {
		fid = inode->fs_private;
		return p9_write(fid, offset, data, data_len);
	}
}

static struct file *p9Open(char *filename, int mode) {
	struct file *filep;
	struct inode *inodep;
	int ret;

	p9ClientInit();
	filep = kmem_cache_alloc(g_slab_filep, 0);
	if (filep == 0)
		goto error;
	if (filename != 0) {
		ut_strcpy(filep->filename, filename);
	} else {
		goto error;
	}

	inodep = fs_getInode(filep->filename);
	if (inodep == 0)
		goto error;
	if (inodep->fs_private == 0) /* need to get info from host  irrespective the file present, REQUEST_OPEN checks the file modification and invalidated the pages*/
	{
		ret = p9Request(REQUEST_OPEN, inodep, 0, 0, 0, mode);
		if (ret < 0)
			goto error;
		inodep->file_size = ret;
	}
	filep->inode = inodep;
	filep->offset = 0;
	return filep;
	error: if (filep != NULL)
		kmem_cache_free(g_slab_filep, filep);
	if (inodep != NULL) {
		fs_putInode(inodep);
	}
	return 0;
}
static int p9Lseek(struct file *filep, unsigned long offset, int whence) {
	filep->offset = offset;
	return 1;
}

static int p9Fdatasync(struct file *filep) {

	return 1;
}
static int p9Write(struct inode *inode, uint64_t offset, unsigned char *data, int data_len) {
	p9ClientInit();
    return  p9Request(REQUEST_WRITE, inode, offset, data, data_len, 0);
}

static int p9Read(struct inode *inode, uint64_t offset, unsigned char *data, int data_len) {
	p9ClientInit();
    return  p9Request(REQUEST_READ, inode, offset, data, data_len, 0);
}

static int p9Close(struct file *filep) {
	if (filep->inode != 0)
		fs_putInode(filep->inode);
	filep->inode = 0;
	kmem_cache_free(g_slab_filep, filep);
	return 1;
}

int p9_initFs() {
//	p9ClientInit(); /* TODO need to include here */
	p9_fs.open = p9Open;
	p9_fs.read = p9Read;
	p9_fs.close = p9Close;
	p9_fs.write = p9Write;
	p9_fs.fdatasync = p9Fdatasync;
	p9_fs.lseek = p9Lseek;
	fs_registerFileSystem(&p9_fs);

	return 1;
}
