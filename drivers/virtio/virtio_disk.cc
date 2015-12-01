/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/virtio_disk.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "virtio_pci.h"
#include "mach_dep.h"

}
/*****************************  Virtio Disk ********************************************/

#define DISK_READ 0
#define DISK_WRITE 1

#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

wait_queue *disk_thread_waitq;
static int virtio_disk_interrupt(void *private_data) {
	jdevice *dev;
	virtio_disk_jdriver *driver = (virtio_disk_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi_enabled == 0) {
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	}

	driver->stat_recv_interrupts++;
	if (driver->waitq != 0) {
		driver->waitq->wakeup();
	}
	if (disk_thread_waitq != 0) {
		disk_thread_waitq->wakeup();
	}
	return 0;
}

struct virtio_scsi_blk_req {
	/* out hdr-1, common to block and scsi */
	uint32_t type;
	uint32_t ioprio;
	uint64_t sector;
	/* out hdr-2, common to block and scsi */
	uint8_t scsi_cmd[100];

	/* in for read /out for write , common to block and scsi */
	uint8_t data[VIRTIO_BLK_DATA_SIZE]; /*TODO:  currently it made fixed, actually it is  variable size data here like data[][512]; */

	/* in hdr-1, common to block and scsi */
#define SCSI_SENSE_BUFFERSIZE 96
	uint8_t sense[SCSI_SENSE_BUFFERSIZE];

	/* in hdr-2, common to block and scsi */
	uint32_t errors;
	uint32_t data_len;
	uint32_t sense_len;
	uint32_t residual;

	/* in hdr-3, common to block and scsi */
	uint8_t status ;
	int len;
};

enum {
	SCSI_CMD_TEST_UNIT_READY = 0x00,
	SCSI_CMD_REQUEST_SENSE = 0x03,
	SCSI_CMD_INQUIRY = 0x12,
	SCSI_CMD_READ_16 = 0x88,
	SCSI_CMD_WRITE_16 = 0x8A,
	SCSI_CMD_READ_CAPACITY = 0x9E,
	SCSI_CMD_SYNCHRONIZE_CACHE_10 = 0x35,
	SCSI_CMD_SYNCHRONIZE_CACHE_16 = 0x91,
	SCSI_CMD_REPORT_LUNS = 0xA0,
};
struct cdb_readwrite_16 {
	uint8_t command;
	uint8_t flags;
	uint64_t lba;
	uint32_t count;
	uint8_t group_number;
	uint8_t control;
}__attribute__((packed));

