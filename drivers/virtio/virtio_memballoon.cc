/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/virtio_memballoon.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/


#if 1  /* Need to convert in to c++ , similar to net and p9 */
#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "virtio_pci.h"
#include "mach_dep.h"
}

static struct virtio_balloon_config {  /* input from qemu to guest using interrupt */
	 /* Number of pages host wants Guest to give up. */
	uint32_t num_pages;
	/* Number of pages we've actually got in balloon. */
	uint32_t actual;
}balloon_config;

#define MAX_TABLE_SIZE 200  /* for every entry it can store 2M pages = 512*4k */
static struct balloon_data_struct{
	/* each entry is a seperate page holding 4k/8=512 pages= 2M */
	unsigned long pages_table[MAX_TABLE_SIZE];
	int current_index;
	int total_pages;
}balloon_data;

virtio_memballoon_jdriver *g_virtio_balloon_driver;
static void virtio_memb_interrupt(void *private_data) {
	virtio_memballoon_jdriver *driver = (virtio_memballoon_jdriver *) private_data;
	jdevice *dev = (jdevice *) driver->device;
	/* reset the irq by resetting the status  */
	unsigned char isr;
	isr = inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	unsigned long config_addr;

	config_addr = dev->pci_device.pci_ioaddr  + 20;
	balloon_config.num_pages= inl(config_addr);
	balloon_config.actual= inl(config_addr+4);
	ut_log("VIRTIO BALLOON :config value:  numpages:%x(%d)  actual:%x(%d) \n",balloon_config.num_pages,balloon_config.num_pages,balloon_config.actual,balloon_config.actual);
}
#if 1
static uint32_t buf[512];
static int send_to_memballoon(unsigned long *v, int inflate) {
	struct scatterlist sg[4];
	int i,in,out;
	int queue_id=0;

	for (i = 0; i < 512; i++) {
#ifdef  MEMBALLOON_4K_PAGE
		buf[i] = __pa(v[i]) >> 12;
#else
		unsigned long addr =(unsigned long)v;
		buf[i] = __pa(addr + (i*PAGE_SIZE)) >> 12;
		//ut_printf("%d : addr :%x \n",i,buf[i]);
#endif

	}
	sg[0].page_link = (unsigned long) buf;
	sg[0].length = 512*4;
	sg[0].offset = 0;

	if (inflate ==0) queue_id=1;
	out=1;
	in=0;

	g_virtio_balloon_driver->send_queues[queue_id]->virtio_enable_cb();
	g_virtio_balloon_driver->send_queues[queue_id]->virtio_add_buf_to_queue(sg, out, in, (void *)sg[0].page_link, 0);
	g_virtio_balloon_driver->send_queues[queue_id]->virtio_queuekick();
	return 0;
}
#endif
static spinlock_t balloon_lock = SPIN_LOCK_UNLOCKED("mem_balloon");
extern "C" {
int Jcmd_balloon(char *arg1, char *arg2) {
	unsigned long flags;
	int inflate = 1;
	int pages = 0;
	int i;
	unsigned long *v;

	if (arg1 == 0)
		return 0;

	if (arg1[0] == '-') {
		arg1[0] = '0';
		inflate = 0;
	}
	pages = ut_atoi(arg1,FORMAT_DECIMAL);
	if (pages == 0)
		return 0;

	inflate = ut_atoi(arg2,FORMAT_DECIMAL);
	ut_printf(" inflate:%d  pages:%d \n",inflate,pages);

	spin_lock_irqsave(&balloon_lock, flags);
	if (inflate) {
		while (pages > 0) {
			if (balloon_data.current_index >= MAX_TABLE_SIZE) {
				break;
			}
#ifdef  MEMBALLOON_4K_PAGE
			v = alloc_page(0);
#else
			v= mm_getFreePages(0, 9); /* allocate 2M page */
#endif
			if (v == 0) {
				ut_printf("Inflate Ballon Fail to allocate the Page \n");
				break;
			}

#ifdef  MEMBALLOON_4K_PAGE
			for (i = 0; i < 512; i++) {
				v[i] = alloc_page(0);
				if (v[i] == 0) continue;
			}
#endif
			send_to_memballoon(v,inflate);
			balloon_data.pages_table[balloon_data.current_index] = v;
			balloon_data.current_index++;
			ut_printf("Inflate Ballon send the 512 pages=2M to device: %d pages:%d  addr:%x\n",
					balloon_data.current_index,pages,v);
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
#ifdef  MEMBALLOON_4K_PAGE
			for (i = 0; i < 512; i++) {
				if (v[i] == 0) continue;
				mm_putFreePages(v[i], 0);
			}
			mm_putFreePages(v, 0);
#else
			mm_putFreePages(v, 9); /* free 2M page */
#endif
			ut_printf("Deflate Ballon send the 512 pages=2M to device: %d pages:%d \n",
					balloon_data.current_index,pages);
			pages--;
		}
	}
	spin_unlock_irqrestore(&balloon_lock, flags);
	return 1;
}
}
virtio_memballoon_jdriver::virtio_memballoon_jdriver(class jdevice *jdev){
	name = "virtio_mem_balloon";
	device = jdev;
	max_vqs = 2;
}

