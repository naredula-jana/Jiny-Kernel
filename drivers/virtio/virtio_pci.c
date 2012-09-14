
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
#include <net/virtio_net.h>

#define MAX_VIRIO_DEVICES 10
int virtio_dev_count = 0;
virtio_dev_t virtio_devices[MAX_VIRIO_DEVICES];
pci_dev_header_t virtio_pci_hdr;

extern void virtio_net_interrupt(registers_t regs);
extern void virtio_9p_interrupt(registers_t regs);
extern int init_virtio_net_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev);
extern int init_virtio_9p_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev);

int init_virtio_memballoon_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev);
void virtio_interrupt(registers_t regs);
void virtio_memb_interrupt(registers_t regs);
typedef struct virtio_pciDevices {
	const char *name;
	int pciSubId;
	int (*init)(pci_dev_header_t *pci_hdr, virtio_dev_t *dev,uint32_t *msi_vector);
	void (*isr)(registers_t regs);
};
static struct virtio_pciDevices pciDevices[] = {
		{ .name = "virtio-9p", .pciSubId = 9, .init = init_virtio_9p_pci, .isr = virtio_9p_interrupt, },
		{ .name = "virtio-net", .pciSubId = 1, .init = init_virtio_net_pci, .isr = virtio_net_interrupt, },
		{ .name = "virtio-memballon", .pciSubId = 5, .init = init_virtio_memballoon_pci, .isr = virtio_memb_interrupt, },
		{ .name = 0, .pciSubId = 0, .init =0, .isr = 0, }};

int init_virtio_pci(pci_dev_header_t *pci_hdr, pci_bar_t bars[], uint32_t len,uint32_t *msi_vector) {
	uint32_t ret, i;
	unsigned long features;

	virtio_pci_hdr = *pci_hdr;

	if (virtio_dev_count >= MAX_VIRIO_DEVICES)
		return 0;
	ar_registerInterrupt(32 + pci_hdr->interrupt_line, virtio_interrupt, "virtio_driver");


	if (bars[0].addr != 0 ) {
		virtio_devices[virtio_dev_count].pci_ioaddr = bars[0].addr - 1;
		virtio_devices[virtio_dev_count].pci_iolen = bars[0].len;
		virtio_devices[virtio_dev_count].pci_mmio = bars[1].addr;
		virtio_devices[virtio_dev_count].pci_mmiolen = bars[1].len;
	} else {
		ut_printf(" ERROR in initializing VIRTIO PCI driver %x : %x \n",bars[0].addr,bars[1].addr);
		return 0;
	}

	DEBUG(" Initializing VIRTIO PCI device id:%x ioaddr :%x(%d) mmioadd:%x(%d)\n", pci_hdr->subsys_id, virtio_devices[virtio_dev_count].pci_ioaddr,virtio_devices[virtio_dev_count].pci_iolen,virtio_devices[virtio_dev_count].pci_mmio,virtio_devices[virtio_dev_count].pci_mmiolen);
	virtio_devices[virtio_dev_count].type = pci_hdr->subsys_id;

	for (i = 0; pciDevices[i].pciSubId!=0; i++) {
		if (pciDevices[i].pciSubId == pci_hdr->subsys_id) {
			pciDevices[i].init(pci_hdr, &virtio_devices[virtio_dev_count],msi_vector);
			virtio_devices[virtio_dev_count].type = pci_hdr->subsys_id;
			virtio_devices[virtio_dev_count].msi = *msi_vector;
            bars[0].name=bars[1].name=pciDevices[i].name;
			virtio_devices[virtio_dev_count].isr = pciDevices[i].isr;
			virtio_dev_count++;
			return 1;
		}
	}

	return 0;
}
void virtio_interrupt(registers_t regs) {
	int i;

	for (i = 0; i < virtio_dev_count; i++) {
		unsigned char isr;

		isr = inb(virtio_devices[i].pci_ioaddr + VIRTIO_PCI_ISR);
		if ((isr != 0) && (virtio_devices[i].isr!=0)) {
			virtio_devices[i].isr(regs);
		}
	}

}
int virtio_createQueue(uint16_t index, virtio_dev_t *dev, int qType);


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
	else {
		DEBUG("ERROR in VIRTIO Notify in VIRT queue :%x\n",vq);
	}

	//DEBUG("VIRTIO NOTIFY in VIRT queue kicking :%x\n",vq);
	/* we write the queue's selector into the notification register to
	 * signal the other end */
	outw(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_NOTIFY, index);
}
void display_virtiofeatures(unsigned long feature, struct virtio_feature_desc *desc) {
	int i,j,bit;
	for (i = 0; i < 32; i++) {
		bit = (feature >> i) & (0x1);
		if (bit) {
			for (j = 0; ( desc[j].name != NULL); j++) {
				if (desc[j].feature_bit == i)
					ut_printf("%s,", desc[j].name);
			}
		}
	}
	ut_printf("\n");
}
static void callback(struct virtqueue *vq) {
	DEBUG("  VIRTIO CALLBACK in VIRT queue :%x\n",vq);
}

int virtio_createQueue(uint16_t index, virtio_dev_t *dev, int qType) {
	int size;
	uint16_t num;
	//struct virtio_pci_vq_info *info;
	unsigned long queue;

	outw(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_SEL, index);

	num = inw(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_NUM);
	DEBUG("virtio NUM-%d : %x  :%x\n",index,num,vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	if (num == 0) {
		dev->vq[index] = 0;
		return 0;
	}

	uint32_t pfn = inl(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_PFN);
//	DEBUG(" pfn-%d : %x \n",index,pfn);

	size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));

	DEBUG("Creating PAGES order: %d size:%d  \n",get_order(size),size);
	//vring_size(num);
	queue = mm_getFreePages(MEM_CLEAR, get_order(size));
	/*if (info == 0)
		return 0;*/

	/* activate the queue */
	outl(dev->pci_ioaddr + VIRTIO_PCI_QUEUE_PFN, __pa(queue) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);


	/* create the vring */
	dev->vq[index] = vring_new_virtqueue(num, VIRTIO_PCI_VRING_ALIGN, dev, queue, &notify, &callback, "VIRTQUEUE");
	virtqueue_enable_cb_delayed(dev->vq[index]);
	dev->vq[index]->qType = qType;

	return 1;
}




