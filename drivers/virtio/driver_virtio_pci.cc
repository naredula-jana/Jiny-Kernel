
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   driver_virtio_pci.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/

extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "virtio.h"
#include "virtio_ring.h"
#include "virtio_pci.h"
#include "net/virtio_net.h"
#include "mach_dep.h"
extern int p9_initFs(void *p);
extern void print_vq(struct virtqueue *_vq);
}
#include "jdevice.h"
extern int register_netdevice(jdevice *device);

/************************   utilities used by vitio drivr ***************************/
struct virtio_feature_desc {
	int feature_bit;
	const char *name;
};
static void display_virtiofeatures(unsigned long feature, struct virtio_feature_desc *desc) {
	int i, j, bit;
	for (i = 0; i < 32; i++) {
		bit = (feature >> i) & (0x1);
		if (bit) {
			for (j = 0; (desc[j].name != NULL); j++) {
				if (desc[j].feature_bit == i)
					ut_log("%s,", desc[j].name);
			}
		}
	}
	ut_log("\n");
}
static unsigned char virtio_get_pcistatus(unsigned long pci_ioaddr) {
	uint16_t addr = pci_ioaddr + VIRTIO_PCI_STATUS;
	return inb(addr);
}
static void virtio_set_pcistatus(unsigned long pci_ioaddr, unsigned char status) {
	uint16_t addr = pci_ioaddr + VIRTIO_PCI_STATUS;
	outb(addr, status);
}
/* the notify function used when creating a virt queue */
static void notify(struct virtqueue *vq) {
	outw(vq->pci_ioaddr + VIRTIO_PCI_QUEUE_NOTIFY, vq->queue_number);
}
static void callback(struct virtqueue *vq) {
	DEBUG("  VIRTIO CALLBACK in VIRT queue :%x\n", vq);
}
static int get_order(unsigned long size) {
	int order;

	size = (size - 1) >> (PAGE_SHIFT - 1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

static int addBufToQueue(struct virtqueue *vq, unsigned char *buf, unsigned long len) {
	struct scatterlist sg[2];
	int ret;

	if (buf == 0) {
		buf = (unsigned char *) alloc_page(0);
		len = 4096; /* page size */
	}
	if (buf == 0){
		BRK;
	}
	ut_memset(buf, 0, sizeof(struct virtio_net_hdr));

	sg[0].page_link = (unsigned long) buf;
	sg[0].length = sizeof(struct virtio_net_hdr);
	sg[0].offset = 0;
	sg[1].page_link = (unsigned long) (buf + sizeof(struct virtio_net_hdr));
	sg[1].length = len - sizeof(struct virtio_net_hdr);
	sg[1].offset = 0;
	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (vq->qType == 1) {
		ret = virtio_add_buf_to_queue(vq, sg, 0, 2, (void *) sg[0].page_link, 0);/* recv q*/
	} else {
		ret = virtio_add_buf_to_queue(vq, sg, 2, 0, (void *) sg[0].page_link, 0);/* send q */
	}

	return ret;
}

/*******************************  virtio_jdriver ********************************/

int virtio_jdriver::print_stats() {

	ut_printf("%s: Send(P,K,I): %d,%d,%d  Recv(P,K,I):%d,%d,%d allocs:%d free:%d err:%d\n", this->name, this->stat_sends,
			this->stat_send_kicks, this->stat_send_interrupts, this->stat_recvs, this->stat_recv_kicks, this->stat_recv_interrupts, this->stat_allocs,
			this->stat_frees, this->stat_err_nospace);
	print_vq(vq[0]);
	print_vq(vq[1]);
	return JSUCCESS;
}
int virtio_jdriver::virtio_create_queue(uint16_t index, int qType) {
	int size;
	uint16_t num;
	unsigned long queue;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;

	outw(pci_ioaddr + VIRTIO_PCI_QUEUE_SEL, index);

	num = inw(pci_ioaddr + VIRTIO_PCI_QUEUE_NUM);
	ut_log("	virtio create queue NUM-%d : num %x(%d)  :%x\n", index, num, num, vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	if (num == 0) {
		vq[index] = 0;
		return 0;
	}

	size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));

	ut_log("	virtio Creating PAGES order: %d size:%d  \n", get_order(size), size);
	//vring_size(num);
	queue = mm_getFreePages(MEM_CLEAR, get_order(size));

	/* activate the queue */
	outl(pci_ioaddr + VIRTIO_PCI_QUEUE_PFN, __pa(queue) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	/* create the vring */
	vq[index] = vring_new_virtqueue(num, VIRTIO_PCI_VRING_ALIGN, device->pci_device.pci_ioaddr, (void *) queue, &notify,
			&callback, "VIRTQUEUE", index);
	virtqueue_enable_cb_delayed(vq[index]);
	vq[index]->qType = qType;

	return 1;
}
/******************************************* virtio net *********************************/

struct virtio_feature_desc vtnet_feature_desc[] = { { VIRTIO_NET_F_CSUM, "TxChecksum" },
		{ VIRTIO_NET_F_GUEST_CSUM, "RxChecksum" }, { VIRTIO_NET_F_MAC, "MacAddress" }, { VIRTIO_NET_F_GSO, "TxAllGSO" }, {
				VIRTIO_NET_F_GUEST_TSO4, "RxTSOv4" }, { VIRTIO_NET_F_GUEST_TSO6, "RxTSOv6" }, { VIRTIO_NET_F_GUEST_ECN, "RxECN" },
		{ VIRTIO_NET_F_GUEST_UFO, "RxUFO" }, { VIRTIO_NET_F_HOST_TSO4, "TxTSOv4" }, { VIRTIO_NET_F_HOST_TSO6, "TxTSOv6" }, {
				VIRTIO_NET_F_HOST_ECN, "TxTSOECN" }, { VIRTIO_NET_F_HOST_UFO, "TxUFO" }, { VIRTIO_NET_F_MRG_RXBUF, "MrgRxBuf" }, {
				VIRTIO_NET_F_STATUS, "Status" }, { VIRTIO_NET_F_CTRL_VQ, "ControlVq" }, { VIRTIO_NET_F_CTRL_RX, "RxMode" }, {
				VIRTIO_NET_F_CTRL_VLAN, "VLanFilter" }, { VIRTIO_NET_F_CTRL_RX_EXTRA, "RxModeExtra" }, { 0, NULL } };
static int virtio_net_poll_device(void *private_data, int enable_interrupt, int total_pkts) {
	unsigned char *addr;
	unsigned int len = 0;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;
	unsigned char *replace_buf;
	int i;
	int ret = 0;

	for (i = 0; i < total_pkts; i++) {
		addr = (unsigned char *) virtio_removeFromQueue(driver->vq[0], (unsigned int *) &len);
		if (addr != 0) {
			driver->stat_recvs++;
			netif_rx(addr, len, &replace_buf);
			addBufToQueue(driver->vq[0], replace_buf, 4096);
			ret = ret + 1;
		} else {
			break;
		}
	}
	if (ret > 0) {
		driver->stat_recv_kicks++;
		virtio_queue_kick(driver->vq[0]);
	}
	if (enable_interrupt) {
		virtio_enable_cb(driver->vq[0]);
	}
	return ret;
}
static spinlock_t virtionet_lock = SPIN_LOCK_UNLOCKED(
		(unsigned char *) "new1_virtio_net");

static int netdriver_xmit(unsigned char* data, unsigned int len, void *private_data) {
	virtio_net_jdriver *net_driver = (virtio_net_jdriver *) private_data;
	return net_driver->write(data, len);
}

static int virtio_net_recv_interrupt(void *private_data) {
	jdevice *dev;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi == 0)
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);

	driver->stat_recv_interrupts++;
	virtio_disable_cb(driver->vq[0]);
	netif_rx_enable_polling(private_data, virtio_net_poll_device);
	//virtio_net_poll_device(private_data,0,10);
	return 0;

}
static int virtio_net_send_interrupt(void *private_data) {
	jdevice *dev;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi == 0)
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);

	driver->stat_send_interrupts++;
	virtio_disable_cb(driver->vq[1]);
	auto ret = ipc_wakeup_waitqueue(&(driver->send_waitq));
	return 0;

}
int virtio_net_jdriver::probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id == VIRTIO_PCI_NET_DEVICE_ID)) {
		ut_log(" Matches inside the NETPROBE.... \n");
		return JSUCCESS;
	}
	return JFAIL;
}
static int net_devices=0;
int virtio_net_jdriver::net_attach_device(class jdevice *jdev) {
	unsigned long addr;
	unsigned long features=0;
	int i;
	pci_dev_header_t *pci_hdr = &jdev->pci_device.pci_header;
	unsigned long pci_ioaddr = jdev->pci_device.pci_ioaddr;
	uint32_t msi_vector;
	unsigned char name[MAX_DEVICE_NAME];

	ut_snprintf(name,MAX_DEVICE_NAME,"net%d",net_devices);
	ut_strcpy(jdev->name,name);
	net_devices++;

	this->device = jdev;
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	VirtioNet: Initializing VIRTIO PCI NET status :%x : \n", virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	VirtioNet:  hostfeatures :%x:  capabilitie:%x\n", features, pci_hdr->capabilities_pointer);
	display_virtiofeatures(features, vtnet_feature_desc);

	 addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	 outl(addr,features);


	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = pci_read_msi(&jdev->pci_device.pci_addr, &jdev->pci_device.pci_header, &jdev->pci_device.pci_bars[0],
				jdev->pci_device.pci_bar_count, &jdev->pci_device.msix_cfg);
		if (msi_vector > 0)
			pci_enable_msix(&jdev->pci_device.pci_addr, &jdev->pci_device.msix_cfg,
					jdev->pci_device.pci_header.capabilities_pointer);
	} else {
		msi_vector = 0;
	}

	if (msi_vector == 0) {
		addr = pci_ioaddr + 20;
	} else {
		addr = pci_ioaddr + 24;
	}
	for (i = 0; i < 6; i++)
		this->mac[i] = inb(addr + i);
	ut_log("	VirtioNet:  pioaddr:%x MAC address : %x :%x :%x :%x :%x :%x mis_vector:%x   :\n", addr, this->mac[0], this->mac[1],
			this->mac[2], this->mac[3], this->mac[4], this->mac[5], msi_vector);

	this->virtio_create_queue(0, 1);
	if (msi_vector > 0) {
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0);
	}
	this->virtio_create_queue(1, 2);
	if (msi_vector > 0) {
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 1);
		ipc_register_waitqueue(&send_waitq, "waitq_net", 0);
	//	outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0xffff);
	}