jdriver *virtio_memballoon_jdriver::attach_device(class jdevice *dev) {
	unsigned long addr;
	unsigned long features;
	static int init_data=0;
	pci_device_t *pci_dev = &(dev->pci_device) ;

	pci_dev_header_t *pci_hdr = &dev->pci_device.pci_header;
	unsigned long pci_ioaddr = dev->pci_device.pci_ioaddr;

	device = dev;
	uint32_t msi_vector = pci_dev->msix_cfg.isr_vector;
	if (init_data == 0) {
		int i;
		for (i = 0; i < MAX_TABLE_SIZE; i++){
			balloon_data.pages_table[i] = 0;
		}
		balloon_data.total_pages = 0;
		balloon_data.current_index = 0;
		init_data = 1;
	}

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO  memory balloon status :%x :  \n", virtio_get_status(dev));

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log(" driver Initialising VIRTIO  memory balloon hostfeatures :%x:\n", features);
	addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	outl(addr, features);
	send_queues[0] = jnew_obj(memballoon_virtio_queue, device, 0, VQTYPE_SEND);
	send_queues[1] = jnew_obj(memballoon_virtio_queue, device, 1, VQTYPE_SEND);
	recv_queue = jnew_obj(memballoon_virtio_queue, device, 2, VQTYPE_RECV);

	g_virtio_balloon_driver = this;
	ar_registerInterrupt(32 + pci_dev->pci_header.interrupt_line, virtio_memb_interrupt, "virt_memballoon_irq", (void *) this);
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");

	inb(pci_ioaddr + VIRTIO_PCI_ISR);
	return (jdriver *) this;
}

int virtio_memballoon_jdriver::probe_device(class jdevice *jdev) {
	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id
					== VIRTIO_PCI_BALLOON_DEVICE_ID)) {
		ut_log(" Matches inside the MEMBALLOONING.... \n");
		return JSUCCESS;
	}
	return JFAIL;
}

int virtio_memballoon_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}

int virtio_memballoon_jdriver::read(unsigned char *buf, int len, int offset,
		int read_ahead) {
	int ret;

	return 0;
}
int virtio_memballoon_jdriver::write(unsigned char *buf, int len, int offset) {
	int ret;
	return 0;
}
int virtio_memballoon_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	return 0;

}
void virtio_memballoon_jdriver::print_stats(unsigned char *arg1, unsigned char *arg2) {
	if (device ==0){
		ut_printf(" driver not attached to device \n");
		return;
	}
	send_queues[0]->print_stats(0, 0);
	send_queues[1]->print_stats(0, 0);
	recv_queue->print_stats(0, 0);
}
#endif

