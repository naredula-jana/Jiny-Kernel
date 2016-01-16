
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/driver_virtio_pci.cc
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
extern int p9_initFs(void *p);
extern void print_vq(struct virtqueue *_vq);
extern int g_conf_net_send_int_disable;

atomic_t  g_conf_stat_pio = ATOMIC_INIT(0);
}
#include "jdevice.h"


/************************   utilities used by vitio drivr ***************************/

void display_virtiofeatures(unsigned long feature, struct virtio_feature_desc *desc) {
	int i, j, bit;
	for (i = 0; i < 32; i++) {
		bit = (feature >> i) & (0x1);
		if (bit) {
			for (j = 0; (desc[j].name != NULL); j++) {
				if (desc[j].feature_bit == i){
					ut_log("[%d] %s,",i,desc[j].name);
				}
			}
		}
	}
	ut_log("\n");
}
unsigned char virtio_get_pcistatus(unsigned long pci_ioaddr) {
	uint16_t addr = pci_ioaddr + VIRTIO_PCI_STATUS;
	return inb(addr);
}
void virtio_set_pcistatus(unsigned long pci_ioaddr, unsigned char status) {
	uint16_t addr = pci_ioaddr + VIRTIO_PCI_STATUS;
	outb(addr, status);
}

static void callback(struct virtqueue *vq) {
	DEBUG("  VIRTIO CALLBACK in VIRT queue :%x\n", vq);
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

/***************************************************************************************************/
extern wait_queue *p9_waitq;
static int virtio_9p_interrupt(void *private_data) { // TODO: handling similar  type of interrupt generating while serving P9 interrupt.
	virtio_p9_jdriver *driver = (virtio_p9_jdriver *) private_data;

	if (driver->device->pci_device.msi_enabled == 0)
		inb(driver->device->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);

	p9_waitq->wakeup(); /* wake all the waiting processes */

	return 0;
}
void virtio_p9_jdriver::print_stats(unsigned char *arg1,unsigned char *arg2) {
	ut_printf("		p9 disk device: %s: Send(P,K,I): %d,%i,%d  Recv(P,K,I):%d,%i,%d allocs:%d free:%d ERR(send no space):%d \n", this->name, this->stat_sends,
			this->stat_send_kicks, this->stat_send_interrupts, this->stat_recvs, this->stat_recv_kicks, this->stat_recv_interrupts, this->stat_allocs,
			this->stat_frees, this->stat_err_nospace);
}
int virtio_p9_jdriver::probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id == VIRTIO_PCI_9P_DEVICE_ID)) {
		ut_log(" Matches the P9Probe \n");
		return JSUCCESS;
	}
	return JFAIL;
}
int virtio_p9_jdriver::p9_attach_device(class jdevice *jdev) {
	auto pci_ioaddr = jdev->pci_device.pci_ioaddr;
	unsigned long features;

	this->device = jdev;
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	Virtio P9: Initializing VIRTIO PCI status :%x : \n", virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	auto addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	Virtio P9: Initializing VIRTIO PCI 9P hostfeatures :%x: status:%x\n", features, virtio_get_pcistatus(pci_ioaddr));

	//this->vq[0] = this->virtio_create_queue(0, VQTYPE_SEND);
	this->vq[0] = jnew_obj(virtio_queue,device,  0,VQTYPE_SEND);
	if (jdev->pci_device.msix_cfg.isr_vector > 0) {
#if 0
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0);
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0xffff);
		ar_registerInterrupt(msi_vector, virtio_9p_interrupt, "virtio_p9_msi");
#endif
	}
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	ut_log("	Virtio P9:  VIRTIO PCI COMPLETED with driver ok :%x \n", virtio_get_pcistatus(pci_ioaddr));
	inb(pci_ioaddr + VIRTIO_PCI_ISR);

	p9_waitq= jnew_obj(wait_queue, "waitq_p9", 0);
	ar_registerInterrupt(32 + jdev->pci_device.pci_header.interrupt_line, virtio_9p_interrupt, "virt_p9_irq", (void *) this);

	p9_initFs(this);
	return 1;
}
jdriver *virtio_p9_jdriver::attach_device(class jdevice *jdev) {
	COPY_OBJ(virtio_p9_jdriver, this, new_obj, jdev);
	((virtio_p9_jdriver *) new_obj)->p9_attach_device(jdev);

	return (jdriver *) new_obj;
}
int virtio_p9_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}
int virtio_p9_jdriver::read(unsigned char *buf, int len, int flags, int opt_flags) {
	return 0;
}
int virtio_p9_jdriver::write(unsigned char *buf, int len, int flags) {
	return 0;
}
int virtio_p9_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	return 0;
}
/*************************************************************************************************/
static virtio_p9_jdriver *p9_jdriver;
static virtio_net_jdriver *net_jdriver;
static virtio_disk_jdriver *disk_jdriver;


extern "C"{

void init_virtio_drivers() {

	/* init p9 */
	p9_jdriver = jnew_obj(virtio_p9_jdriver);
	p9_jdriver->name = (unsigned char *) "p9_driver";
	register_jdriver(p9_jdriver);

	/* init net */
	net_jdriver = jnew_obj(virtio_net_jdriver);
	net_jdriver->name = (unsigned char *) "net_virtio_driver";
	register_jdriver(net_jdriver);

	/* init disk */
	disk_jdriver = jnew_obj(virtio_disk_jdriver, 0);
	disk_jdriver->name = (unsigned char *) "disk_virtio_driver";
	register_jdriver(disk_jdriver);
}

virtio_queue *virtio_jdriver_getvq(void *driver, int index) {
	virtio_p9_jdriver *jdriver = (virtio_p9_jdriver *) driver;

	return jdriver->vq[index];
}
}
