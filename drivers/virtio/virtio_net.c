#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include <virtio.h>
#include <virtio_ring.h>
#include <virtio_pci.h>
#include <virtio_net.h>

extern virtio_dev_t virtio_devices[];
extern int virtio_dev_count;

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

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO PCI NET status :%x :  \n",vp_get_status(dev));

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO PCI NET hostfeatures :%x:\n",features);

	addr = dev->pci_ioaddr + 20;
	DEBUG(" MAC address : %x :%x :%x :%x :%x :%x status: %x:%x  :\n",inb(addr),inb(addr+1),inb(addr+2),inb(addr+3),inb(addr+4),inb(addr+5),inb(addr+6),inb(addr+7));
	for (i=0; i<6; i++)
	  dev->mac[i]=inb(addr+i);
	virtio_createQueue(0, dev, 1);
	virtio_createQueue(1, dev, 2);
	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");
	virtio_addToQueue(dev->vq[0], 0, 4096);
	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
}

static int send_packet(struct virtqueue *vq, unsigned char *c, unsigned long len){
	c[10]=0x88;
	c[11]=0x2;
	c[12]=0x3;
	c[13]=0x4;
	c[13]=0x5;
	c[13]=0x6;
	virtio_addToQueue(vq, c, len);
	return 1;
}
#if 1
void virtio_net_interrupt(registers_t regs) {
	unsigned int len,index;
	unsigned long addr;
	unsigned char *c;
	unsigned char isr;
	static int i=0;
	isr = inb(virtio_devices[0].pci_ioaddr + VIRTIO_PCI_ISR);

index=0;
i++;

	//for (index = 0; index < 1; index++) {
		/* reset the irq by resetting the status  */
		len = 0;
		addr = virtio_removeFromQueue(virtio_devices[0].vq[0], &len);
		if (addr == 0) {
			DEBUG("ISR EMPTY\n");
			return;
		}
		c = addr;
		DEBUG("NEW6666666ISR:%d :%x  tot:%d addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n",index,isr,virtio_dev_count,addr,len,c[0],c[1],c[2],c[3],c[10],c[11],c[12],c[13]);
	//	if (index==0) { /* recveive q */

			virtio_addToQueue(virtio_devices[0].vq[0], 0, 4096);/* TODO hardcoded 4096*/

		//	send_packet(virtio_devices[0].vq[1], c, len);
	//	}else { /* send q */
	//		mm_putFreePages(addr,0);
	//	}
	//}
}
#else
void virtio_net_interrupt(registers_t regs) {
	unsigned int len,index;
	unsigned long addr;
	unsigned char *c;
	unsigned char isr;
	static int i=0;
	virtio_dev_t *dev = &virtio_devices[0];

	isr = inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
i++;
	for (index = 0; index < 2; index++) {
		/* reset the irq by resetting the status  */
		len = 0;
		addr = virtio_removeFromQueue(dev->vq[index], &len);
		if (addr == 0) {
			continue;
		}
		c = addr;
		DEBUG("%d:NEW ISR:%d :%x  tot:%d addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n",i,index,isr,virtio_dev_count,addr,len,c[0],c[1],c[2],c[3],c[10],c[11],c[12],c[13]);
		if (index==0) { /* recveive q */
			if (dev->rx_func != 0){
				dev->rx_func( c+sizeof(struct virtio_net_hdr), len-sizeof(struct virtio_net_hdr));/* TODO : need to free the buf */
			}
			virtio_addToQueue(dev->vq[0], 0, 4096);/* TODO hardcoded 4096*/
		}else { /* send q */
			mm_putFreePages(addr,0);
		}
	}
}
#endif

int netfront_xmit(virtio_dev_t  *dev, unsigned char* data, int len) {
	if (dev == 0) return 0;
	 unsigned long addr = mm_getFreePages(MEM_CLEAR, 0);
	 len=4096;
	 if (addr ==0) return 0;

	virtio_addToQueue(dev->vq[1], addr, len,0);
	return 1;
}

void *init_netfront(void(*net_rx)(unsigned char* data, int len),
		unsigned char *rawmac, char **ip) { /* if succesfull return dev */
	int i,j;
	return 0;
	for (i = 0; i < virtio_dev_count; i++) {
		if (virtio_devices[virtio_dev_count].type != 1)
			continue;
		if (rawmac != 0 ) {
			for (j = 0; j < 6; j++)
				rawmac[j] = virtio_devices[virtio_dev_count].mac[i];
		}
		virtio_devices[virtio_dev_count].rx_func = net_rx;
		return &virtio_devices[virtio_dev_count];
	}
}

int shutdown_netfront(void *dev) {

}