static __inline uint32_t bswap32(uint32_t __x) {
	return (__x >> 24) | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000)
			| (__x << 24);
}
static __inline uint64_t bswap64(uint64_t __x) {
	return ((bswap32(__x) + 0ULL) << 32) | bswap32(__x >> 32);
}
static void req_construct(unsigned char *buf, unsigned long offset,
		unsigned long len) {
	struct cdb_readwrite_16 *req = (struct cdb_readwrite_16 *) buf;
	uint64_t lba;
	uint32_t count;

	lba = offset / 512;
	count = len / 512;
	req->lba = bswap64(lba);
	req->count = bswap32(count);
	req->command = SCSI_CMD_READ_16;
}
void *virtio_disk_jdriver::scsi_addBufToQueue(int type, unsigned char *buf_arg,
		uint64_t len, uint64_t sector, uint64_t data_len) {
#if 0
	struct virtqueue *tmp_vq = this->queues[0].send;
	struct virtio_scsi_blk_req *req;
	struct scatterlist sg[8];
	int ret;
	int transfer_len=len;
	int out,in;
	unsigned char *buf=0;
	ut_printf("Sending the SCSI request sector:%x len:%d  :%d \n",sector,len,data_len);
	if (buf == 0) {
		//buf = (unsigned char *) jalloc_page(0);
		buf = mm_getFreePages(0,1); /* TODO: for write request does not work, need to copy the data buf */
	}
	if (buf == 0 ) {
		BRK;
	}

	req = (struct virtio_scsi_blk_req *)buf;
	ut_memset(buf, 0, sizeof(struct virtio_scsi_blk_req));
	req->sector = sector;
	if (type == DISK_READ) {
		req->type = VIRTIO_BLK_T_SCSI_CMD;
		out = 2;
		in = 4;
	} else {
		req->type = VIRTIO_BLK_T_OUT;
		if (data_len>VIRTIO_BLK_DATA_SIZE) {
			ut_memcpy(buf+16, buf_arg, VIRTIO_BLK_DATA_SIZE);
		} else {
			ut_memcpy(buf+16, buf_arg, data_len);
		}
		out = 2;
		in = 1;

	}
	req->status = 0xff;
	int cmd_len=100;
	int scsi_sense_hdr = SCSI_SENSE_BUFFERSIZE;
	int resp_len=4*4; /* scsi response */
	if (transfer_len < blk_size) {
		transfer_len=blk_size;
	}

	sg[0].page_link = (unsigned long) buf;
	sg[0].length = 16;
	sg[0].offset = 0;

	req_construct(buf+16, sector*512, len );
	sg[1].page_link = (unsigned long) buf + 16;
	sg[1].length = 16;
	sg[1].offset = 0;

	sg[2].page_link = (unsigned long) (buf + 16 + cmd_len);
	sg[2].length = transfer_len;
	sg[2].offset = 0;

	sg[3].page_link = (unsigned long) (buf + 16 + cmd_len + VIRTIO_BLK_DATA_SIZE);
	sg[3].length = scsi_sense_hdr;
	sg[3].offset = 0;

	sg[4].page_link = (unsigned long) (buf + 16 + cmd_len + VIRTIO_BLK_DATA_SIZE + scsi_sense_hdr);
	sg[4].length = resp_len;
	sg[4].offset = 0;

	sg[5].page_link = (unsigned long) (buf + 16 + cmd_len + VIRTIO_BLK_DATA_SIZE + scsi_sense_hdr + resp_len);
	sg[5].length = 1;
	sg[5].offset = 0;

	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));

	virtio_disable_cb(tmp_vq);
	ret = virtio_add_buf_to_queue(tmp_vq, sg, out, in, (void *) sg[0].page_link, 0);/* send q */
	queue_kick(queues[0].send);

	virtio_enable_cb(tmp_vq);
	sc_sleep(2000);
	return 0; /* remove later */
#endif
	//return (void *)buf;
}
void virtio_disk_jdriver::print_stats(unsigned char *arg1,
		unsigned char *arg2) {
	queues[0].send->print_stats(0, 0);
}
struct virtio_blk_req *virtio_disk_jdriver::createBuf(int type,
		unsigned char *user_buf, uint64_t sector, uint64_t data_len) {
	unsigned char *buf = 0;
	struct virtio_blk_req *req;
	int donot_copy = 0;

	if (user_buf >= pc_startaddr && user_buf < pc_endaddr) {
		donot_copy = 1;
		buf = mm_getFreePages(0, 0);
		req = (struct virtio_blk_req *) buf;
		ut_memset(buf, 0, sizeof(struct virtio_blk_req));
		req->user_data = user_buf;
	} else {
		buf = mm_getFreePages(0, 1);
		req = (struct virtio_blk_req *) buf;
		ut_memset(buf, 0, sizeof(struct virtio_blk_req));
		req->user_data = 0;
	}

	req->sector = sector;
	if (type == DISK_READ) {
		req->type = VIRTIO_BLK_T_IN;
	} else {
		req->type = VIRTIO_BLK_T_OUT;
		if (donot_copy == 0) {
			ut_memcpy(&req->data[0], user_buf, data_len);
		}
	}
	req->status = 0xff;
	req->len = data_len;
	return req;
}
int virtio_disk_jdriver::MaxBufsSpace() {
	return queues[0].send->MaxBufsSpace();
}
int virtio_disk_jdriver::burst_recv(struct struct_mbuf *mbuf_list, int len) {
	return queues[0].send->BulkRemoveFromQueue(mbuf_list, len);
}
void virtio_disk_jdriver::burst_send(struct struct_mbuf *mbuf, int len) {
	int ret;

	queues[0].send->virtio_disable_cb();
	ret = queues[0].send->BulkAddToQueue(mbuf, len, 0);
	queues[0].send->virtio_queuekick();

	if (interrupts_disabled == 0) {
		queues[0].send->virtio_enable_cb();
	}
	return;
}

