/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/file.cc
*/
/*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*        file--- vinode->fs_inode->(file,dir)
         file--- vinode->socket
         file--- vinode->pipe
         file--- vinode->device
*/
#define JFS 1
#if JFS
#include "file.hh"
extern int epoll_close(struct file *filep);
extern "C" {
struct page *pc_getInodePage(struct fs_inode *inode, unsigned long offset);
int pc_insertPage(struct fs_inode *inode, struct page *page);
#define OFFSET_ALIGN(x) ((x/PC_PAGESIZE)*PC_PAGESIZE) /* the following 2 need to be removed */

/**************************************** global variables ***********************************/
LIST_HEAD(fs_inode_list);
void *g_inode_lock = 0; /* protects inode_list */
kmem_cache_t *g_slab_filep;
}

typedef struct {
	struct filesystem *fs;
	unsigned char *mount_pnt;
	unsigned char *type;
	unsigned char *device_name;
} filesystem_list_t;
int g_conf_root_dev_scsi  __attribute__ ((section ("confdata")))=1;
filesystem_list_t g_fs_list[]={
		{0,"/","tar_fs",0},  /* Make the root mount as the first entry, do not repeat the mount point entries */
		{0,"/p9","p9_fs",0},
		{0,"/proc/","proc",0},
		{0,"/data","tar_fs",0},
		{0,0,0,0}
};
kmem_cache_t *fs_inode::slab_objects = 0;

void vinode::update_stat_in(int in_req,int in_byte){
	if (in_byte <= 0) return;
	stat_in = stat_in+in_req;
	stat_in_bytes = stat_in_bytes+in_byte;
}
void vinode::update_stat_out(int out_req,int out_byte){
	if (out_byte <= 0) return;
	stat_out = stat_out+out_req;
	stat_out_bytes = stat_out_bytes+out_byte;
}
/******************************************** fs_inode class **********************************/

fs_inode::fs_inode(uint8_t *arg_filename, unsigned long mode, struct filesystem *arg_vfs) {
	int i;

	//ut_log("Filname in CONSTRUCTOR : %s \n",arg_filename);
	count.counter = 1;
	nrpages = 0;
	stat_locked_pages.counter = 0;

	flags = INODE_LONGLIVED;
	fileStat.st_size = 0;
	fs_private = 0;
	fileStat.inode_no = 0;
	fileStat_insync = 0;
	stat_last_offset=0;
	read_ahead_offset=0;
	hard_links=0;
	open_mode = mode;
	vfs = arg_vfs;
	ut_strcpy(filename, (uint8_t *) arg_filename);
	for (i = 0; i < PAGELIST_HASH_SIZE; i++){
		INIT_LIST_HEAD(&(page_list[i]));
	}
	INIT_LIST_HEAD(&(vma_list));
	INIT_LIST_HEAD(&(inode_link));
	DEBUG(" inode init filename:%s: :%x  :%x \n", filename, &page_list, &(page_list));

	mutexLock(g_inode_lock);
	list_add(&inode_link, &fs_inode_list);
	mutexUnLock(g_inode_lock);
}
extern wait_queue *read_ahead_waitq;
extern "C" {
unsigned long g_stat_readahead_miss;
}
struct page *fs_inode::fs_genericRead(unsigned long offset, int read_ahead) {
	struct page *page;
	int tret;
	int len;
	int err = 0;

