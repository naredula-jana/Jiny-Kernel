
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

}
#include "jdevice.h"

/************************   utilities used by vitio drivr ***************************/
struct virtio_feature_desc {
	int feature_bit;
	char *name;
};
void display_virtiofeatures(unsigned long feature, struct virtio_feature_desc *desc) {
	int i,j,bit;
	for (i = 0; i < 32; i++) {
		bit = (feature >> i) & (0x1);
		if (bit) {
			for (j = 0; ( desc[j].name != NULL); j++) {
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
static void virtio_set_pcistatus(unsigned long pci_ioaddr,
		unsigned char status) {
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

static int addBufToQueue(struct virtqueue *vq, unsigned char *buf,
		unsigned long len) {
	struct scatterlist sg[2];
	int ret;

	if (buf == 0) {
		buf = (unsigned char *) alloc_page(0);
		len = 4096; /* page size */
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
		ret = virtio_add_buf_to_queue(vq, sg, 0, 2, (void *) sg[0].page_link,
				0);/* recv q*/
	} else {
		ret = virtio_add_buf_to_queue(vq, sg, 2, 0, (void *) sg[0].page_link,
				0);/* send q */
	}

	return ret;
}
static int virtio_stat(jdriver *arg){
	virtio_jdriver *driver =(virtio_jdriver *)arg;

	ut_printf(" Send(P,K): %d,%d  Recv(P,K,I):%d,%d,%d\n",driver->stats_sends,driver->stat_send_kicks,driver->stat_recvs,driver->stat_recv_kicks,driver->stat_recv_interrupts);
}
/*******************************  virtio_jdriver ********************************/

int virtio_jdriver::virtio_create_queue(uint16_t index, int qType) {
	int size;
	uint16_t num;
	unsigned long queue;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;
#if 1
	outw(pci_ioaddr + VIRTIO_PCI_QUEUE_SEL, index);

	num = inw(pci_ioaddr + VIRTIO_PCI_QUEUE_NUM);
	DEBUG(
			"virtio NUM-%d : %x  :%x\n", index, num, vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	if (num == 0) {
		vq[index] = 0;
		return 0;
	}

	size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));

	DEBUG("Creating PAGES order: %d size:%d  \n", get_order(size), size);
	//vring_size(num);
	queue = mm_getFreePages(MEM_CLEAR, get_order(size));

	/* activate the queue */
	outl(pci_ioaddr + VIRTIO_PCI_QUEUE_PFN,
			__pa(queue) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	/* create the vring */
	vq[index] = vring_new_virtqueue(num, VIRTIO_PCI_VRING_ALIGN,
			device->pci_device.pci_ioaddr, (void *) queue, &notify, &callback,
			"VIRTQUEUE", index);
	virtqueue_enable_cb_delayed(vq[index]);
	vq[index]->qType = qType;
#endif
	return 1;
}
/******************************************* virtio net *********************************/

struct virtio_feature_desc vtnet_feature_desc[] = { { VIRTIO_NET_F_CSUM,
		"TxChecksum" }, { VIRTIO_NET_F_GUEST_CSUM, "RxChecksum" }, {
		VIRTIO_NET_F_MAC, "MacAddress" }, { VIRTIO_NET_F_GSO, "TxAllGSO" }, {
		VIRTIO_NET_F_GUEST_TSO4, "RxTSOv4" }, { VIRTIO_NET_F_GUEST_TSO6,
		"RxTSOv6" }, { VIRTIO_NET_F_GUEST_ECN, "RxECN" }, {
		VIRTIO_NET_F_GUEST_UFO, "RxUFO" },
		{ VIRTIO_NET_F_HOST_TSO4, "TxTSOv4" }, { VIRTIO_NET_F_HOST_TSO6,
				"TxTSOv6" }, { VIRTIO_NET_F_HOST_ECN, "TxTSOECN" }, {
				VIRTIO_NET_F_HOST_UFO, "TxUFO" }, { VIRTIO_NET_F_MRG_RXBUF,
				"MrgRxBuf" }, { VIRTIO_NET_F_STATUS, "Status" }, {
				VIRTIO_NET_F_CTRL_VQ, "ControlVq" }, { VIRTIO_NET_F_CTRL_RX,
				"RxMode" }, { VIRTIO_NET_F_CTRL_VLAN, "VLanFilter" }, {
				VIRTIO_NET_F_CTRL_RX_EXTRA, "RxModeExtra" }, { 0, NULL } };
static int virtio_net_poll_device(void *private_data, int enable_interrupt,
		int total_pkts) {
	unsigned char *addr;
	unsigned int len = 0;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;
	unsigned char *replace_buf;
	int i;
	int ret = 0;

	for (i = 0; i < total_pkts; i++) {
		addr = (unsigned char *) virtio_removeFromQueue(driver->vq[0],
				(unsigned int *) &len);
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
static int virtio_send_errors = 0;
static int netdriver_xmit(unsigned char* data, unsigned int len,
		void *private_data) {
	virtio_net_jdriver *net_driver = (virtio_net_jdriver *) private_data;
	static int send_pkts = 0;
	jdevice *dev;
	int i, ret;
	unsigned long flags;

	dev = (jdevice *) net_driver->device;
	if (dev == 0 || data == 0)
		return 0;

	unsigned long addr;
	addr = (unsigned long) alloc_page(0);
	if (addr == 0)
		return 0;
	ut_memset((unsigned char *) addr, 0, 10);
	ut_memcpy((unsigned char *) addr + 10, data, len);
	ret = -ENOSPC;

	spin_lock_irqsave(&virtionet_lock, flags);
	ret = addBufToQueue(net_driver->vq[1], (unsigned char *) addr, len + 10);
	net_driver->stats_sends++;
	send_pkts++;
	if (ret == -ENOSPC) {
		mm_putFreePages((unsigned long) addr, 0);
		virtio_send_errors++;
	}
	send_pkts = 0;
	virtio_queue_kick(net_driver->vq[1]);
	net_driver->stat_send_kicks++;
	spin_unlock_irqrestore(&virtionet_lock, flags);
#if 1
	i = 0;
	while (i < 50) {
		i++;
		spin_lock_irqsave(&virtionet_lock, flags);
		addr = (unsigned long) virtio_removeFromQueue(net_driver->vq[1], &len);
		spin_unlock_irqrestore(&virtionet_lock, flags);
		if (addr) {
			mm_putFreePages(addr, 0);
		} else {
			return 1;
		}
	}
#endif
	return 1;
}

static int virtio_net_interrupt(void *private_data) {
	jdevice *dev;
	unsigned char isr;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi == 0)
		isr = inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);

	driver->stat_recv_interrupts++;
	virtio_disable_cb(driver->vq[0]);
	netif_rx_enable_polling(private_data, virtio_net_poll_device);
	//virtio_net_poll_device(private_data,0,10);
	return 0;

}
static int net_attach_device(class jdevice *jdev) {
	unsigned long addr;
	unsigned long features;
	int i;
	virtio_net_jdriver *net_driver = (virtio_net_jdriver *) jdev->driver;
	pci_dev_header_t *pci_hdr = &jdev->pci_device.pci_header;
	unsigned long pci_ioaddr = jdev->pci_device.pci_ioaddr;
	uint32_t msi_vector;

	net_driver->device = jdev;
	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	VirtioNet: Initializing VIRTIO PCI NET status :%x : \n",
			virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	VirtioNet:  hostfeatures :%x:  capabilitie:%x\n", features,
			pci_hdr->capabilities_pointer);
	display_virtiofeatures(features, vtnet_feature_desc);

	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = pci_read_msi(&jdev->pci_device.pci_addr,
				&jdev->pci_device.pci_header, &jdev->pci_device.pci_bars[0],
				jdev->pci_device.pci_bar_count, &jdev->pci_device.msix_cfg);
		if (msi_vector > 0)
			pci_enable_msix(&jdev->pci_device.pci_addr,
					&jdev->pci_device.msix_cfg,
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
		net_driver->mac[i] = inb(addr + i);
	ut_log("	VirtioNet:  pioaddr:%x MAC address : %x :%x :%x :%x :%x :%x mis_vector:%x   :\n",addr, net_driver->mac[0], net_driver->mac[1], net_driver->mac[2], net_driver->mac[3], net_driver->mac[4], net_driver->mac[5],msi_vector);

	net_driver->virtio_create_queue(0, 1);
	if (msi_vector > 0) {
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0);
	}
	net_driver->virtio_create_queue(1, 2);
	if (msi_vector > 0) {
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 1);
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0xffff);
	}

	virtio_disable_cb(net_driver->vq[1]); /* disable interrupts on sending side */

	if (msi_vector > 0) {
		for (i = 0; i < 3; i++)
			ar_registerInterrupt(msi_vector + i, virtio_net_interrupt,
					"virt_net_msi", (void *) net_driver);
	}


	for (i = 0; i < 120; i++) /* add buffers to recv q */
		addBufToQueue(net_driver->vq[0], 0, 4096);

	inb(pci_ioaddr + VIRTIO_PCI_ISR);
	virtio_queue_kick(net_driver->vq[0]);

	registerNetworkHandler(NETWORK_DRIVER, netdriver_xmit, (void *) net_driver,net_driver->mac);

	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	ut_log("VirtioNet:  Initilization Completed status:%x\n",
			virtio_get_pcistatus(pci_ioaddr));

	return 1;
}
static int net_probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID) && (jdev->pci_device.pci_header.device_id == VIRTIO_PCI_NET_DEVICE_ID)) {
		ut_log(" Matches inside the NETPROBE.... \n");
		return JSUCCESS;
	}
	return JFAIL;
}

