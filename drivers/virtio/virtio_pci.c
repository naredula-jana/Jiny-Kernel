
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

#define MAX_VIRIO_DEVICES 10
static int virio_dev_count = 0;
virtio_dev_t virtio_devices[MAX_VIRIO_DEVICES];
pci_dev_header_t virtio_pci_hdr;
static void virtio_net_interrupt(registers_t regs) ;


int send_packet(struct virtqueue *vq, unsigned char *c, unsigned long len){
	c[10]=0x88;
	c[11]=0x2;
	c[12]=0x3;
	c[13]=0x4;
	c[13]=0x5;
	c[13]=0x6;
	add_buf_vq(vq, c, len,0);
	return 1;
}

static unsigned char vp_get_status(virtio_dev_t *dev) {
	uint16_t addr = dev->pci_ioaddr + VIRTIO_PCI_STATUS;
	return inb(addr);
}
unsigned long debug_brk = 0;
static void vp_set_status(virtio_dev_t *dev, unsigned char status) {
	uint16_t addr = dev->pci_ioaddr + VIRTIO_PCI_STATUS;
	outb(addr, status);
}
int init_virtio_pci(pci_dev_header_t *pci_hdr, pci_bar_t bars[], uint32_t len) {
	uint32_t ret, i;
	unsigned long features;
	virtio_pci_hdr = *pci_hdr;

	DEBUG(" Initializing VIRTIO PCI \n");
	if (virio_dev_count >= MAX_VIRIO_DEVICES)
		return 0;
	if (bars[0].addr != 0 && bars[1].addr != 0) {
		virtio_devices[virio_dev_count].pci_ioaddr = bars[0].addr - 1;
		virtio_devices[virio_dev_count].pci_iolen = bars[0].len;
		virtio_devices[virio_dev_count].platform_mmio = bars[1].addr;
		virtio_devices[virio_dev_count].platform_mmiolen = bars[1].len;
	} else {
		ut_printf(" ERROR in initializing VIRTIO PCI driver \n");
		return 0;
	}

	if (pci_hdr->interrupt_line > 0) {
		DEBUG(" Interrupt number : %i \n", pci_hdr->interrupt_line);
		ar_registerInterrupt(32 + pci_hdr->interrupt_line,
				virtio_net_interrupt, "virtio_pci");
	}
	DEBUG(" Initializing VIRTIO PCI:%x  :%x :  sub device id:%x\n",virtio_devices[virio_dev_count].pci_ioaddr, vp_get_status(&virtio_devices[virio_dev_count]), pci_hdr->subsys_id);
	virtio_devices[virio_dev_count].type = pci_hdr->subsys_id;
	if (pci_hdr->subsys_id == 9) { /* 9p */
		init_virtio_9p_pci(pci_hdr, &virtio_devices[virio_dev_count]);
		virtio_devices[virio_dev_count].type = 9;
	} else if (pci_hdr->subsys_id == 1) { /* network io */
		init_virtio_net_pci(pci_hdr, &virtio_devices[virio_dev_count]);
		virtio_devices[virio_dev_count].type = 1;
	}
	virio_dev_count++;
	//start_networking();
	return 1;
}
int create_queue_pci(uint16_t index, virtio_dev_t *dev);
int init_virtio_9p_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev) {
	unsigned long addr;
	unsigned long features;

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO PCI 9p status :%x :  \n",vp_get_status(dev));

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO PCI 9p hostfeatures :%x:\n",features);

	create_queue_pci(0, dev);
	create_queue_pci(1, dev);
	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" Initialising VIRTIO 9p PCI COMPLETED with driver ok :%x \n");

	struct scatterlist sg[2];
	sg[0].page_link = mm_getFreePages(MEM_CLEAR, 0);
	sg[0].length = 4090;
	sg[0].offset = 0;
	/*sg[1].page_link = mm_getFreePages(MEM_CLEAR, 0);
	 sg[1].length = 4090;
	 sg[1].offset = 0;*/
	virtqueue_add_buf_gfp(dev->vq[0], sg, 1, 0, sg[0].page_link, 0);
	virtqueue_kick(dev->vq[0]);
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
	create_queue_pci(0, dev);
	create_queue_pci(1, dev);
	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");
	add_buf_vq(dev->vq[0], 0, 4096,1);
	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);


}
int add_buf_vq(struct virtqueue *vq, unsigned long addr, unsigned long len, int directon_recv) {
	struct scatterlist sg[2];

	if (addr == 0){
		 addr = mm_getFreePages(MEM_CLEAR, 0);
		 len=4096;
	}
	sg[0].page_link = addr;
	sg[0].length = sizeof(struct virtio_net_hdr); /* TODO hardcoded */
	sg[0].offset = 0;
	sg[1].page_link = addr + sizeof(struct virtio_net_hdr);
	sg[1].length = len-sizeof(struct virtio_net_hdr);
	sg[1].offset = 0;
	DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (directon_recv == 1) {
	     virtqueue_add_buf_gfp(vq, sg, 0, 2, sg[0].page_link, 0);
	}else {
	     virtqueue_add_buf_gfp(vq, sg, 2, 0, sg[0].page_link, 0);
	}
	virtqueue_kick(vq);
	return 1;
}
int get_order(unsigned long size) {
	int order;

	size = (size - 1) >> (PAGE_SHIFT - 1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

/* the notify function used when creating a virt queue */
static void notify(struct virtqueue *vq) {
	virtio_dev_t *dev = vq->vdev;
	uint16_t index;

	if (dev->vq[0] == vq)
		index = 0;
	else if (dev->vq[1] == vq)
		index = 1;
    else
    	DEBUG("ERROR in VIRTIO Notify in VIRT queue :%x\n",vq);

		DEBUG("VIRTIO NOTIFY in VIRT queue kicking :%x\n",vq);
		/* we write the queue's selector into the notification register to
		 * signal the other end */
		outw(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_NOTIFY, index);
}

static void callback(struct virtqueue *vq) {
	DEBUG("VIRTIO CALLBACK in VIRT queue :%x\n",vq);
}

int create_queue_pci(uint16_t index, virtio_dev_t *dev) {
	int size;
	uint16_t num;
	struct virtio_pci_vq_info *info;
	unsigned long queue;

	outw(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_SEL, index);

	num = inw(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_NUM);
	DEBUG(" NUM-%d : %x  :%x\n",index,num,vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	if (num == 0) {
		dev->vq[index] = 0;
		return 0;
	}

	uint32_t pfn = inl(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_PFN);
	DEBUG(" pfn-%d : %x \n",index,pfn);

	size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	//  info->queue = alloc_pages_exact(size, GFP_KERNEL|__GFP_ZERO);

	DEBUG("Creating PAGES++ %d %d for %d \n",get_order(size),size,index);
	//vring_size(num);
	queue = mm_getFreePages(MEM_CLEAR, get_order(size));
	if (info == 0)
		return 0;

	/* activate the queue */
	outl(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_PFN, __pa(queue)
			>> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	/* create the vring */
	dev->vq[index] = vring_new_virtqueue(num, VIRTIO_PCI_VRING_ALIGN, dev,
			queue, &notify, &callback, "VIRTQUEUE");
	virtqueue_enable_cb_delayed(dev->vq[index]);

	return 1;
}

#if 1
static void virtio_net_interrupt(registers_t regs) {
	unsigned int len,index;
	unsigned long addr;
	unsigned char *c;
	unsigned char isr;
	static int i=0;
	isr = inb(virtio_devices[0].pci_ioaddr + VIRTIO_PCI_ISR);

index=0;
i++;
if (i>4) return ;
	//for (index = 0; index < 1; index++) {
		/* reset the irq by resetting the status  */
		len = 0;
		addr = virtqueue_get_buf(virtio_devices[0].vq[0], &len);
		if (addr == 0) {
			DEBUG("ISR EMPTY\n");
			return;
		}
		c = addr;
		DEBUG("NEW2222ISR:%d :%x  tot:%d addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n",index,isr,virio_dev_count,addr,len,c[0],c[1],c[2],c[3],c[10],c[11],c[12],c[13]);
	//	if (index==0) { /* recveive q */

			add_buf_vq(virtio_devices[0].vq[0], 0, 4096, 1);/* TODO hardcoded 4096*/

		//	send_packet(virtio_devices[0].vq[1], c, len);
	//	}else { /* send q */
	//		mm_putFreePages(addr,0);
	//	}
	//}
}
#else
static void virtio_net_interrupt(registers_t regs) {
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
		addr = virtqueue_get_buf(dev->vq[index], &len);
		if (addr == 0) {
			continue;
		}
		c = addr;
		DEBUG("%d:NEW ISR:%d :%x  tot:%d addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n",i,index,isr,virio_dev_count,addr,len,c[0],c[1],c[2],c[3],c[10],c[11],c[12],c[13]);
		if (index==0) { /* recveive q */
			if (dev->rx_func != 0){
				dev->rx_func( c+sizeof(struct virtio_net_hdr), len-sizeof(struct virtio_net_hdr));/* TODO : need to free the buf */
			}
			add_buf_vq(dev->vq[0], 0, 4096, 1);/* TODO hardcoded 4096*/
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

	add_buf_vq(dev->vq[1], addr, len,0);
	return 1;
}

void *init_netfront(void(*net_rx)(unsigned char* data, int len),
		unsigned char *rawmac, char **ip) { /* if succesfull return dev */
	int i,j;
	return 0;
	for (i = 0; i < virio_dev_count; i++) {
		if (virtio_devices[virio_dev_count].type != 1)
			continue;
		if (rawmac != 0 ) {
			for (j = 0; j < 6; j++)
				rawmac[j] = virtio_devices[virio_dev_count].mac[i];
		}
		virtio_devices[virio_dev_count].rx_func = net_rx;
		return &virtio_devices[virio_dev_count];
	}
}

int shutdown_netfront(void *dev) {

}
