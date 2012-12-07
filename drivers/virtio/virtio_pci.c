
#define DEBUG_ENABLE 1
#include "common.h"
#include "device.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include <virtio.h>
#include <virtio_ring.h>
#include <virtio_pci.h>
#include <net/virtio_net.h>
#include "mach_dep.h"

#define MAX_VIRIO_DEVICES 10
int virtio_dev_count = 0;
virtio_dev_t virtio_devices[MAX_VIRIO_DEVICES];
pci_dev_header_t virtio_pci_hdr;

static void virtio_interrupt(registers_t regs);

extern device_class_t deviceClass_virtio_pci;
static int probe_virtio_pci(device_t *dev) {
	device_class_t *devClass;

	if (dev->pci_hdr.vendor_id != VIRTIO_PCI_VENDOR_ID)
		return 0;

	devClass = deviceClass_virtio_pci.children;
	while (devClass != NULL) {
		if (devClass->probe != 0 && devClass->probe(dev) == 1) {
			dev->devClass = devClass;
			return 1;
		}
		devClass = devClass->sibling;
	}
	return 0;
}
static int attach_virtio_pci(device_t *dev) {
	pci_dev_header_t *pci_hdr;
	pci_bar_t *bars;
	int len;
	int ret;


	if (read_pci_info(dev) != 1)
		return 0;

	pci_hdr = &dev->pci_hdr;
	bars = &dev->pci_bars[0];
	len = dev->pci_bar_count;

	virtio_pci_hdr = *pci_hdr;

	if (virtio_dev_count >= MAX_VIRIO_DEVICES)
		return 0;
	ar_registerInterrupt(32 + pci_hdr->interrupt_line, virtio_interrupt,
			"virtio_driver",NULL);

	if (bars[0].addr != 0) {
		virtio_devices[virtio_dev_count].pci_ioaddr = bars[0].addr - 1;
		virtio_devices[virtio_dev_count].pci_iolen = bars[0].len;
		virtio_devices[virtio_dev_count].pci_mmio = bars[1].addr;
		virtio_devices[virtio_dev_count].pci_mmiolen = bars[1].len;
	} else {
		ut_printf(" ERROR in initializing VIRTIO PCI driver %x : %x \n",
				bars[0].addr, bars[1].addr);
		return 0;
	}

	DEBUG(
			" Initializing VIRTIO PCI device id:%x ioaddr :%x(%d) mmioadd:%x(%d)\n", pci_hdr->subsys_id, virtio_devices[virtio_dev_count].pci_ioaddr, virtio_devices[virtio_dev_count].pci_iolen, virtio_devices[virtio_dev_count].pci_mmio, virtio_devices[virtio_dev_count].pci_mmiolen);
	virtio_devices[virtio_dev_count].type = pci_hdr->subsys_id;

	dev->private_data = &virtio_devices[virtio_dev_count];
	ret = dev->devClass->attach(dev);
	virtio_devices[virtio_dev_count].type = pci_hdr->subsys_id;

	virtio_dev_count++;
	return ret;

}

static int dettach_virtio_pci(device_t *dev) {
return 0;
}

static void virtio_interrupt(registers_t regs) {
	int i;

	for (i = 0; i < virtio_dev_count; i++) {
		unsigned char isr;

		isr = inb(virtio_devices[i].pci_ioaddr + VIRTIO_PCI_ISR);

#if 1  //TODO
		if ((isr != 0) && (virtio_devices[i].isr!=0)) {
			virtio_devices[i].isr(regs);
		}
#endif
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

DEFINE_DRIVER(virtio_pci, root, probe_virtio_pci, attach_virtio_pci, dettach_virtio_pci);


