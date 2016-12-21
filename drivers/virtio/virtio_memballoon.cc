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

#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "virtio_pci.h"
#include "mach_dep.h"
int g_conf_balloon_enable = 1;
}

struct virtio_balloon_config {  /* input from qemu to guest using interrupt */
	 /* Number of pages host wants Guest need to give up.
	  *  example: balloon 10 for 256M vm ,means guest need to reduces to 10M, so need to giveup 246M or 246*256 4k-pages, then num_pages=62976
	  *        balloon 100 for 256M vm, means guest need to reduce and giveup to 256-100=156M or 156*256=39936 */
	uint32_t num_pages;

	/* Number of pages we've actually got in balloon. */
	uint32_t actual;
};

#define MAX_TABLE_SIZE 2000  /* for every entry it can store 2M pages = 512*4k, covers 2*2000 = 4G */
static struct balloon_data_struct{
	/* each entry is a seperate page holding 4k/8=512 pages= 2M */
	unsigned long pages_table[MAX_TABLE_SIZE];
	int current_index;
	int total_pages;
	struct virtio_balloon_config config;
	int stat_inflates;
	int stat_deflates;
}balloon_data;

virtio_memballoon_jdriver *g_virtio_balloon_driver;
#define VIRTIO_BALLOON_S_SWAP_IN  0   /* Amount of memory swapped in */
#define VIRTIO_BALLOON_S_SWAP_OUT 1   /* Amount of memory swapped out */
#define VIRTIO_BALLOON_S_MAJFLT   2   /* Number of major faults */
#define VIRTIO_BALLOON_S_MINFLT   3   /* Number of minor faults */
#define VIRTIO_BALLOON_S_MEMFREE  4   /* Total amount of free memory */
#define VIRTIO_BALLOON_S_MEMTOT   5   /* Total amount of memory */
#define VIRTIO_BALLOON_S_AVAIL    6   /* Available memory as in /proc */
#define VIRTIO_BALLOON_S_NR       7

struct virtio_balloon_stat {
	uint16_t tag;
	uint64_t val;
} __attribute__((packed));
static struct virtio_balloon_stat vb_stats[6];

#define BALLOON_INFLATE 1
#define BALLOON_DEFLATE 2

#define MEMBALLOON_2M_PAGE_SIZE 1
/*
 * The following are performance benifits of 2M page:
 * 1) adding and removing the pages from/to host host is fast, because hypervisor uses one madvise call instead of 512 system calls in qemu.
 * 2) Huge pages in host page table will be fragmented in case of 4k pages, where as in 2M it will be intact. with 4k pages ballooning, THP(Transparent Huges Pages ) subsystem will be doing lot of fragmentation and defragmentation in the host. These cycles will be saved with 2M page.
 * 3) With 2M pages, Nested paging work faster, this makes app inside the vm faster.
 *
 * Issues with 2M pages:
 * 1) for a small vm(with small ammount of physical memory) while inflating the balloon, getting the free 2M page may be some time difficult.
 * Solution: Incase 2M free page is not found, fallback to 4k page.
 * KVM Hypervisor Changes:
 *  1) the 512 pages transported to hypervisor need to check if it all belong to same 2M page, if it is then use one madvise call
 *  instead of 512 calls, this is same for inflate and deflate.
 *  The above change is optional, if the above change is not present then only second benifit will be available.
 *  This is a best effort solution. and also does need any changes to virtio protocol between guest and kvm.
 *
 *
 */

static inline void update_stat(virtio_balloon_stat *stats, int idx,
		uint16_t tag, uint64_t val){
	stats[idx].tag = tag;
	stats[idx].val = val;
}

#define pages_to_bytes(x) ((uint64_t)(x) << PAGE_SHIFT)

extern int g_nr_free_pages;
extern unsigned long g_stat_mem_size,g_phy_mem_size;

static void virtio_memb_interrupt(void *private_data) {
	virtio_memballoon_jdriver *driver = (virtio_memballoon_jdriver *) private_data;
	jdevice *dev = (jdevice *) driver->device;
	/* reset the irq by resetting the status  */
	unsigned char isr;
	isr = inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	unsigned long config_addr;

	config_addr = dev->pci_device.pci_ioaddr  + 20;
	balloon_data.config.num_pages= inl(config_addr);
	balloon_data.config.actual= inl(config_addr+4);

	//ut_log("VIRTIO BALLOON interrupt :config value:  numpages:%x(%d)  actual:%x(%d) \n",balloon_config.num_pages,balloon_config.num_pages,balloon_config.actual,balloon_config.actual);
}