//	virtio_disable_cb(this->vq[1]); /* disable interrupts on sending side */

	if (msi_vector > 0) {
		for (i = 0; i < 3; i++){
			char irq_name[MAX_DEVICE_NAME];
			if (i==0){
				ut_snprintf(irq_name,MAX_DEVICE_NAME,"%s_recv_msi",jdev->name);
				ar_registerInterrupt(msi_vector + i, virtio_net_recv_interrupt, irq_name, (void *) this);
			}
			if (i!=0){
				ut_snprintf(irq_name,MAX_DEVICE_NAME,"%s_send_msi",jdev->name);
				ar_registerInterrupt(msi_vector + i, virtio_net_send_interrupt, irq_name, (void *) this);
			}
		}
	}

	for (i = 0; i < 120; i++) /* add buffers to recv q */
		addBufToQueue(this->vq[0], 0, 4096);

	inb(pci_ioaddr + VIRTIO_PCI_ISR);
	virtio_queue_kick(this->vq[0]);
	virtio_queue_kick(this->vq[1]);

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	ut_log("VirtioNet:  Initilization Completed status:%x\n", virtio_get_pcistatus(pci_ioaddr));
	return 1;
}

jdriver *virtio_net_jdriver::attach_device(class jdevice *jdev) {
	stat_allocs = 0;
	stat_frees = 0;
	stat_err_nospace = 0;
	COPY_OBJ(virtio_net_jdriver, this, new_obj, jdev);
	((virtio_net_jdriver *) new_obj)->net_attach_device(jdev);
	jdev->driver = new_obj;
	register_netdevice(jdev);
	return (jdriver *) new_obj;
}

