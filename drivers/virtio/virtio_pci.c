
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
int virtio_dev_count = 0;
virtio_dev_t virtio_devices[MAX_VIRIO_DEVICES];
pci_dev_header_t virtio_pci_hdr;

extern void virtio_net_interrupt(registers_t regs) ;
extern void virtio_9p_interrupt(registers_t regs) ;
extern int init_virtio_net_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev);
extern int init_virtio_9p_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev);
typedef struct virtio_pciDevices{
    const char *name;
    int pciSubId;
    int (*init)(pci_dev_header_t *pci_hdr, virtio_dev_t *dev);
    void (*isr)(registers_t regs);
};
struct virtio_pciDevices pciDevices [] = {
		{
				.name = "virtio-9p",
				.pciSubId = 9,
				.init = init_virtio_9p_pci,
				.isr = virtio_9p_interrupt,
		},
		{
				.name = "virtio-nettt",
				.pciSubId = 1,
				.init = init_virtio_net_pci,
				.isr = virtio_net_interrupt,
		}
};

int init_virtio_pci(pci_dev_header_t *pci_hdr, pci_bar_t bars[], uint32_t len) {
	uint32_t ret, i;
	unsigned long features;
	virtio_pci_hdr = *pci_hdr;


	DEBUG(" Initializing VIRTIO PCI \n");
	if (virtio_dev_count >= MAX_VIRIO_DEVICES)
		return 0;
	if (bars[0].addr != 0 && bars[1].addr != 0) {
		virtio_devices[virtio_dev_count].pci_ioaddr = bars[0].addr - 1;
		virtio_devices[virtio_dev_count].pci_iolen = bars[0].len;
		virtio_devices[virtio_dev_count].platform_mmio = bars[1].addr;
		virtio_devices[virtio_dev_count].platform_mmiolen = bars[1].len;
	} else {
		ut_printf(" ERROR in initializing VIRTIO PCI driver \n");
		return 0;
	}


	DEBUG(" Initializing VIRTIO PCI:%x    sub device id:%x\n",virtio_devices[virtio_dev_count].pci_ioaddr,  pci_hdr->subsys_id);
	virtio_devices[virtio_dev_count].type = pci_hdr->subsys_id;

	for (i=0; i<2; i++) {
		if (pciDevices[i].pciSubId == pci_hdr->subsys_id) {
			pciDevices[i].init(pci_hdr, &virtio_devices[virtio_dev_count]);
			virtio_devices[virtio_dev_count].type = pci_hdr->subsys_id;

			if (pci_hdr->interrupt_line > 0) {
							DEBUG(" Interrupt number : %i \n", pci_hdr->interrupt_line);
							ar_registerInterrupt(32 + pci_hdr->interrupt_line,
									pciDevices[i].isr, pciDevices[i].name);

						}
			virtio_dev_count++;
			return 1;
		}
	}

	//start_networking();
	return 1;
}
int virtio_createQueue(uint16_t index, virtio_dev_t *dev, int qType);


int virtio_addToQueue(struct virtqueue *vq, unsigned long buf, unsigned long len) {
	struct scatterlist sg[2];

	if (buf == 0){
		buf = mm_getFreePages(MEM_CLEAR, 0);
		 len=4096;
	}
	sg[0].page_link = buf;
	sg[0].length = sizeof(struct virtio_net_hdr); /* TODO hardcoded */
	sg[0].offset = 0;
	sg[1].page_link = buf + sizeof(struct virtio_net_hdr);
	sg[1].length = len-sizeof(struct virtio_net_hdr);
	sg[1].offset = 0;
	DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (vq->qType == 1) {
	     virtqueue_add_buf_gfp(vq, sg, 0, 2, sg[0].page_link, 0);/* recv q*/
	}else {
	     virtqueue_add_buf_gfp(vq, sg, 2, 0, sg[0].page_link, 0);/* send q */
	}
	virtqueue_kick(vq);
	return 1;
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

int virtio_createQueue(uint16_t index, virtio_dev_t *dev, int qType) {
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
	dev->vq[index]->qType = qType;

	return 1;
}