	retry_again: /* the purpose to retry is to get again from the page cache with page count incr */
	page = pc_getInodePage(this, offset);
	if (page == NULL) {
		//ut_log(" Trying to insert file: %x offset :%x pid:%x\n",this,offset,g_current_task->pid);
		page = pc_getFreePage(0);
		if (page == NULL) {
			err = -3;
			goto error;
		}
		page->offset = OFFSET_ALIGN(offset);
		assert(page->magic_number == PAGE_MAGIC);
		int len = PC_PAGESIZE;
		if (read_ahead){
			page->fs_inode = this;
			pc_get_page(page);   /* this page will reside in driver for some time */
			PageSetReadinProgress(page);
			len=0;  /* for the fs that does not support read-ahead let it fail */
		}
		tret = this->vfs->read(this, page->offset, pcPageToPtr(page),
				len,read_ahead);

		if (tret > 0) {
			if (pc_insertPage(this, page) == JFAIL) {
				if (read_ahead){ /* wait till the page is released from reahead driver */
					while(PageReadinProgress(page)){
						sc_sleep(2);
					}
				}
				pc_putFreePage(page);
				err = -5;
				page = pc_getInodePage(this, offset);
				if (page != NULL){ /* some other thread got this page */
					err=0;
				}
				goto error;
			}else{
				if ((tret >= 0) && (tret < PC_PAGESIZE)){
					ut_memset(pcPageToPtr(page)+tret,0,PC_PAGESIZE-tret);
				}
			}
			if ((tret + page->offset) > this->fileStat.st_size){
				//ut_printf(" FILE size changed from  %d to %d ret:%d page offset:%d readoffset:%d\n",this->fileStat.st_size,(tret + offset),tret,page->offset,offset);
				this->fileStat.st_size = offset + tret;
			}
		} else {
			if (read_ahead){
				pc_put_page(page);
				PageClearReadinProgress(page);
				pc_putFreePage(page);
				return 0;
			}
			pc_putFreePage(page);
			err = -4;
			goto error;
		}
		goto retry_again;
	}else{
		//read_ahead=0;  /* got from the caceh no impact on readahead */
	}

	error: if (err < 0) {
		ut_log(" Error in reading the file :%i :%x read_ahead:%d tret:%x offset:%d, roffset:%d\n", -err,page,read_ahead,tret,offset,read_ahead_offset);
		page = 0;
		//BUG();
	}
	if (page && PageReadinProgress(page)){
		if (read_ahead == 0){
			g_stat_readahead_miss++;
			while(PageReadinProgress(page)){
				read_ahead_waitq->wait(5);
			}
			return page;
		}else{
			pc_put_page(page);
			return 0;
		}
	}
	return page;
}
extern "C" {
// TODO: fix need to read partial pages: if disk_blk_side is 512  for pvscsi
int g_conf_read_ahead_pages  __attribute__ ((section ("confdata")))= 0;  /* TODO disabling readahed:  tar fs is giving wrong pages , so disabling */
int g_conf_print_cksum =0;
}