int virtio_net_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}
int virtio_net_jdriver::read(unsigned char *buf, int len) {
	return 0;
}
int virtio_net_jdriver::free_send_bufs(){
	int i;
	unsigned long flags;
	unsigned long addr;
	int len;

	i = 0;
	while (i < 50) {
		i++;
		spin_lock_irqsave(&virtionet_lock, flags);
		addr = (unsigned long) virtio_removeFromQueue(this->vq[1], (unsigned int *) &len);
		spin_unlock_irqrestore(&virtionet_lock, flags);
		if (addr != 0) {
			free_page(addr);
			stat_frees++;
		} else {
			return 1;
		}
	}
}
int virtio_net_jdriver::write(unsigned char *data, int len) {
	jdevice *dev;
	int i, ret;
	unsigned long flags;
	unsigned long addr;

	dev = (jdevice *) this->device;
	if (dev == 0 || data == 0)
		return 0;

	addr = (unsigned long) alloc_page(0);
	if (addr == 0)
		return 0;
	stat_allocs++;
	ut_memset((unsigned char *) addr, 0, 10);
	ut_memcpy((unsigned char *) addr + 10, data, len);
	ret = -ERROR_VIRTIO_ENOSPC;

	spin_lock_irqsave(&virtionet_lock, flags);

#if 1
	for (i=0; i<2 && ret == -ERROR_VIRTIO_ENOSPC; i++){
		ret = addBufToQueue(this->vq[1], (unsigned char *) addr, len + 10);
		if (ret == -ERROR_VIRTIO_ENOSPC){
			stat_err_nospace++;
			free_send_bufs();
			ret = addBufToQueue(this->vq[1], (unsigned char *) addr, len + 10);
		}

		if (ret == -ERROR_VIRTIO_ENOSPC){
			stat_err_nospace++;
			spin_unlock_irqrestore(&virtionet_lock, flags);
			virtio_queue_kick(this->vq[1]);
			virtio_enable_cb(this->vq[1]);

			ipc_waiton_waitqueue(&send_waitq, 20);
			spin_lock_irqsave(&virtionet_lock, flags);
		}
	}
#else
	ret = addBufToQueue(this->vq[1], (unsigned char *) addr, len + 10);
#endif

	stat_sends++;

	if (ret == -ERROR_VIRTIO_ENOSPC) {
		free_page((unsigned long) addr);
		stat_err_nospace++;
		stat_frees++;
	}
	if ((stat_sends%1)==0){
		virtio_queue_kick(this->vq[1]);
		stat_send_kicks++;
	}

	spin_unlock_irqrestore(&virtionet_lock, flags);
	free_send_bufs();
	return 1;
}
int virtio_net_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	unsigned char *arg_mac = (unsigned char *) arg2;
	if (arg_mac == 0)
		return JFAIL;
	else {
		ut_memcpy(arg_mac, mac, 7);
		return JSUCCESS;
	}
}
/***************************************************************************************************/
extern wait_queue_t p9_waitq;
static int virtio_9p_interrupt(void *private_data) { // TODO: handling similar  type of interrupt generating while serving P9 interrupt.
	virtio_p9_jdriver *driver = (virtio_p9_jdriver *) private_data;

	if (driver->device->pci_device.msi == 0)
		inb(driver->device->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);

	auto ret = ipc_wakeup_waitqueue(&p9_waitq); /* wake all the waiting processes */
	if (ret == 0) {
		//ut_log("ERROR:New  p9 wait No one is waiting requests:%d intr:%d\n",stat_request,stat_intr);
	}
	return 0;
}
int virtio_p9_jdriver::probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id == VIRTIO_PCI_9P_DEVICE_ID)) {
		ut_log(" Matches the P9Probe \n");
		return JSUCCESS;
	}
	return JFAIL;
}
int virtio_p9_jdriver::p9_attach_device(class jdevice *jdev) {
	auto pci_ioaddr = jdev->pci_device.pci_ioaddr;
	unsigned long features;

	this->device = jdev;
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	Virtio P9: Initializing VIRTIO PCI NET status :%x : \n", virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	auto addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	Virtio P9: Initializing VIRTIO PCI 9P hostfeatures :%x:\n", features);


	this->virtio_create_queue(0, 2);
	if (jdev->pci_device.msix_cfg.isr_vector > 0) {
#if 0
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0);
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0xffff);
		ar_registerInterrupt(msi_vector, virtio_9p_interrupt, "virtio_p9_msi");