static uint64_t virtio_config64(unsigned long pcio_addr) {
	uint64_t ret;
	auto addr = pcio_addr + VIRTIO_MSI_CONFIG_VECTOR;
	ret = inl(addr);
	ret = ret + (inl(addr + 4) * (0x1 << 32));
	return ret;
}
static uint32_t virtio_config32(unsigned long pcio_addr) {
	uint64_t ret;
	auto addr = pcio_addr + VIRTIO_MSI_CONFIG_VECTOR;
	ret = inl(addr);
	return ret;
}

static uint16_t virtio_config16(unsigned long pcio_addr) {
	uint64_t ret;
	auto addr = pcio_addr + VIRTIO_MSI_CONFIG_VECTOR;
	ret = inw(addr);
	return ret;
}

/* Feature bits */
#define VIRTIO_BLK_F_SIZE_MAX   1       /* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX    2       /* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY   4       /* Legacy geometry available  */
#define VIRTIO_BLK_F_RO         5       /* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE   6       /* Block size of disk is available*/
#define VIRTIO_BLK_F_TOPOLOGY   10      /* Topology information is available */
#define VIRTIO_BLK_F_MQ         12      /* support more than one vq */

/* Legacy feature bits */
//#ifndef VIRTIO_BLK_NO_LEGACY
#define VIRTIO_BLK_F_BARRIER    0       /* Does host support barriers? */
#define VIRTIO_BLK_F_SCSI       7       /* Supports scsi command passthru */
#define VIRTIO_BLK_F_WCE        9       /* Writeback mode enabled after reset */
#define VIRTIO_BLK_F_CONFIG_WCE 11      /* Writeback mode available in config */
/*
 * struct v i r t i o _ b l k _ c o n f i g {
 u64 capacity ;
 u32 size_max ;
 u32 seg_max ;
 s t r u c t v i r t i o _ b l k _ g e o m e t r y {
 u16 cylinders ;
 u8 heads;
 u8 sectors;
 } g e o m e t r y ;
 u32 blk_size;

 the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY
 exponent for physical block per logical block.
 __u8 physical_block_exp;
 **  alignment offset in logical blocks. --
 __u8 alignment_offset;
 ** minimum I/O size without performance penalty in logical blocks. --/
 __u16 min_io_size;
 ** optimal sustained I/O size in logical blocks. --
 __u32 opt_io_size;
 ** writeback mode (if VIRTIO_BLK_F_CONFIG_WCE) --
 __u8 wce;
 __u8 unused;

 ** number of vqs, only available when VIRTIO_BLK_F_MQ is set **
 __u16 num_queues;
 }
 */
