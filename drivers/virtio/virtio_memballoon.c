#if 0  /* Need to convert in to c++ , similar to net and p9 */
#define DEBUG_ENABLE 1
#include "common.h"
#include "device.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "virtio.h"
#include "virtio_ring.h"
#include "virtio_pci.h"
#include "mach_dep.h"
static struct virtio_balloon_config {
	 /* Number of pages host wants Guest to give up. */
	uint32_t num_pages ;
	/* Number of pages we've actually got in balloon. */
	uint32_t actual ;
}balloon_config;

#define MAX_TABLE_SIZE 200  /* for every entry it can store 2M pages = 512*4k */
static struct balloon_data_struct{
	/* each entry is a seperate page holding 4k/8=512 pages= 2M */
	unsigned long pages_table[MAX_TABLE_SIZE];
	int current_index;
	int total_pages;
}balloon_data;

static virtio_dev_t *memb_dev = 0;
static void virtio_memb_interrupt(registers_t regs) {
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
static uint32_t buf[512];
static int send_to_memballoon(unsigned long *v, int inflate) {
	struct scatterlist sg[4];
	int i,in,out;
	int queue_id=0;

	for (i = 0; i < 512; i++) {
		buf[i] = __pa(v[i]) >> 12;
	}
	sg[0].page_link = (unsigned long) buf;
	sg[0].length = 1024;
	sg[0].offset = 0;

	if (inflate ==0) queue_id=1;
	out=1;
	in=0;
	virtio_enable_cb(memb_dev->vq[queue_id]);
	virtio_add_buf_to_queue(memb_dev->vq[queue_id], sg, out, in, sg[0].page_link, 0);
	virtio_queuekick(memb_dev->vq[queue_id]);

	return 0;
}
static spinlock_t balloon_lock = SPIN_LOCK_UNLOCKED("mem_balloon");
int Jcmd_balloon(char *arg1, char *arg2) {
	unsigned long flags;
	int inflate = 1;
	int pages = 0;
	int i;
	unsigned long *v;

#if 1
	if (arg1 == 0)
		return 0;

	if (arg1[0] == '-') {
		arg1[0] = '0';
		inflate = 0;
	}
	pages = ut_atoi(arg1);
	if (pages == 0)
		return 0;
#endif
	spin_lock_irqsave(&balloon_lock, flags);
	if (inflate) {
		while (pages > 0) {
			if (balloon_data.current_index >= MAX_TABLE_SIZE) {
				break;
			}
			v = alloc_page(0);
			if (v == 0) {
				break;
			}
			for (i = 0; i < 512; i++) {
				v[i] = alloc_page(0);
				if (v[i] == 0) continue;
			}
			send_to_memballoon(v,inflate);
			balloon_data.pages_table[balloon_data.current_index] = v;
			balloon_data.current_index++;
			ut_printf("Inflate Ballon send the 512 pages=2M to device: %d pages:%d \n",
					balloon_data.current_index,pages);
			pages--;
		}
	} else {
		while (pages > 0) {
			if (balloon_data.current_index <= 0) {
				break;
			}
			balloon_data.current_index--;
			v = balloon_data.pages_table[balloon_data.current_index];
			if (v == 0) {
				break;
			}
			send_to_memballoon(v,inflate);
			for (i = 0; i < 512; i++) {
				if (v[i] == 0) continue;
				mm_putFreePages(v[i], 0);
			}
			mm_putFreePages(v, 0);
			ut_printf("Deflate Ballon send the 512 pages=2M to device: %d pages:%d \n",
					balloon_data.current_index,pages);
			pages--;
		}
	}
	spin_unlock_irqrestore(&balloon_lock, flags);
	return 1;
}
static int attach_virtio_memballon_pci(device_t *pci_dev) {
	unsigned long addr;
	unsigned long features;
	int i;
	static int init_data=0;

	virtio_dev_t *dev = (virtio_dev_t *) pci_dev->private_data;
	uint32_t msi_vector = pci_dev->msix_cfg.isr_vector;
	if (init_data == 0) {
		for (i = 0; i < MAX_TABLE_SIZE; i++)
			balloon_data.pages_table[i] = 0;
		balloon_data.total_pages = 0;
		balloon_data.current_index = 0;
		init_data = 1;
	}

	memb_dev = dev;
	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO  memory balloon status :%x :  \n", virtio_get_status(dev));

	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO  memory balloon hostfeatures :%x:\n", features);

	virtio_createQueue(0, dev, 2);/* both are send queues*/
	virtio_createQueue(1, dev, 2);
	dev->isr = virtio_memb_interrupt;
	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");

	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);

}


static int probe_virtio_memballon_pci(device_t *dev) {
	if (dev->pci_hdr.device_id == VIRTIO_PCI_BALLOON_DEVICE_ID) {
		return 1;
	}
	return 0;
}
static int dettach_virtio_memballon_pci(device_t *pci_dev) {
	return 0;
}
DEFINE_DRIVER(virtio_memballon_pci, virtio_pci, probe_virtio_memballon_pci, attach_virtio_memballon_pci, dettach_virtio_memballon_pci);
#endif