static void update_balloon_size(){
	virtio_memballoon_jdriver *driver = g_virtio_balloon_driver;
	jdevice *dev = (jdevice *) driver->device;
	unsigned long config_addr;

	uint32_t total = balloon_data.total_pages ;
	//ut_log("BALLOON  ACTUAL size :%x(%d) \n",total,total);

	config_addr = dev->pci_device.pci_ioaddr  + 20;

	outl(config_addr+4,total);
}
static void update_balloon_stats(){
	int idx = 0;
	struct scatterlist sg[4];
	int len;

	g_virtio_balloon_driver->stat_queue->virtio_removeFromQueue(&len);
	if (g_virtio_balloon_driver->stat_queue->stat_add_success > (g_virtio_balloon_driver->stat_queue->stat_rem_success)+2){
		//ut_log("MEM BALLOON FAIL to update: adds:%d removes:%d \n",g_virtio_balloon_driver->stat_queue->stat_add_success,g_virtio_balloon_driver->stat_queue->stat_rem_success);
		return;
	}
	uint64_t total_available_mem = g_stat_mem_size-(balloon_data.total_pages * PAGE_SIZE) ;
	uint64_t total_phy_mem = g_phy_mem_size-(balloon_data.total_pages * PAGE_SIZE) ; /* only for logging */

	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MEMFREE, g_nr_free_pages*PAGE_SIZE);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MEMTOT,total_available_mem );
	//update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_AVAIL,total_available_mem );
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_SWAP_OUT, 0);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MINFLT, 0);
	update_stat(vb_stats, idx++, VIRTIO_BALLOON_S_MAJFLT, 0);

	//ut_log("MEM Balloon update status, free :%d(%x) Avail:%d(%x), Total:%d\n",g_nr_free_pages*PAGE_SIZE,g_nr_free_pages*PAGE_SIZE,total_available_mem,total_available_mem,total_phy_mem);

	sg[0].page_link = (unsigned long) vb_stats;
	sg[0].length = sizeof(vb_stats);
	sg[0].offset = 0;

	g_virtio_balloon_driver->stat_queue->virtio_enable_cb();
	g_virtio_balloon_driver->stat_queue->virtio_add_buf_to_queue(sg, 1, 0, (void *)sg[0].page_link, 0);
	g_virtio_balloon_driver->stat_queue->virtio_queuekick();

	g_virtio_balloon_driver->stat_queue->virtio_removeFromQueue(&len);
}
static int balloon_pages(int pages, int type);
void balloon_thread(){
	int ret;
	sc_sleep(1000); /* sleep for 10 sec */
	while (1){
		sc_sleep(500); /* sleep for 5 sec */

		if (g_conf_balloon_enable == 1){
			int diff = balloon_data.config.num_pages - balloon_data.config.actual;
			if (diff > 0){ /* inflate */
				ret = balloon_pages(diff, 1);
				ut_log(" Virtio Balloon: Inflated pages:%d \n",ret);
			}else if (diff< 0) { /* deflate */
				ret = balloon_pages(-diff, 0);
				ut_log(" Virtio Balloon: Deflated pages:%d \n",ret);
			}
		}
		update_balloon_stats();
	}
}

