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

	addr = dev->pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	features = 0x710;
	outl(addr, features);
	DEBUG(" driver Initialising VIRTIO PCI NET GUESTfeatures :%x:\n", features);

	addr = dev->pci_ioaddr + 20;
	DEBUG(
			" MAC address : %x :%x :%x :%x :%x :%x status: %x:%x  :\n", inb(addr), inb(addr+1), inb(addr+2), inb(addr+3), inb(addr+4), inb(addr+5), inb(addr+6), inb(addr+7));
	for (i = 0; i < 6; i++)
		dev->mac[i] = inb(addr + i);
	virtio_createQueue(0, dev, 1);
	virtio_createQueue(1, dev, 2);
	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");

	for (i = 0; i < 50; i++) /* add buffers to recv q */
		virtio_netAddToQueue(dev->vq[0], 0, 4096);

	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
	virtqueue_kick(dev->vq[0]);

}

void virtio_net_interrupt(registers_t regs) {
	unsigned int len;
	unsigned long addr;
	unsigned char *c;
	unsigned char isr;
	static int i = 0;
	virtio_dev_t *dev = net_dev;

	/* reset the irq by resetting the status  */
	isr = inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);

	i = 0;
	while (i < 10) { /* receive queue */
		i++;
		len = 0;
		addr = virtio_removeFromQueue(dev->vq[0], &len);
		if (addr == 0) {
			break;
		}
		c = addr;
		//DEBUG("%d: new NEW ISR:%d :%x addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n", i, index, isr, addr, len, c[0], c[1], c[2], c[3], c[10], c[11], c[12], c[13]);


		virtio_netAddToQueue(dev->vq[0], 0, 4096);/* TODO hardcoded 4096*/
		virtqueue_kick(dev->vq[0]);
		if (dev->rx_func != 0) {
			dev->rx_func(c ,len);/* TODO : need to free the buf */
		}else{
			mm_putFreePages(c, 0);
		}
	//	send_packet(dev->vq[1], c, len);

	}

	i = 0;
	while (i < 10) { /* send queue */
		i++;
		addr = virtio_removeFromQueue(dev->vq[1], &len);
		if (addr) {
			mm_putFreePages(addr, 0);
		} else {
			break;
		}
	}

}

int netfront_xmit(virtio_dev_t *dev, unsigned char* data, int len) {

	if (dev == 0) return 0;

	unsigned long addr = mm_getFreePages(MEM_CLEAR, 0);
	len = 4096;
	if (addr == 0)
		return 0;
	ut_memcpy(addr, data, len);
	virtio_netAddToQueue(dev->vq[1], addr, len, 0);
	virtqueue_kick(dev->vq[1]);
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