/***************************************************************************************************/
extern wait_queue_t p9_waitq;
static int virtio_9p_interrupt(void *private_data) { // TODO: handling similar  type of interrupt generating while serving P9 interrupt.
	unsigned char isr;
	int ret;
	virtio_p9_jdriver *driver = (virtio_p9_jdriver *) private_data;

	if (driver->device->pci_device.msi == 0)
		isr = inb(driver->device->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);

	ret = ipc_wakeup_waitqueue(&p9_waitq); /* wake all the waiting processes */
	if (ret == 0) {
		//ut_log("ERROR:New  p9 wait No one is waiting requests:%d intr:%d\n",stat_request,stat_intr);
	}
	return 0;
}

static int p9_attach_device(class jdevice *jdev) {
	pci_dev_header_t *pci_hdr = &jdev->pci_device.pci_header;
	unsigned long pci_ioaddr = jdev->pci_device.pci_ioaddr;
	virtio_p9_jdriver *p9_driver = (virtio_p9_jdriver *) jdev->driver;
	unsigned long addr;
	unsigned long features;

	p9_driver->device = jdev;
	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	Virtio P9: Initializing VIRTIO PCI NET status :%x : \n",
			virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	Virtio P9: Initializing VIRTIO PCI 9P hostfeatures :%x:\n", features);

	p9_driver->virtio_create_queue(0, 2);
	if (jdev->pci_device.msix_cfg.isr_vector > 0) {
#if 0
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0);
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0xffff);
		ar_registerInterrupt(msi_vector, virtio_9p_interrupt, "virtio_p9_msi");