void print_cksum(unsigned long offset, unsigned char *data, int len){
	int i;
	unsigned long cksum=0;
	if (g_conf_print_cksum ==0 ) return;
	if (offset > 20480) return;
	for (i=0; i<len; i++){
		cksum = cksum +data[i];
	}
	ut_printf(" offset:%d cksum :%x \n",offset,cksum);
	return;
}
int fs_inode::read(unsigned long offset, unsigned char *data, int len, int unused_read_flags, int opt_flags) {
	struct page *page;
	int i;

	page = this->fs_genericRead(offset,0);
	if (page == 0) {
		return 0;
	}

	int ret = PC_PAGESIZE;
	ret = ret - (offset - OFFSET_ALIGN(offset));
	if (ret > len)
		ret = len;
	if ((offset + ret) > fileStat.st_size) {
		int r;
		r = fileStat.st_size - offset;
		if (r < ret)
			ret = r;
	}
	if (page > 0 && ret > 0) {
		ut_memcpy(data, pcPageToPtr(page) + (offset - OFFSET_ALIGN(offset)),
				ret);
		print_cksum(offset,pcPageToPtr(page),PC_PAGESIZE);
		DEBUG(" memcpy :%x %x  %d \n", buff, pcPageToPtr(page), ret);
	}
	pc_put_page(page);
	if (ret > 0){ /* read some more pages a head asyncronously */
		for (i=1; i<g_conf_read_ahead_pages; i++){
			if (offset+(i*PC_PAGESIZE)  <  read_ahead_offset) continue;
			page = this->fs_genericRead(offset+(i*PC_PAGESIZE),1);
			if (page){
				pc_put_page(page);
			}
			read_ahead_offset=offset+(i*PC_PAGESIZE);
		}
	}
	return ret;
}
int fs_inode::write(unsigned long offset, unsigned char *data, int len, int wr_flags) {
	int ret, tmp_len;
	int size, page_offset;
	struct page *page;


	if (file_type == DIRECTORY_FILE) { //TODO : check for testing
		BUG();
	}
#if 0
	if ( offset > (fileStat.st_size))
		return -1;
#endif
	ret = 0;
	tmp_len = 0;
	while (tmp_len < len) {
		try_again: page = pc_getInodePage(this, offset);
		if (page == NULL) {
			page = pc_getFreePage(1);
			if (page == NULL) {
				ret = -3;
				goto error;
			}
			page->offset = OFFSET_ALIGN(offset);
			if (pc_insertPage(this, page) == JFAIL) {
				pc_putFreePage(page);
				ut_log("Fail to insert page \n");
				ret = -1;
				goto error;
			} else {
				goto try_again;
			}
		}
		size = PC_PAGESIZE;
		if (size > (len - tmp_len))
			size = len - tmp_len;
		page_offset = offset - page->offset;
		if ((page_offset < 0 || page_offset > PC_PAGESIZE)
				|| size < 0|| size> PC_PAGESIZE) {
			ut_log("ERROR in write: offset:%x page_offset:%x diff:%x \n",
					offset, page->offset, page_offset);
			while (1)
				;
		}
		ut_memcpy(pcPageToPtr(page) + page_offset, data + tmp_len, size);
		pc_pageDirted(page);
		pc_put_page(page);

		offset = offset + size;

		if (fileStat.st_size < offset)
			fileStat.st_size = offset;
		tmp_len = tmp_len + size;
		if (pc_is_freepages_available() == 0) {
			pc_housekeep();
			//inode_sync(inode,0);
		}
		DEBUG("write memcpy :%x %x  %d \n", buff, pcPageToPtr(page), size);
	}
	ret = tmp_len;
	error: return ret;
}
int fs_inode::close() {
	int ret = JFAIL;

	mutexLock(g_inode_lock);

	atomic_dec(&this->count);
	if ( this->nrpages == 0 && this->count.counter <= 0) {
		list_del(&this->inode_link);
		ret = JSUCCESS;
	} else {
		if (this->count.counter == 1) {
			this->flags = this->flags & (~INODE_EXECUTING);
		}
	}
	mutexUnLock(g_inode_lock);

	if (ret == JSUCCESS){
		//mm_slab_cache_free(fs_inode::slab_objects, this);
		jfree_obj((unsigned long)this);
	}

	return ret;
}
int fs_inode::ioctl(unsigned long arg1,unsigned long arg2) {
	int ret = JFAIL;
	if (arg1 == IOCTL_FILE_UNLINK){
		ret = vfs->remove(this);
	}
	return ret;
}
void fs_inode::print_stats(unsigned char *arg1,unsigned char *arg2){

}
unsigned long fs_registerFileSystem(filesystem *fs,unsigned char *fs_type, unsigned char *device_name) {
	int i=0;
	if (fs_type == 0) return JFAIL;
	INIT_LOG("		Registering files system of type :%s: \n",fs_type);

	while(g_fs_list[i].type != 0){
		if (i==0 && g_conf_root_dev_scsi==0 &&  ut_strcmp(device_name,"pvscsi")==0){
			i++;
			continue;
		}
	/*	if (i==0 &&  ut_strcmp(device_name,"ramdisk")==0){
			i++;
			continue;
		}*/
		if (g_fs_list[i].fs==0 &&  ut_strcmp(g_fs_list[i].type,fs_type)==0){
			g_fs_list[i].fs = fs;
			g_fs_list[i].device_name = device_name;
			INIT_LOG("		file system %s  mounted @ %s\n",g_fs_list[i].type,g_fs_list[i].mount_pnt);
			fs->set_mount_pnt(g_fs_list[i].mount_pnt);
			return JSUCCESS;
		}
		i++;
	}

	return JFAIL;
}

