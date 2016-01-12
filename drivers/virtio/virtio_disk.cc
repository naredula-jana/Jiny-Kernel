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

void virtio_disk_jdriver::print_stats(unsigned char *arg1,
		unsigned char *arg2) {
	if (device ==0){
		ut_printf(" driver not attached to device \n");
		return;
	}
	queues[0].send->print_stats(0, 0);
}

int virtio_disk_jdriver::MaxBufsSpace() {
	return queues[0].send->MaxBufsSpace();
}
int virtio_disk_jdriver::burst_recv(struct struct_mbuf *mbuf_list, int len) {
	return queues[0].send->BulkRemoveFromQueue(mbuf_list, len);
}
int virtio_disk_jdriver::burst_send(struct struct_mbuf *mbuf, int len) {
	int ret;

	queues[0].send->virtio_disable_cb();
	ret = queues[0].send->BulkAddToQueue(mbuf, len, 0);
	queues[0].send->virtio_queuekick();

	if (interrupts_disabled == 0) {
		queues[0].send->virtio_enable_cb();
	}
	return ret;
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
int virtio_disk_jdriver::init_device(class jdevice *jdev) {
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
virtio_disk_jdriver::virtio_disk_jdriver(class jdevice *jdev){
	device = jdev;
	if (jdev != 0){
		init_device(jdev);
		req_blk_size = PAGE_SIZE;
		//req_blk_size = 512;
		waitq = jnew_obj(wait_queue,"waitq_cirtio_disk", 0);
		init_tarfs((jdriver *) this);
	}
}
extern jdriver *disk_drivers[];
jdriver *virtio_disk_jdriver::attach_device(class jdevice *jdev) {
	int i;
	virtio_disk_jdriver *new_obj = jnew_obj(virtio_disk_jdriver,jdev);

	for (i = 0; i < 5; i++) {
		if (disk_drivers[i] == 0) {
			disk_drivers[i] = (jdriver *) new_obj;
			break;
		}
	}

	return (jdriver *) new_obj;
}

int virtio_disk_jdriver::dettach_device(jdevice *jdev) {
	/*TODO:  Need to free the resources */
	return JFAIL;
}


int virtio_disk_jdriver::read(unsigned char *buf, int len, int offset,
		int read_ahead) {
	int ret;
//ut_log(" read len :  %d offset:%d \n",len, offset);
	ret = disk_io(DISK_READ, buf, len, offset, read_ahead, this);
	return ret;
}
int virtio_disk_jdriver::write(unsigned char *buf, int len, int offset) {
	int ret;

	ret = disk_io(DISK_WRITE, buf, len, offset, 0, this);
	return ret;
}
int virtio_disk_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	if (arg1 == IOCTL_DISK_SIZE) {
		return disk_size;
	}
	return JSUCCESS;
}