#if 1
static uint32_t buf[512];
static int send_to_memballoon(unsigned long *v, int type) {
	struct scatterlist sg[4];
	int i,in,out;
	int queue_id=0;
	int len;
	int ret=0;

	for (i = 0; i < 512; i++) {
#ifdef  MEMBALLOON_2M_PAGE_SIZE
		unsigned long addr =(unsigned long)v;
		buf[i] = __pa(addr + (i*PAGE_SIZE)) >> 12;
#else
		buf[i] = __pa(v[i]) >> 12;
#endif
		ret++;
	}
	sg[0].page_link = (unsigned long) buf;
	sg[0].length = 512*4;
	sg[0].offset = 0;

	if (type == BALLOON_INFLATE){
		queue_id=0;
	}else{
		queue_id=1;
	}
	out=1;
	in=0;

	g_virtio_balloon_driver->send_queues[queue_id]->virtio_enable_cb();
	g_virtio_balloon_driver->send_queues[queue_id]->virtio_add_buf_to_queue(sg, out, in, (void *)sg[0].page_link, 0);
	g_virtio_balloon_driver->send_queues[queue_id]->virtio_queuekick();

	g_virtio_balloon_driver->send_queues[queue_id]->virtio_removeFromQueue(&len);

	return ret;
}
#endif
static spinlock_t balloon_lock = SPIN_LOCK_UNLOCKED("mem_balloon");
extern "C" {
int Jcmd_balloon(char *arg1, char *arg2) {
	int inflate = BALLOON_INFLATE;
	int pages = 0;

	if (arg1 == 0)
		return 0;

	if (arg1[0] == '-') {
		arg1[0] = '0';
		inflate = BALLOON_DEFLATE;
	}
	pages = ut_atoi(arg1,FORMAT_DECIMAL);
	if (pages == 0){
		return 0;
	}

	inflate = ut_atoi(arg2,FORMAT_DECIMAL);
	ut_printf(" Ballooned :%d  pages:%d \n",inflate,pages);
	return balloon_pages(pages, inflate);
}

}
static int balloon_pages(int pages, int type){
	unsigned long flags;
	unsigned long *v;
	int i;
	int total_ballooned =0;

	if (pages <= 512){
		return 0;
	}

	spin_lock_irqsave(&balloon_lock, flags);
	if (type == BALLOON_INFLATE) {
		while (pages > 0) {
			if (balloon_data.current_index >= MAX_TABLE_SIZE) {
				break;
			}
#ifdef  MEMBALLOON_2M_PAGE_SIZE
			v = mm_getFreePages(0, 9); /* allocate 2M page */
			if (v != 0){
				pages = pages - 512;
			}else{
				break;
			}
			/* TODO:  Yet to implement , if we cannot get the big pages, fall back to 4k  pages : */
#if 0
			unsigned char *testp = (unsigned char *)v;
			*testp =0 ; /* touch the memory to bring the page */
#endif

#else
			v = alloc_page(0);
			if (v == 0) {
				ut_printf("Inflate Ballon Fail to allocate the Page \n");
				break;
			}
			pages--;
			for (i = 0; i < 512; i++) {
				v[i] = alloc_page(0);
				if (v[i] == 0) continue;
			}
#endif

			balloon_data.pages_table[balloon_data.current_index] = v;
			balloon_data.current_index++;
			total_ballooned += send_to_memballoon(v,type);
			ut_printf("INFLATE Balloon SEND the 512 pages=2M to device: %d pages:%d  addr:%x\n",
					balloon_data.current_index,pages,v);
		}
		balloon_data.total_pages+=total_ballooned;
		if (total_ballooned > 0){
			balloon_data.stat_inflates++;
		}
	} else {
		while (pages > 0) {
			if (balloon_data.total_pages==0 || balloon_data.current_index <= 0) {
				break;
			}
			balloon_data.current_index--;
			v = balloon_data.pages_table[balloon_data.current_index];
			if (v == 0) {
				break;
			}
			balloon_data.pages_table[balloon_data.current_index] = 0;
			total_ballooned += send_to_memballoon(v,type);

#ifdef  MEMBALLOON_2M_PAGE_SIZE
			mm_putFreePages(v, 9); /* free 2M page */
#else
			for (i = 0; i < 512; i++) {
				if (v[i] == 0) continue;
				mm_putFreePages(v[i], 0);
			}
			mm_putFreePages(v, 0);
#endif
			ut_printf("Deflate Ballon send the 512 pages=2M to device: %d pages:%d \n",
					balloon_data.current_index,pages);
			pages--;
		}
		balloon_data.total_pages -= total_ballooned;
		if (total_ballooned > 0){
			balloon_data.stat_deflates++;
		}
	}
	spin_unlock_irqrestore(&balloon_lock, flags);
	if (total_ballooned != 0){
		update_balloon_size();
	}
	return total_ballooned;
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
	ut_printf("--------------------\n Balloon config [numpages:%d actual:%d ]\n Balloon Data   [Index:%d Total pages:%d]  \n",
			balloon_data.config.num_pages,balloon_data.config.actual,balloon_data.current_index,balloon_data.total_pages);
	ut_printf(" Inflates :%d Deflates:%d \n",balloon_data.stat_inflates,balloon_data.stat_deflates);
	send_queues[0]->print_stats(0, 0);
	send_queues[1]->print_stats(0, 0);
	stat_queue->print_stats(0, 0);
}