#endif
	}
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	ut_log("	Virtio P9:  VIRTIO PCI COMPLETED with driver ok :%x \n", virtio_get_pcistatus(pci_ioaddr));
	inb(pci_ioaddr + VIRTIO_PCI_ISR);

	ipc_register_waitqueue(&p9_waitq, "waitq_p9", 0);
	ar_registerInterrupt(32 + jdev->pci_device.pci_header.interrupt_line, virtio_9p_interrupt, "virt_p9_irq", (void *) this);

	p9_initFs(this);
	return 1;
}
jdriver *virtio_p9_jdriver::attach_device(class jdevice *jdev) {
	COPY_OBJ(virtio_p9_jdriver, this, new_obj, jdev);
	((virtio_p9_jdriver *) new_obj)->p9_attach_device(jdev);
	return (jdriver *) new_obj;
}
int virtio_p9_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}
int virtio_p9_jdriver::read(unsigned char *buf, int len) {
	return 0;
}
int virtio_p9_jdriver::write(unsigned char *buf, int len) {
	return 0;
}
int virtio_p9_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	return 0;
}
/*************************************************************************************************/
static virtio_p9_jdriver p9_jdriver;
static virtio_net_jdriver net_jdriver;

extern "C" {
static void *vptr_p9[7] = { (void *) &virtio_p9_jdriver::probe_device, (void *) &virtio_p9_jdriver::attach_device,
		(void *) &virtio_p9_jdriver::dettach_device, (void *) &virtio_p9_jdriver::read, (void *) &virtio_p9_jdriver::write,
		(void *) &virtio_jdriver::print_stats, 0 };
void init_p9_jdriver() {
	void **p = (void **) &p9_jdriver;
	*p = &vptr_p9[0];

	p9_jdriver.name = (unsigned char *) "p9_driver";

	register_jdriver(&p9_jdriver);
}
void *vptr_net[8] = { (void *) &virtio_net_jdriver::probe_device, (void *) &virtio_net_jdriver::attach_device,
		(void *) &virtio_net_jdriver::dettach_device, (void *) &virtio_net_jdriver::read, (void *) &virtio_net_jdriver::write,
		(void *) &virtio_jdriver::print_stats, (void *) &virtio_net_jdriver::ioctl, 0 };
void init_net_jdriver() {
	void **p = (void **) &net_jdriver;
	*p = &vptr_net[0];

	net_jdriver.name = (unsigned char *) "net_driver";

	register_jdriver(&net_jdriver);
}
struct virtqueue *virtio_jdriver_getvq(void *driver, int index) {
	virtio_jdriver *jdriver = (virtio_jdriver *) driver;

	return jdriver->vq[index];
}
}
