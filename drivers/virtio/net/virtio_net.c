#define DEBUG_ENABLE 1
#include "common.h"
#include "device.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "../virtio.h"
#include "../virtio_ring.h"
#include "../virtio_pci.h"
#include "virtio_net.h"
#include "mach_dep.h"

int test_virtio_nob = 0;

static void virtio_net_interrupt(registers_t regs,  void *private_data);
static struct virtio_feature_desc vtnet_feature_desc[] = { { VIRTIO_NET_F_CSUM,
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
extern void print_vq(struct virtqueue *_vq);
static int netdriver_xmit(unsigned char* data, unsigned int len,
		void *private_data);

static int addBufToQueue(struct virtqueue *vq, unsigned char *buf, unsigned long len) {
	struct scatterlist sg[2];
	int ret;

	if (buf == 0) {
		buf = (unsigned char *) alloc_page(0);
#if 1
		if (test_virtio_nob == 1) { /* TODO: this is introduced to burn some cpu cycles, otherwise throughput drops drastically  from 1.6G to 500M , with vhost this is not a issue*/
			unsigned long buf1 = alloc_page(MEM_CLEAR);
			mm_putFreePages(buf1, 0);
		}
#endif
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
		ret = virtio_add_buf_to_queue(vq, sg, 0, 2, (void *) sg[0].page_link, 0);/* recv q*/
	} else {
		ret = virtio_add_buf_to_queue(vq, sg, 2, 0, (void *) sg[0].page_link, 0);/* send q */
	}

	return ret;
}

static int probe_virtio_net_pci(device_t *dev) {
	if (dev->pci_hdr.device_id == VIRTIO_PCI_NET_DEVICE_ID) {
		return 1;
	}
	return 0;
}


static int attach_virtio_net_pci(device_t *pci_dev) {
	unsigned long addr;
	unsigned long features;
	int i;
	pci_dev_header_t *pci_hdr = &pci_dev->pci_hdr;
	virtio_dev_t *virtio_dev = (virtio_dev_t *) pci_dev->private_data;
	uint32_t msi_vector;


	virtio_set_status(virtio_dev,
			virtio_get_status(virtio_dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("VirtioNet: Initializing VIRTIO PCI NET status :%x : virtio_dev:%x \n", virtio_get_status(virtio_dev),virtio_dev);

	virtio_set_status(virtio_dev,
			virtio_get_status(virtio_dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = virtio_dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	VirtioNet:  hostfeatures :%x: dev:%x capabilitie:%x\n", features,pci_dev,pci_hdr->capabilities_pointer);
	display_virtiofeatures(features, vtnet_feature_desc);

	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = read_msi(pci_dev);
		if (msi_vector > 0)
			enable_msix(pci_dev);
	} else {
		msi_vector = 0;
	}

	if (msi_vector == 0) {
		addr = virtio_dev->pci_ioaddr + 20;
	} else {
		addr = virtio_dev->pci_ioaddr + 24;
	}
	for (i = 0; i < 6; i++)
		pci_dev->mac[i] = inb(addr + i);
	ut_log("	VirtioNet:  pioaddr:%x MAC address : %x :%x :%x :%x :%x :%x mis_vector:%x   :\n",addr, pci_dev->mac[0], pci_dev->mac[1], pci_dev->mac[2], pci_dev->mac[3], pci_dev->mac[4], pci_dev->mac[5],msi_vector);

	virtio_createQueue(0, virtio_dev, 1);
	if (msi_vector > 0) {
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0);
	}
	virtio_createQueue(1, virtio_dev, 2);
	if (msi_vector > 0) {
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 1);
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0xffff);
	}

	virtio_disable_cb(virtio_dev->vq[1]); /* disable interrupts on sending side */
#if 1
	if (msi_vector > 0) {
		for (i = 0; i < 3; i++)
			ar_registerInterrupt(msi_vector + i, virtio_net_interrupt,
					"virt_net_msi",pci_dev);
	}
#endif

	for (i = 0; i < 120; i++) /* add buffers to recv q */
		addBufToQueue(virtio_dev->vq[0], 0, 4096);

	inb(virtio_dev->pci_ioaddr + VIRTIO_PCI_ISR);
	virtio_queue_kick(virtio_dev->vq[0]);

	registerNetworkHandler(NETWORK_DRIVER, netdriver_xmit, (void *) pci_dev);

	virtio_set_status(virtio_dev,
			virtio_get_status(virtio_dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	ut_log("VirtioNet:  Initilization Completed status:%x\n",virtio_get_status(virtio_dev));
	return 1;
}

static int dettach_virtio_net_pci(device_t *pci_dev) { //TODO
	return 0;
}
static spinlock_t virtionet_lock = SPIN_LOCK_UNLOCKED("virtio_net");
static int virtio_net_poll_device(void *private_data, int enable_interrupt, int total_pkts){
	unsigned char *addr;
	unsigned int len = 0;
	device_t *pci_dev = (device_t *) private_data;
	unsigned char *replace_buf;
	virtio_dev_t *dev = (virtio_dev_t *) pci_dev->private_data;
	int i;
	int ret=0;

	for (i = 0; i < total_pkts; i++) {
		addr = (unsigned char *) virtio_removeFromQueue(dev->vq[0],
				(unsigned int *) &len);
		if (addr != 0){
			netif_rx(addr, len, &replace_buf);
			addBufToQueue(dev->vq[0], replace_buf, 4096);
			ret = ret+1;
		}else{
			break;
		}
	}
	if (ret > 0){
		virtio_queue_kick(dev->vq[0]);
	}
	if (enable_interrupt){
		virtio_enable_cb(dev->vq[0]);
	}
	return ret;
}
static void virtio_net_interrupt(registers_t regs, void *private_data) {
	/* reset the irq by resetting the status  */
	unsigned char isr;
	unsigned int len = 0;
	unsigned char *addr;
	device_t *pci_dev = (device_t *) private_data;
	virtio_dev_t *dev;
	unsigned long flags;
	unsigned char *replace_buf;
	int i;
//BRK;
//	spin_lock_irqsave(&virtionet_lock, flags);

	dev = (virtio_dev_t *) pci_dev->private_data;
	if (dev->msi == 0)
		isr = inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
#if 1
	virtio_disable_cb(dev->vq[0]);
	netif_rx_enable_polling(private_data, virtio_net_poll_device);
	//virtio_net_poll_device(private_data,0,10);
#else
	for (i = 0; i < 10; i++) {
		addr = (unsigned char *) virtio_removeFromQueue(dev->vq[0],
				(unsigned int *) &len);
		if (addr != 0){
			netif_rx(addr, len, &replace_buf);
			addBufToQueue(dev->vq[0], replace_buf, 4096);
			virtio_queue_kick(dev->vq[0]);
		}else{
			break;
		}
	}
#endif
//	spin_unlock_irqrestore(&virtionet_lock, flags);
}

int virtio_send_errors = 0;
static int netdriver_xmit(unsigned char* data, unsigned int len,
		void *private_data) {
	device_t *pci_dev = (device_t *) private_data;
	static int send_pkts = 0;
	virtio_dev_t *dev;
	int i, ret;
	unsigned long flags;

	dev = (virtio_dev_t *) pci_dev->private_data;
	if (dev == 0 || data==0)
		return 0;

	unsigned char *addr;
	addr = (unsigned char *) alloc_page(0);
    if (addr ==0) return 0;
    ut_memset(addr,0,10);
	ut_memcpy(addr+10,data,len);
	ret = -ENOSPC;

	spin_lock_irqsave(&virtionet_lock, flags);
	ret = addBufToQueue(dev->vq[1], (unsigned char *) addr, len+10);
	send_pkts++;
	if (ret == -ENOSPC) {
		mm_putFreePages(addr, 0);
		virtio_send_errors++;
	}
	send_pkts = 0;
	virtio_queue_kick(dev->vq[1]);
	spin_unlock_irqrestore(&virtionet_lock, flags);
#if 1
	i = 0;
	while (i < 50) {
		i++;
		spin_lock_irqsave(&virtionet_lock, flags);
		addr = (unsigned long) virtio_removeFromQueue(dev->vq[1], &len);
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


static int shutdown_netfront(void *dev) { //TODO
	return 1;
}


DEFINE_DRIVER(virtio_net_pci, virtio_pci, probe_virtio_net_pci, attach_virtio_net_pci, dettach_virtio_net_pci);

