#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "../virtio.h"
#include "../virtio_ring.h"
#include "../virtio_pci.h"
#include "virtio_net.h"

static virtio_dev_t *net_dev = 0;

static unsigned char vp_get_status(virtio_dev_t *dev) {
	uint16_t addr = dev->pci_ioaddr + VIRTIO_PCI_STATUS;
	return inb(addr);
}
static void vp_set_status(virtio_dev_t *dev, unsigned char status) {
	uint16_t addr = dev->pci_ioaddr + VIRTIO_PCI_STATUS;
	outb(addr, status);
}
 int addBufToQueue(struct virtqueue *vq, unsigned char *buf,
		unsigned long len) {
	struct scatterlist sg[2];

	if (buf == 0) {
		buf = mm_getFreePages(0, 0);
		ut_memset(buf,0,sizeof(struct virtio_net_hdr));

		if (1){ /* TODO: this is introduced to burn some cpu cycles, otherwise throughput drops drastically  from 1.6G to 500M */
			unsigned long buf1=mm_getFreePages(MEM_CLEAR, 0);
			mm_putFreePages(buf1, 0);
		}

		len = 4096; /* page size */
	}
	sg[0].page_link = buf;
	sg[0].length = sizeof(struct virtio_net_hdr);
	sg[0].offset = 0;
	sg[1].page_link = buf + sizeof(struct virtio_net_hdr);
	sg[1].length = len - sizeof(struct virtio_net_hdr);
	sg[1].offset = 0;
	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (vq->qType == 1) {
		virtqueue_add_buf_gfp(vq, sg, 0, 2, sg[0].page_link, 0);/* recv q*/
	} else {
		virtqueue_add_buf_gfp(vq, sg, 2, 0, sg[0].page_link, 0);/* send q */
	}

	return 1;
}

int init_virtio_net_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev) {
	unsigned long addr;
	unsigned long features;
	int i;

	net_dev = dev;

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO PCI NET status :%x :  \n", vp_get_status(dev));

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO PCI NET hostfeatures :%x:\n", features);

	/*addr = dev->pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	 features = 0x710;
	 outl(addr, features);*/
	DEBUG(" driver Initialising VIRTIO PCI NET GUESTfeatures :%x:\n", features);

	addr = dev->pci_ioaddr + 20;
	DEBUG(
			" MAC address : %x :%x :%x :%x :%x :%x status: %x:%x  :\n", inb(addr), inb(addr+1), inb(addr+2), inb(addr+3), inb(addr+4), inb(addr+5), inb(addr+6), inb(addr+7));
	for (i = 0; i < 6; i++)
		dev->mac[i] = inb(addr + i);
	virtio_createQueue(0, dev, 1);
	virtio_createQueue(1, dev, 2);
	virtqueue_disable_cb(dev->vq[1]); /* disable interrupts on sending side */


	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");

	for (i = 0; i < 120; i++) /* add buffers to recv q */
		addBufToQueue(dev->vq[0], 0, 4096);

	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
	virtqueue_kick(dev->vq[0]);

}

static int s_i = 0;
static int s_r = 0;
int stop_queueing_bh=0;

#if 0
void virtio_net_interrupt(registers_t regs) {
	unsigned int len;
	unsigned long addr;
	unsigned char *c;
	unsigned char isr;
	static int i = 0;
	virtio_dev_t *dev = net_dev;

	/* reset the irq by resetting the status  */
	isr = inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);

	s_i++;
	i = 0;
	while (i < 10) { /* receive queue */
		len = 0;
		if (stop_queueing_bh>0) break;
		addr = virtio_removeFromQueue(dev->vq[0], &len);
		if (addr == 0) {
			break;
		}
		c = addr;
		//DEBUG("%d: new NEW ISR:%d :%x addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n", i, index, isr, addr, len, c[0], c[1], c[2], c[3], c[10], c[11], c[12], c[13]);

		addBufToQueue(dev->vq[0], 0, 4096);/* TODO hardcoded 4096*/
		virtqueue_kick(dev->vq[0]);
		if (dev->rx_func != 0) {
			dev->rx_func(c, len);
		} else {
			mm_putFreePages(c, 0);
		}
		i++;
	}

	if (i > 0) {
		s_r++;
		//virtqueue_kick(dev->vq[0]);
	}
}
#else
extern queue_t nbh_waitq;
extern int netbh_started;
void virtio_net_interrupt(registers_t regs) {
	/* reset the irq by resetting the status  */
	unsigned char isr;
	isr = inb(net_dev->pci_ioaddr + VIRTIO_PCI_ISR);
	if (netbh_started == 1){
	    sc_wakeUp(&nbh_waitq, NULL);
	    virtqueue_disable_cb(net_dev->vq[0]);
	}
}
#endif

int netfront_xmit(virtio_dev_t *dev, unsigned char* data, int len) {

	int i;
	if (dev == 0)
		return 0;

	unsigned long addr;

	addr = data;
	addBufToQueue(dev->vq[1], addr, len);
	virtqueue_kick(dev->vq[1]);
#if 1
	i = 0;
	while (i < 3) {
		i++;
		addr = virtio_removeFromQueue(dev->vq[1], &len);
		if (addr) {
			mm_putFreePages(addr, 0);
		} else {
			return 1;
		}
	}
#endif
	return 1;
}

void *init_netfront(void (*net_rx)(unsigned char* data, int len),
		unsigned char *rawmac, char **ip) { /* if succesfull return dev */
	int j;

	if (net_dev == 0)
		return 0;

	if (rawmac != 0) {
		for (j = 0; j < 6; j++)
			rawmac[j] = net_dev->mac[j];
	}

	net_dev->rx_func = net_rx;
	return net_dev;
}

int shutdown_netfront(void *dev) {

}