int virtio_disk_jdriver::disk_attach_device(class jdevice *jdev) {
	auto pci_ioaddr = jdev->pci_device.pci_ioaddr;
	pci_dev_header_t *pci_hdr = &device->pci_device.pci_header;
	uint32_t features;
	uint32_t guest_features = 0;
	uint32_t mask_features = 0;
	uint32_t msi_vector;

	this->device = jdev;
	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	Virtio disk: Initializing status :%x : \n",
			virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);
	auto addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log(
			"	Virtio disk: Initializing VIRTIO PCI hostfeatures :%x: status :%x :\n",
			features, virtio_get_pcistatus(pci_ioaddr));
	guest_features = features;
	mask_features = (0x0007ff);

	guest_features = guest_features & mask_features;
	addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	outl(addr, features);

	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = pci_read_msi(&device->pci_device.pci_addr,
				&device->pci_device.pci_header, &device->pci_device.pci_bars[0],
				device->pci_device.pci_bar_count, &device->pci_device.msix_cfg);
		if (msi_vector > 0) {
#if 1
			pci_enable_msix(&device->pci_device.pci_addr,
					&device->pci_device.msix_cfg,
					device->pci_device.pci_header.capabilities_pointer);
#else
			msi_vector = 0;
#endif
			ut_log("  virtio_disk  MSI available  :%d:%x \n", msi_vector,
					msi_vector);
		}
	} else {
		msi_vector = 0;
	}

	int i;
	unsigned long config_data;
	disk_size = 0;

	if (jdev->pci_device.pci_header.device_id != VIRTIO_PCI_SCSI_DEVICE_ID) {
		auto cfg_addr = pci_ioaddr;
		if (msi_vector != 0) {
			cfg_addr = cfg_addr + 4;
		}
		disk_size = virtio_config64(cfg_addr + 0) * 512;
		blk_size = virtio_config32(cfg_addr + 20);
		max_vqs = virtio_config16(cfg_addr + 34);
		ut_log(" 	Virtio-blk Disk size:%d(%x)  blk_size:%d   max_vqs: %d \n",
				disk_size, disk_size, blk_size, max_vqs);
	} else {
		ut_log("	virtio-scsi  Num of Reques Queues: %d \n",
				virtio_config32(pci_ioaddr + 0));
		INIT_LOG("	SCSI seg max: %d \n", virtio_config32(pci_ioaddr + 4));
		INIT_LOG("	SCSI max sector: %d \n", virtio_config32(pci_ioaddr + 8));
		INIT_LOG("	SCSI cmd_per_lun: %d \n", virtio_config32(pci_ioaddr + 12));
		INIT_LOG("	SCSI event_info_size: %d \n",
				virtio_config32(pci_ioaddr + 16));
		INIT_LOG("	SCSI sense size: %d \n", virtio_config32(pci_ioaddr + 20));
		INIT_LOG("	SCSI cdb size: %d \n", virtio_config32(pci_ioaddr + 24));
	}
	if (jdev->pci_device.pci_header.device_id != VIRTIO_PCI_SCSI_DEVICE_ID) {
		this->queues[0].send = jnew_obj(disk_virtio_queue, device, 0,
				VQTYPE_SEND)
		;
	} else {
		this->queues[0].send = jnew_obj(disk_virtio_queue, device, 0,
				VQTYPE_SEND)
		;
	}
	if (msi_vector > 0) {
		ut_log("  virtio disk :  msi vectors: %d\n",
				jdev->pci_device.msix_cfg.isr_vector);
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0);
		ar_registerInterrupt(msi_vector, virtio_disk_interrupt,
				"virtio_disk_msi", this);
		inb(pci_ioaddr + VIRTIO_PCI_ISR);
	} else {
		INIT_LOG("	Virtio disk:  VIRTIO PCI COMPLETED with driver ok :%x \n",
				virtio_get_pcistatus(pci_ioaddr));
		inb(pci_ioaddr + VIRTIO_PCI_ISR);
		ar_registerInterrupt(32 + jdev->pci_device.pci_header.interrupt_line,
				virtio_disk_interrupt, "virt_disk_irq", (void *) this);
	}

	INIT_LOG("		driver status:  %x :\n", virtio_get_pcistatus(pci_ioaddr));
	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	INIT_LOG(
			"		second time	Virtio disk:  VIRTIO PCI COMPLETED with driver ok :%x \n",
			virtio_get_pcistatus(pci_ioaddr));
	interrupts_disabled = 0;

//	virtio_disable_cb(this->queues[0].send); /* disable interrupts on sending side */
	return 1;
}

int virtio_disk_jdriver::probe_device(class jdevice *jdev) {
	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& ((jdev->pci_device.pci_header.device_id
					== VIRTIO_PCI_BLOCK_DEVICE_ID)
					|| (jdev->pci_device.pci_header.device_id
							== VIRTIO_PCI_SCSI_DEVICE_ID))) {
		ut_log("		Matches the disk Probe :%d\n",
				jdev->pci_device.pci_header.device_id);
		return JSUCCESS;
	}
	return JFAIL;
}
extern "C" {
extern int init_tarfs(jdriver *driver_arg);
}
extern jdriver *disk_drivers[];

