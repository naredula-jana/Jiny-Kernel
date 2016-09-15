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
int Jcmd_balloon_auto = 0;
}

static struct virtio_balloon_config {  /* input from qemu to guest using interrupt */
	 /* Number of pages host wants Guest ned to give up pages.
	  *  example: balloon 10 for 256M vm , means guest need  reduces to 10M, and giveup 246M or 246*256 4k-pages, then num_pages=62976
	  *        balloon 100 for 256M vm , means guest need to reduce , and giveup to 256-100=156M or 156*256=39936 */
	uint32_t num_pages;

	/* Number of pages we've actually got in balloon. */
	uint32_t actual;
}balloon_config;

#define MAX_TABLE_SIZE 2000  /* for every entry it can store 2M pages = 512*4k */
static struct balloon_data_struct{
	/* each entry is a seperate page holding 4k/8=512 pages= 2M */
	unsigned long pages_table[MAX_TABLE_SIZE];
	int current_index;
	int total_pages;
}balloon_data;

virtio_memballoon_jdriver *g_virtio_balloon_driver;
#define VIRTIO_BALLOON_S_SWAP_IN  0   /* Amount of memory swapped in */
#define VIRTIO_BALLOON_S_SWAP_OUT 1   /* Amount of memory swapped out */
#define VIRTIO_BALLOON_S_MAJFLT   2   /* Number of major faults */
#define VIRTIO_BALLOON_S_MINFLT   3   /* Number of minor faults */
#define VIRTIO_BALLOON_S_MEMFREE  4   /* Total amount of free memory */
#define VIRTIO_BALLOON_S_MEMTOT   5   /* Total amount of memory */
#define VIRTIO_BALLOON_S_NR       6

struct virtio_balloon_stat {
	uint16_t tag;
	uint64_t val;
} __attribute__((packed));
static struct virtio_balloon_stat vb_stats[6];

static inline void update_stat(virtio_balloon_stat *stats, int idx,
		uint16_t tag, uint64_t val){
	stats[idx].tag = tag;
	stats[idx].val = val;
}

#define pages_to_bytes(x) ((uint64_t)(x) << PAGE_SHIFT)

extern int g_nr_free_pages;
extern unsigned long g_stat_mem_size;
static void update_balloon_stats(){
	int idx = 0;
	struct scatterlist sg[4];
	int len;
	g_virtio_balloon_driver->stat_queue->virtio_removeFromQueue(&len);
	if (g_virtio_balloon_driver->stat_queue->stat_add_success > (g_virtio_balloon_driver->stat_queue->stat_rem_success+3)){
		//ut_log("MEM BALLOON FAIL to update: adds:%d removes:%d \n",g_virtio_balloon_driver->stat_queue->stat_add_success,g_virtio_balloon_driver->stat_queue->stat_rem_success);
		return;
	}
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MEMFREE, g_nr_free_pages*PAGE_SIZE);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MEMTOT, g_stat_mem_size);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_SWAP_OUT, 0);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MINFLT, 0);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MAJFLT, 0);

	//ut_log("MEM Balloon update status, free :%d(%x) total:%d(%x)\n",g_nr_free_pages*PAGE_SIZE,g_nr_free_pages*PAGE_SIZE,g_stat_mem_size,g_stat_mem_size);

	sg[0].page_link = (unsigned long) vb_stats;
	sg[0].length = sizeof(vb_stats);
	sg[0].offset = 0;

	g_virtio_balloon_driver->stat_queue->virtio_enable_cb();
	g_virtio_balloon_driver->stat_queue->virtio_add_buf_to_queue(sg, 1, 0, (void *)sg[0].page_link, 0);
	g_virtio_balloon_driver->stat_queue->virtio_queuekick();

	g_virtio_balloon_driver->stat_queue->virtio_removeFromQueue(&len);
}
static int balloon_pages(int pages, int inflate);
void balloon_thread(){
	while (1){
		sc_sleep(500); /* sleep for 5 sec */

	if (Jcmd_balloon_auto == 1){
		int diff = balloon_config.num_pages - balloon_config.actual;
		if (diff > 0){ /* deflate */
			balloon_pages(diff, 0);
		}else if (diff< 0) { /* inflate */
			balloon_pages(-diff, 1);
		}
	}

		update_balloon_stats();
	}
}
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


	//ut_log("VIRTIO BALLOON interrupt :config value:  numpages:%x(%d)  actual:%x(%d) \n",balloon_config.num_pages,balloon_config.num_pages,balloon_config.actual,balloon_config.actual);
}
#if 1
static uint32_t buf[512];
static int send_to_memballoon(unsigned long *v, int inflate) {
	struct scatterlist sg[4];
	int i,in,out;
	int queue_id=0;
	int len;

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

	g_virtio_balloon_driver->send_queues[queue_id]->virtio_removeFromQueue(&len);

	return 0;
}
#endif
static spinlock_t balloon_lock = SPIN_LOCK_UNLOCKED("mem_balloon");
extern "C" {
int Jcmd_balloon(char *arg1, char *arg2) {
	int inflate = 1;
	int pages = 0;

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
	ut_printf(" INFLATE:%d  pages:%d \n",inflate,pages);
	return balloon_pages(pages, inflate);
}
}
static int balloon_pages(int pages, int inflate){
	unsigned long flags;
	unsigned long *v;
	int i;

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
			pages--;
#if 1
			unsigned char *testp = (unsigned char *)v;
			*testp =0 ; /* touch the memory to bring the page */
#endif
#ifdef  MEMBALLOON_4K_PAGE
			for (i = 0; i < 512; i++) {
				v[i] = alloc_page(0);
				if (v[i] == 0) continue;
			}
#endif
			send_to_memballoon(v,inflate);
			balloon_data.pages_table[balloon_data.current_index] = v;
			balloon_data.current_index++;
			ut_printf("INFLATE Balloon SEND the 512 pages=2M to device: %d pages:%d  addr:%x\n",
					balloon_data.current_index,pages,v);

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

virtio_memballoon_jdriver::virtio_memballoon_jdriver(class jdevice *jdev){
	name = "virtio_mem_balloon";
	device = jdev;
	max_vqs = 3;
}

jdriver *virtio_memballoon_jdriver::attach_device(class jdevice *dev) {
	unsigned long addr;
	uint32_t features;
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
	ut_log(" driver Initialising VIRTIO  MEMORY balloon hostfeatures :%x:\n", features);
	addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	outl(addr, features);
	send_queues[0] = jnew_obj(memballoon_virtio_queue, device, 0, VQTYPE_SEND);
	send_queues[1] = jnew_obj(memballoon_virtio_queue, device, 1, VQTYPE_SEND);
	stat_queue = jnew_obj(memballoon_virtio_queue, device, 2, VQTYPE_SEND);

	g_virtio_balloon_driver = this;
	ar_registerInterrupt(32 + pci_dev->pci_header.interrupt_line, virtio_memb_interrupt, "virt_memballoon_irq", (void *) this);
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising.. VIRTIO PCI COMPLETED with driver ok :%x \n");

	inb(pci_ioaddr + VIRTIO_PCI_ISR);
	sc_createKernelThread(balloon_thread, 0,"VirtioBalloon",0);
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
	ut_printf("--------------------\n Balloon config [numpages:%d actual:%d ]\n Balloon Data [index:%d total pages:%d]  \n",balloon_config.num_pages,balloon_config.actual,balloon_data.current_index,balloon_data.total_pages);
	send_queues[0]->print_stats(0, 0);
	send_queues[1]->print_stats(0, 0);
	stat_queue->print_stats(0, 0);
}
#endif