#endif
	}
	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	ut_log("	Virtio P9:  VIRTIO PCI COMPLETED with driver ok :%x \n", virtio_get_pcistatus(pci_ioaddr));
	inb(pci_ioaddr + VIRTIO_PCI_ISR);

	ipc_register_waitqueue(&p9_waitq, "waitq_p9", 0);
	ar_registerInterrupt(32 + jdev->pci_device.pci_header.interrupt_line,
			virtio_9p_interrupt, "virt_p9_irq", (void *) jdev->driver);

#if 0
	virtio_dev_t *virtio_dev;
	virtio_dev = (virtio_dev_t *)ut_malloc(sizeof(virtio_dev_t));
	virtio_dev->pci_ioaddr = jdev->pci_device.pci_ioaddr;
	virtio_dev->pci_iolen= jdev->pci_device.pci_iolen;
	virtio_dev->pci_mmio = jdev->pci_device.pci_mmio;
	virtio_dev->pci_mmiolen = jdev->pci_device.pci_mmiolen;
	virtio_dev->vq[0]=p9_driver->vq[0];
	virtio_dev->vq[1]=p9_driver->vq[1];
	p9_driver->virtio_dev = virtio_dev;
	p9_dev = virtio_dev;
#endif
	p9_initFs(p9_driver);
	return 1;
}

static int p9_probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id == VIRTIO_PCI_9P_DEVICE_ID)) {
		ut_log(" Matches the P9Probe \n");
		return JSUCCESS;
	}
	return JFAIL;
}

/*************************************************************************************************/
virtio_p9_jdriver p9_jdriver;
virtio_net_jdriver net_jdriver;

extern "C" {
void init_p9_jdriver() {
	p9_jdriver.name = "p9_driver";
	p9_jdriver.init_func(p9_probe_device, p9_attach_device,virtio_stat);
	register_jdriver(&p9_jdriver);
}
void init_net_jdriver() {
	net_jdriver.name = "net_driver";
	net_jdriver.init_func(net_probe_device, net_attach_device,virtio_stat);
	register_jdriver(&net_jdriver);
}
struct virtqueue *virtio_jdriver_getvq(void *driver, int index){
	virtio_jdriver *jdriver = (virtio_jdriver *)driver;

	return jdriver->vq[index];
}
}
