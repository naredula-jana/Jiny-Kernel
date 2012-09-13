#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "virtio.h"
#include "virtio_ring.h"
#include "virtio_pci.h"
static struct virtio_balloon_config {
	uint32_t num_pages ;
	uint32_t actual ;
}balloon_config;


static virtio_dev_t *memb_dev = 0;

int init_virtio_memballoon_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev,uint32_t *msi_vector) {
	unsigned long addr;
	unsigned long features;
	int i;

	memb_dev = dev;
	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO  memory balloon status :%x :  \n", virtio_get_status(dev));

	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO  memory balloon hostfeatures :%x:\n", features);

	virtio_createQueue(0, dev, 2);/* both are send queues*/
	virtio_createQueue(1, dev, 2);

	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");

	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
}

void virtio_memb_interrupt(registers_t regs) {
	/* reset the irq by resetting the status  */
	unsigned char isr;
	//isr = inb(memb_dev->pci_ioaddr + VIRTIO_PCI_ISR);
	ut_printf("GOT MEM BALLOON interrupt\n");
	unsigned long config_addr;
	config_addr = memb_dev->pci_ioaddr + 20;
	balloon_config.num_pages= inl(config_addr);
	balloon_config.actual= inl(config_addr+4);
	ut_printf("config value:  numpages:%x(%d)  actual:%x(%d) \n",balloon_config.num_pages,balloon_config.num_pages,balloon_config.actual,balloon_config.actual);
}



