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
static int send_9pmessage(virtio_dev_t *dev) {
	unsigned char *p="9P2000.u" ;

	 unsigned char *msg = mm_getFreePages(MEM_CLEAR, 0);

	 msg[0]=0x15;
	 msg[4]=0x64; /* t version */
	 msg[5]=msg[6]=0xff;
	 msg[7]=0x18;
	 msg[8]=0x20;
	 ut_strcpy(&msg[11],"9P2000.u");

	virtio_addToP9Queue(dev->vq[0], msg, 4096);
}
int virtio_addToP9Queue(struct virtqueue *vq, unsigned long buf, unsigned long len) {
	struct scatterlist sg[5];

	if (buf == 0){
		buf = mm_getFreePages(MEM_CLEAR, 0);
		 len=4096;
	}
	sg[0].page_link = buf;
	sg[0].length = len/4;
	sg[0].offset = 0;
	sg[1].page_link = buf + len/4;
	sg[1].length = len/4;
	sg[1].offset = 0;
	sg[2].page_link = buf+ len*(2/4);
	sg[2].length = len/4;
	sg[2].offset = 0;
	sg[3].page_link = buf + len*(3/4);
	sg[3].length = len/4;
	sg[3].offset = 0;
	DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (vq->qType == 1) {
	     virtqueue_add_buf_gfp(vq, sg, 2, 2, sg[0].page_link, 0);/* recv q*/
	}else {
	     virtqueue_add_buf_gfp(vq, sg, 2, 2, sg[0].page_link, 0);/* send q */
	}
	virtqueue_kick(vq);
	return 1;
}
int init_virtio_9p_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev) {
	unsigned long addr;
	unsigned long features;

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO PCI p9 status :%x :  \n",vp_get_status(dev));

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO PCI 9P hostfeatures :%x:\n",features);

	virtio_createQueue(0, dev, 2);

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising..9P INPUT  VIRTIO PCI COMPLETED with driver ok :%x \n",vp_get_status(dev));
	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);

	send_9pmessage(dev);
}
void virtio_9p_interrupt(registers_t regs) {
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
			DEBUG("9p ISR EMPTY\n");
			return;
		}
		c = addr;
		DEBUG("NEW9pISR:%d :%x  tot:%d addd:%x len:%x c:%x:%x:%x:%x :%x:%x:%x:%x\n",index,isr,virtio_dev_count,addr,len,c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7]);



			//send_packet(virtio_devices[0].vq[1], c, len);
	//	}else { /* send q */
	//		mm_putFreePages(addr,0);
	//	}
	//}
}