jdriver *virtio_disk_jdriver::attach_device(class jdevice *jdev) {
	int i;

	COPY_OBJ(virtio_disk_jdriver, this, new_obj, jdev);
	((virtio_disk_jdriver *) new_obj)->disk_attach_device(jdev);
	for (i = 0; i < 5; i++) {
		if (disk_drivers[i] == 0) {
			disk_drivers[i] = (jdriver *) new_obj;
			break;
		}
	}

	((virtio_disk_jdriver *) new_obj)->waitq = jnew_obj(wait_queue,
			"waitq_disk", 0)
	;
	//spin_lock_init(&((virtio_disk_jdriver *)new_obj)->io_lock);
	init_tarfs((jdriver *) new_obj);
	return (jdriver *) new_obj;
}

int virtio_disk_jdriver::dettach_device(jdevice *jdev) {
	/*TODO:  Need to free the resources */
	return JFAIL;
}

#define MAX_REQS 10
int diskio_submit_requests(struct virtio_blk_req **buf, int len,
		virtio_disk_jdriver *dev, unsigned char *user_buf, int user_len,
		int intial_skip, int read_ahead);
int virtio_disk_jdriver::disk_io(int type, unsigned char *buf, int len,
		int offset, int read_ahead) {
	struct virtio_blk_req *reqs[MAX_REQS], *tmp_req;
	int sector;
	int i, req_count, data_len, curr_len, max_reqs;
	int initial_skip, blks;
	unsigned long addr, flags;
	int qlen, ret;
	ret = 0;
	int curr_offset;
	int scsi_type = 0;

	if (device->pci_device.pci_header.device_id == VIRTIO_PCI_SCSI_DEVICE_ID) {
		scsi_type = 1;
		ut_printf(" scsi reading ..\n");
		//BRK;
	}

	sector = offset / blk_size;
	initial_skip = offset - sector * blk_size;

	data_len = len + initial_skip;
	curr_offset = offset - initial_skip;
	curr_len = data_len;
	max_reqs = 5;

	for (req_count = 0; req_count < max_reqs && curr_len > 0; req_count++) {
		int req_len = curr_len;
		if (req_len > VIRTIO_BLK_DATA_SIZE) {
			req_len = VIRTIO_BLK_DATA_SIZE;
		}
		if ((req_len + curr_offset) >= disk_size) {
			req_len = disk_size - curr_offset;
		}

		blks = req_len / blk_size;
		if ((blks * blk_size) != req_len) {
			req_len = (blks + 1) * blk_size;
		}
		reqs[req_count] = createBuf(type,
				buf + (req_count * VIRTIO_BLK_DATA_SIZE), sector, req_len);
		curr_offset = curr_offset + VIRTIO_BLK_DATA_SIZE;
		curr_len = curr_len - VIRTIO_BLK_DATA_SIZE;
	}
	ret = diskio_submit_requests(reqs, req_count, this, buf, len, initial_skip,
			read_ahead);

//		ut_printf("%d -> %d  DATA :%x :%x  disksize:%d blksize:%d\n",i,disk_bufs[i]->status,disk_bufs[i]->data[0],disk_bufs[i]->data[1],disk_size,blk_size);
	return ret;
}

int virtio_disk_jdriver::read(unsigned char *buf, int len, int offset,
		int read_ahead) {
	int ret;
//ut_log(" read len :  %d offset:%d \n",len, offset);
	ret = disk_io(DISK_READ, buf, len, offset, read_ahead);
	return ret;
}
int virtio_disk_jdriver::write(unsigned char *buf, int len, int offset) {
	int ret;

	ret = disk_io(DISK_WRITE, buf, len, offset, 0);
	return ret;
}
int virtio_disk_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	if (arg1 == IOCTL_DISK_SIZE) {
		return disk_size;
	}
	return JSUCCESS;
}