/***************************************************************************************/
extern "C" {
int Jcmd_mount(uint8_t *arg1, uint8_t *arg2){
	int i=0;

	while(g_fs_list[i].type != 0){
		if (g_fs_list[i].fs != 0){
			ut_printf(" type: %s-> :%s devicename:%s read_bytes: %xK read reqs:%x rerr:%x  disk_size:%d fs_size:%d",g_fs_list[i].type,g_fs_list[i].mount_pnt, g_fs_list[i].device_name, g_fs_list[i].fs->stat_byte_reads/1000,
					g_fs_list[i].fs->stat_read_req, g_fs_list[i].fs->stat_read_errors ,g_fs_list[i].fs->device_size,g_fs_list[i].fs->filesystem_size);
			ut_printf("write_bytes: %xK write reqs:%x werr:%x\n",g_fs_list[i].fs->stat_byte_writes/1000,g_fs_list[i].fs->stat_write_req, g_fs_list[i].fs->stat_write_errors );
			if (arg1 != 0 && i!=0){
				g_fs_list[i].fs->print_stat();
			}
		}
		i++;
	}
	return 1;
}
#if 0
int Jcmd_unmount(uint8_t *arg1, uint8_t *arg2) {
	struct fs_inode *tmp_inode;
	struct list_head *p;
	struct filesystem *vfs_fs = gvfs_fs;

	vfs_fs->unmount();
	mutexLock(g_inode_lock);
	list_for_each(p, &fs_inode_list) {
		tmp_inode = list_entry(p, struct fs_inode, inode_link);
		tmp_inode->fs_private = 0;
	}
	mutexUnLock(g_inode_lock);

	return JSUCCESS;
}
#endif
static void inode_sync(struct fs_inode *inode, unsigned long truncate) {
	struct page *page;
	int ret;
	unsigned long offset;
	struct filesystem *vfs_fs = inode->vfs;

	if (truncate) {
		ret = vfs_fs->setattr(inode, 0);
		fs_fadvise(inode, 0, 0, POSIX_FADV_DONTNEED);
		return;
	}

	for (offset = 0; offset < inode->fileStat.st_size;
			offset = offset + PAGE_SIZE) {
		page = pc_getInodePage(inode, offset);
		if (page == NULL)
			continue;
		if (PageDirty(page)) {
			uint64_t len = inode->fileStat.st_size;
			if (len < (page->offset + PC_PAGESIZE)) {
				len = len - page->offset;
			} else {
				len = PC_PAGESIZE;
			}
			if (len > 0) {
				assert(page->magic_number == PAGE_MAGIC);
				ret = vfs_fs->write(inode, page->offset, pcPageToPtr(page),
						len);
				if (ret == len) {
					pc_pagecleaned(page);
				}
			}
		}
		pc_put_page(page);
	}
}
static struct filesystem *transform_filename(uint8_t *arg_filename, uint8_t *filename) {
	int i, len;
	struct filesystem *vfs=0;
	filename[0] = 0;
	if (arg_filename[0] != '/') {
		ut_strcpy((uint8_t *) filename,
				(const uint8_t *) g_current_task->fs->cwd);
	}
	i = 0;

	/* remove "./" from input file */
	if (arg_filename[0] == '.' && arg_filename[1] == '/')
		i = 2;

	len = ut_strlen((const uint8_t *) filename);
	if (filename[len - 1] != '/') {
		ut_strcat((uint8_t *) filename, (uint8_t *) "/");
	}
	ut_strcat((uint8_t *) filename, &arg_filename[i]); //TODO use strncat
//BRK;
	int k=1;
	vfs=g_fs_list[0].fs;
	while(g_fs_list[k].type != 0){
		if (ut_strstr(filename,g_fs_list[k].mount_pnt)!=0  && g_fs_list[k].fs!=0){
			vfs=g_fs_list[k].fs;
			break;
		}
		k++;
	}

	len = ut_strlen((const uint8_t *) filename);

	/* remove "/" as last character */
	if (filename[len - 1] == '/') {
		filename[len - 1] = 0;
	}

	DEBUG(" opening in :%s:  transformfile :%s: \n", arg_filename, filename);
	return vfs;
}
unsigned long fs_vinode_close(void *arg) {
	struct fs_inode *inode = (struct fs_inode *) arg;
	return inode->close();
}

static fs_inode *remove_duplicate_inodes(fs_inode *new_inode) {
	fs_inode *tmp_inode;
	struct list_head *p;
	hard_link_t *hard_link;
	int found = 0;

	mutexLock(g_inode_lock);
	list_for_each(p, &fs_inode_list) {
		tmp_inode = list_entry(p, struct fs_inode, inode_link);
		if (tmp_inode == new_inode)
			continue;
		if ((new_inode->fileStat.inode_no == tmp_inode->fileStat.inode_no) && (new_inode->vfs == tmp_inode->vfs)) {
			hard_link = tmp_inode->hard_links;

			while (hard_link != 0) {
				if (ut_strcmp(hard_link->filename, new_inode->filename) == 0) {
					found = 1;
					// TODO: need to undo ths at the timeof closing  hard_link->count.counter++;
					break;
				}
				hard_link = hard_link->next;
			}
			if (found == 0) {
				hard_link = (hard_link_t *) ut_malloc(sizeof(hard_link_t));
				ut_strncpy(hard_link->filename, tmp_inode->filename,
						MAX_FILENAME);
				hard_link->next = tmp_inode->hard_links;
				hard_link->count.counter = 1;
				tmp_inode->hard_links = hard_link;
				atomic_inc(&new_inode->count);
			}
			//tmp_inode->vfs->close(new_inode);
			new_inode->count.counter = 0;
			if (new_inode->close() == JFAIL) {
				BUG();
			}
			new_inode = tmp_inode;
			break;
		}
	}
	mutexUnLock(g_inode_lock);
	return new_inode;
}
static struct fs_inode *fs_getInode(uint8_t *arg_filename, int flags,
		int mode) {
	fs_inode *ret_inode, *tmp_inode;
	struct list_head *p;
	int ret;
	uint8_t filename[MAX_FILENAME];
	struct filesystem *vfs=0;

	ret_inode = 0;
	vfs=transform_filename(arg_filename, filename);
	if (vfs == 0){
		return 0;
	}

	mutexLock(g_inode_lock);
	list_for_each(p, &fs_inode_list) {
		if (tmp_inode == ret_inode)
			continue;

		ret_inode = list_entry(p, struct fs_inode, inode_link);
		if (ut_strcmp((uint8_t *) filename, ret_inode->filename) == 0) {
			atomic_inc(&ret_inode->count);
			mutexUnLock(g_inode_lock);
			goto last;
		}
	}
	mutexUnLock(g_inode_lock);
#if 1
	//ret_inode = (fs_inode *) mm_slab_cache_alloc(fs_inode::slab_objects, 0);
	ret_inode = jnew_obj(fs_inode, filename, mode & 0x3, vfs);

	if (ret_inode != 0) {
		struct fileStat stat;

		struct filesystem *vfs_fs = ret_inode->vfs;
		if (vfs_fs ==0){
			ret_inode = 0;
			goto last;
		}
		ret = vfs_fs->open(ret_inode, flags, mode & 0x3);
		if (ret == JFAIL) {
			ret_inode->count.counter = 0;
			if (ret_inode->close() == JFAIL) {
				BUG();
			}
			ret_inode = 0;
			goto last;
		}
		DEBUG(" filename:%s: %x\n", ret_inode->filename, ret_inode);
		if (vfs_fs->stat(ret_inode, &stat) == JSUCCESS) { /* this is to get inode number */
			ret_inode->fileStat.inode_no = stat.inode_no;

			mutexLock(g_inode_lock);
			list_for_each(p, &fs_inode_list) {
				tmp_inode = list_entry(p, struct fs_inode, inode_link);
				if (tmp_inode == ret_inode)
					continue;
				if (ret_inode->fileStat.inode_no == tmp_inode->fileStat.inode_no
						&& ut_strcmp(ret_inode->filename, tmp_inode->filename)
								== 0) {
					ret_inode->count.counter = 0;
					if (ret_inode->close() == JFAIL) {
						BUG();
					}
					ret_inode = tmp_inode;
				}
			}
			mutexUnLock(g_inode_lock);
			ret_inode = remove_duplicate_inodes(ret_inode);
		} else {
			//if (vfs_fs != procfs_fs)
				ut_printf("ERROR: cannot able to get STAT\n");
				goto last;
		}

		atomic_inc(&ret_inode->count);
	}
#endif

last:
	if (ret_inode != 0 && ret_inode->fs_private == 0) {
		struct filesystem *vfs_fs = ret_inode->vfs;
		vfs_fs->open(ret_inode, flags, mode & 0x3);
	}
	return ret_inode;
}
struct file *fs_open(uint8_t *filename, int flags, int mode) {
	struct file *filep;
	struct fs_inode *inodep;

	if ( filename == 0)
		return 0;

	filep = (struct file *) mm_slab_cache_alloc(g_slab_filep, 0);
	if (filep == 0)
		goto error;
	if (filename != 0) {
		ut_strcpy(filep->filename, filename);
	} else {
		goto error;
	}
	/* special files handle here before going to regular files */
	if (ut_strcmp((uint8_t *) filename, (uint8_t *) "/dev/sockets") == 0) {
		filep->vinode = socket::create_new(mode);
		if (filep->vinode == NULL)
			goto error;
		filep->offset = 0;
		filep->type = NETWORK_FILE;
		return filep;
	} else {
		filep->type = REGULAR_FILE;
	}

	inodep = fs_getInode((uint8_t *) filep->filename, mode, flags);
	if (inodep == 0)
		goto error;

	filep->vinode = inodep;
	inodep->read_ahead_offset = 0;
	if (flags & O_APPEND) {
		filep->offset = inodep->fileStat.st_size;
	} else {
		filep->offset = 0;
	}
	if (flags & O_CREAT) {
		if ((inodep->fileStat.st_size > 0) && (filep->offset == 0)) {
			inode_sync(inodep, 1); /* truncate the file */
		}
	}
	filep->flags = flags;
	return filep;

error:
	if (filep != NULL)
		mm_slab_cache_free(g_slab_filep, filep);
	if (inodep != NULL) {
		inodep->close();
	}
	return 0;
}


int fs_read(struct file *filep, uint8_t *buff, unsigned long len) {
	int ret;

	ret = 0;
	if (filep == 0)
		return 0;

	if (ar_check_valid_address((addr_t) buff, len) == JFAIL) {
		BUG();
	}
	if (buff == 0 || len == 0) return 0;
	if (filep->type == OUT_FILE || filep->type == OUT_PIPE_FILE || filep->type==EVENT_POLL_FILE) {
		ut_log(" ERROR: read on OUT_FILE : name: %s type:%d\n", g_current_task->name,filep->type);
		//return -1;
		BUG();
	}

	DEBUG("Read filename from hs  :%s: offset:%d inode:%x buff:%x len:%x \n", filep->filename, filep->offset, filep->inode, buff, len);
	vinode *vinode = (struct vinode *) filep->vinode;
	//TODO 	if (inode->length <= filep->offset) return 0;
	if (vinode == 0){
		BUG();
	}
	//ut_log("VFS: read len :  %d  \n",len);
	ret = vinode->read(filep->offset, buff, len,filep->flags,0);
	if (ret > 0) {
		filep->offset = filep->offset + ret;
	}
	last: return ret;
}
int fs_write(struct file *filep, uint8_t *buff, unsigned long len) {
	int i, ret;

	ret = 0;
	if (filep == 0)
		return 0;

	if (ar_check_valid_address((unsigned long) buff, len) == JFAIL) {
		BUG();
	}
	if (filep->type == DEV_NULL_FILE) {
		return len;
	}
	struct vinode *inode = (struct vinode *) filep->vinode;
	//if (inode ==0 || !(inode->file_type&filep->type) ){
	if (inode ==0){
		return 0;
		//BUG();
	}


	DEBUG("Write  filename from hs  :%s: offset:%d inode:%x \n", filep->filename, filep->offset, filep->inode);
	ret = inode->write(filep->offset, buff, len,0);
	if (ret < 0) {
		ut_log(" fs_write fails error:%x pid:%d \n", ret, g_current_task->task_id);
		return 0;
	}
	filep->offset = filep->offset+ret;
	return ret;
}
struct file *fs_dup(struct file *old_filep, struct file *new_filep) {

	if (old_filep == 0)
		return 0;
	if (new_filep == 0) {
		new_filep = (struct file *) mm_slab_cache_alloc(g_slab_filep, 0);
	}

	/* 1. close the resources of  new */
	if (new_filep->type == REGULAR_FILE || new_filep->type == NETWORK_FILE || new_filep->type == OUT_PIPE_FILE
			 || new_filep->type == IN_PIPE_FILE) {
		struct vinode *vinode=(struct vinode *)new_filep->vinode;
		if (vinode ==0){
			BUG();
		}
		vinode->close();
	}

	/* 2. copy from old */
	ut_memcpy((uint8_t *) new_filep, (uint8_t *) old_filep,
			sizeof(struct file));
	if ((new_filep->type == REGULAR_FILE || new_filep->type == NETWORK_FILE)
			&& new_filep->vinode != 0) {
		struct fs_inode *ninode = (struct fs_inode *) new_filep->vinode;
		atomic_inc(&ninode->count);
	} else if (new_filep->type == OUT_PIPE_FILE || new_filep->type == IN_PIPE_FILE) {
		struct vinode *ninode = (struct vinode *) new_filep->vinode;
		if (ninode == 0){
			BUG();
		}
		atomic_inc(&ninode->count);
		//fs_dup_pipe(new_filep);
	}
	return new_filep;
}
extern int socketpair_close(struct file *filep);
int fs_close(struct file *filep) {
	if (filep == 0 )
		return 0;
	if (filep->type == REGULAR_FILE || filep->type == OUT_PIPE_FILE || filep->type == IN_PIPE_FILE || filep->type == NETWORK_FILE) {
		if (filep->vinode != 0) {
			struct vinode *vinode= (struct vinode *)filep->vinode;
			vinode->close();
		}else{
			BUG();
		}
	}  else if ((filep->type == OUT_FILE) || (filep->type == IN_FILE)
			|| (filep->type == DEV_NULL_FILE)) { /* nothing todo */
		//ut_log("Closing the IO file :%x name :%s: \n",filep,g_current_task->name);
	} else if (filep->type == EVENT_POLL_FILE){
		epoll_close(filep);
	} else if (filep->type == SOCKETPAIR_FILE){
		socketpair_close(filep);
	}
	filep->vinode = 0;
	mm_slab_cache_free(g_slab_filep, filep);
	return SYSCALL_SUCCESS;
}
/*********************************************************************************************/

int init_vfs(unsigned long unused_arg) {
	int i;
	g_slab_filep = kmem_cache_create((const unsigned char *) "file_struct",
			sizeof(struct file), 0, 0, 0, 0);
	fs_inode::slab_objects = kmem_cache_create(
			(const unsigned char *) "fs_vinodes", sizeof(struct fs_inode), 0,
			JSLAB_FLAGS_DEBUG, 0, 0);

	socket::udp_list.size =0;
	socket::tcp_listner_list.size=0;
	socket::tcp_connected_list.size=0;
	read_ahead_waitq = jnew_obj(wait_queue, "read_ahead_waitq", 0); /* all thread waiting on read_ahead will wait on this */

	return JSUCCESS;
}
/************************************************************************************/

int fs_sync() {
	struct fs_inode *tmp_inode;
	struct list_head *p;
	struct fileStat stat;

	fs_data_sync(0);

	restart:

	/* sync the file system with host */
	mutexLock(g_inode_lock);
	list_for_each(p, &fs_inode_list) {
		tmp_inode = list_entry(p, struct fs_inode, inode_link);
		struct filesystem *vfs_fs = tmp_inode->vfs;
		if (vfs_fs->stat(tmp_inode, &stat) != JSUCCESS) {
			if (tmp_inode->close()==JSUCCESS) {
				mutexUnLock(g_inode_lock);
				goto restart;
			}
		}
	}
	mutexUnLock(g_inode_lock);

	return SYSCALL_SUCCESS;
}
unsigned long fs_fadvise(void *inode_arg, unsigned long offset,
		unsigned long len, int advise) {
	struct page *page;
	struct fs_inode *inode = (struct fs_inode *) inode_arg;
	int ret;
	int count = 0;

	//TODO : implementing other advise types
	if (advise == POSIX_FADV_DONTNEED && len == 0) {
		while (offset < inode->fileStat.st_size) /* delete all the pages in the inode */
		{
			ret = JFAIL;

			page = pc_getInodePage(inode, offset);
			offset = offset + PAGE_SIZE;
			if (page == 0)
				continue;

			mutexLock(g_inode_lock);
			pc_put_page(page);
			if (page->count.counter > 0) { //TODO: need to handle this case, this will happen for paralle write into same file
				//ut_log(" ERROR in Truncate the file: page is in Use: %d \n",page->count.counter);
				while (1)
					;
			}
			ret = pc_deletePage(page);
			mutexUnLock(g_inode_lock);

			if (ret == JSUCCESS) {
				count++;
			}
		}
	}
	return count;
}
unsigned long fs_lseek(struct file *file, unsigned long offset, int whence) {

	if (file == 0)
		return 0;
	struct fs_inode *inode = (struct fs_inode *) file->vinode;
	if (inode ==0){
		BUG();
	}
	if (inode->vfs ==0){
		BUG();
	}

	return inode->vfs->lseek(file, offset, whence);
}
unsigned long fs_fdatasync(struct file *filep) {
	struct fs_inode *inode;

	if ( filep == 0)
		return 0;

	inode = (struct fs_inode *) filep->vinode;
	inode_sync(inode, 0);

	return 0;
}
int fs_stat(struct file *filep, struct fileStat *stat) {
	int ret;
	struct fs_inode *inode = (struct fs_inode *) filep->vinode;

	if ( filep == 0)
		return 0;
	if (inode && inode->fileStat_insync == 1) {
		DEBUG(" stat filename :%s: \n", filep->filename);
		ret = JSUCCESS;
		ut_memcpy((uint8_t *) stat, (uint8_t *) &(inode->fileStat),
				sizeof(struct fileStat));
	} else {
		//BRK;
		if (inode ==0){
			BUG();
		}
		struct filesystem *vfs_fs = inode->vfs;
		if (filep->vinode!=0 && inode->vfs!=0)
			vfs_fs = inode->vfs;
		if (vfs_fs == 0){
			return 0;
			//BUG();
		}
		ret = vfs_fs->stat(filep->vinode, stat);
		if (ret == JSUCCESS) {
			stat->st_size = inode->fileStat.st_size;
//TODO
		}
	}
	return ret;
}
unsigned long fs_readdir(struct file *file, struct dirEntry *dir_ent, int len,
		int *offset) {

	if (file == 0)
		return 0;
	struct fs_inode *inode = (struct fs_inode *) file->vinode;
	if (file->vinode && inode->file_type != DIRECTORY_FILE) {
		ut_log("ERROR: SYS_getdents  file type error\n");
		return -1;
		//BUG();
	}
	if (inode==0){
		return  -1;
	}
	struct filesystem *vfs_fs = inode->vfs;
	return vfs_fs->readDir(file->vinode, dir_ent, len, offset);
}
int fs_data_sync(int num_pages) {
	int ret;
	page_struct_t *page;
	int write_count = 0;

	while (1) {
		page = pc_get_dirty_page();
		if (page == 0) {
			goto last;
		}
		struct fs_inode *inode = (struct fs_inode *) page->fs_inode;
		struct filesystem *vfs_fs = inode->vfs;
		uint64_t len = inode->fileStat.st_size;
		if (len < (page->offset + PC_PAGESIZE)) {
			len = len - page->offset;
		} else {
			len = PC_PAGESIZE;
		}
		if ((len > PC_PAGESIZE) || (len < 0)) {
			ut_log(" Error in data_sync len:%x  offset:%x inode_len:%x \n", len,
					page->offset, inode->fileStat.st_size);
			len = PC_PAGESIZE;
		}
		assert(page->magic_number == PAGE_MAGIC);
		ret = vfs_fs->write(page->fs_inode, page->offset, pcPageToPtr(page),
				len);
		if (ret == len) {
			pc_pagecleaned(page);
			write_count++;
		}
		pc_put_page(page);
		if (num_pages > 0 && write_count > num_pages) {
			goto last;
		}
	}
	last: return write_count;
}
unsigned long fs_getVmaPage(struct vm_area_struct *vma, unsigned long offset) {
	struct page *page;
	fs_inode *inode;
	unsigned long ret;

	inode = (fs_inode *) vma->vm_inode;
	page = inode->fs_genericRead(offset,0);
	if (page == NULL)
		return 0;
	pc_put_page(page); /* The file will be marked as executable, the pages will be locked at the file level */

	ret = (unsigned long) pc_page_to_ptr(page);
	ret = __pa(ret);
	//DEBUG(
	//		" mapInodepage phy addr :%x  hostphyaddr:%x offset:%x diff:%x \n", ret, g_hostShmPhyAddr, offset, (to_ptr(page)-pc_startaddr));
	return ret; /* return physical address */
}

}
#endif
