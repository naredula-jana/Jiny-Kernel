/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/virtio_net.cc
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
extern int g_conf_net_send_int_disable;


}
/******************************************* virtio net *********************************/
void virtio_net_jdriver::print_stats(unsigned char *arg1,unsigned char *arg2) {

}

struct virtio_feature_desc vtnet_feature_desc[] =
		{ { VIRTIO_NET_F_CSUM, "TxChecksum" }, { VIRTIO_NET_F_GUEST_CSUM,
				"RxChecksum" }, { VIRTIO_NET_F_MAC, "MacAddress" }, {
				VIRTIO_NET_F_GSO, "TxAllGSO" }, {
		VIRTIO_NET_F_GUEST_TSO4, "RxTSOv4" }, { VIRTIO_NET_F_GUEST_TSO6,
				"RxTSOv6" }, { VIRTIO_NET_F_GUEST_ECN, "RxECN" }, {
				VIRTIO_NET_F_GUEST_UFO, "RxUFO" }, { VIRTIO_NET_F_HOST_TSO4,
				"TxTSOv4" }, { VIRTIO_NET_F_HOST_TSO6, "TxTSOv6" }, {
		VIRTIO_NET_F_HOST_ECN, "TxTSOECN" }, { VIRTIO_NET_F_HOST_UFO, "TxUFO" },
				{ VIRTIO_NET_F_MRG_RXBUF, "MrgRxBuf" }, {
				VIRTIO_NET_F_STATUS, "Status" }, { VIRTIO_NET_F_CTRL_VQ,
						"ControlVq" }, { VIRTIO_NET_F_CTRL_RX, "RxMode" }, {
				VIRTIO_NET_F_CTRL_VLAN, "VLanFilter" }, {
						VIRTIO_NET_F_CTRL_RX_EXTRA, "RxModeExtra" }, {
						VIRTIO_NET_F_MQ, "Multi queue" }, { 0, NULL } };

extern "C" {
extern int virtio_BulkRemoveFromNetQueue(struct virtqueue *_vq,
		struct struct_mbuf *mbuf_list, int list_len);
extern int virtio_BulkAddToNetqueue(struct virtqueue *_vq,
		struct struct_mbuf *mbuf_list, int list_len, int is_send);
}
extern "C" {
extern int virtio_check_recv_pkt(void *);
}
int virtio_net_jdriver::check_for_pkts() {
	/* TODO :   checking only one queue: need to check other queues also */
	return queues[0].recv->check_recv_pkt();
}
int virtio_net_jdriver::burst_recv(int total_pkts) {
	unsigned char *addr;
	unsigned int list_len = MAX_BUF_LIST_SIZE;
	int i;
	int ret = 0;
	int recv_pkts = 0;

	if (total_pkts < list_len) {
		list_len = total_pkts;
	}

	recv_pkts = queues[0].recv->BulkRemoveFromQueue(&recv_mbuf_list[0],
			list_len);
	if (recv_pkts <= 0) {
		return 0;
	}

	//queues[0].recv->BulkAddToNetqueue( 0, MAX_BUF_LIST_SIZE*3,0);
	fill_empty_buffers(queues[0].recv);
	queues[0].recv->virtio_queuekick();

	for (i = 0; i < recv_pkts; i++) {
		netif_rx(recv_mbuf_list[i].buf, recv_mbuf_list[i].len);
		recv_mbuf_list[i].buf = 0;
	}
	return ret;
}

extern int g_net_bh_active;
static int virtio_net_recv_interrupt(void *private_data) {
	jdevice *dev;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi_enabled == 0) {
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	}
	driver->stat_recv_interrupts++;
	if (driver->recv_interrupt_disabled == 0) {
		driver->queues[0].recv->virtio_disable_cb(); /* disabling interrupts have Big negative impact on packet recived when smp enabled */
	}

	g_net_bh_active = 1;
	return 0;
}
static int virtio_net_send_interrupt(void *private_data) {
	jdevice *dev;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi_enabled == 0) {
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	}

	driver->stat_send_interrupts++;
	driver->queues[0].send->virtio_disable_cb();
	driver->send_waitq->wakeup();
	return 0;
}

int virtio_net_jdriver::probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id
					== VIRTIO_PCI_NET_DEVICE_ID)) {
		ut_log(" Matches inside the NETPROBE.... \n");
		return JSUCCESS;
	}
	return JFAIL;
}
static int net_devices = 0;

struct virtio_net_ctrl_mq {
	uint16_t virtqueue_pairs;
};
#define VIRTIO_NET_CTRL_MQ   4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0
static int virtnet_send_command(net_virtio_queue *vq, uint16_t mq_len) {
	struct scatterlist sg[2];
	int ret;
	int len;
	unsigned char *buf = 0;

	if (buf == 0) {
		buf = (unsigned char *) jalloc_page(MEM_NETBUF);
		len = 4096; /* page size */
	}
	if (buf == 0) {
		BRK
			;
	}

	ut_memset(buf, 0, sizeof(struct virtio_net_hdr));
	struct virtio_net_ctrl_hdr *ctrl;
	struct virtio_net_ctrl_mq *mq;

	ctrl = (struct virtio_net_ctrl_hdr *) buf;
	ctrl->var_class = VIRTIO_NET_CTRL_MQ;
	ctrl->cmd = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
	mq = (struct virtio_net_ctrl_mq *) (buf + 1024);
	mq->virtqueue_pairs = mq_len;

	sg[0].page_link = (unsigned long) buf;
	sg[0].length = sizeof(struct virtio_net_ctrl_hdr);
	sg[0].offset = 0;
	sg[1].page_link = (unsigned long) (mq);
	sg[1].length = sizeof(struct virtio_net_ctrl_mq);
	sg[1].offset = 0;
	sg[2].page_link = buf + 2048;
	sg[2].length = 1024;
	sg[2].offset = 0;
	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));

	//ret = virtio_add_buf_to_queue(vq, sg, 2, 1, (void *) sg[0].page_link, 0);/* send q */
	//virtio_queuekick(vq);

	ret = vq->virtio_add_buf_to_queue(sg, 2, 1, (void *) sg[0].page_link, 0);/* send q */
	vq->virtio_queuekick();
	//sc_sleep(1000);
	ut_log("Net ControlQ: SEND the maxq control command \n");
	/* TODO:  wait for the response and free the buf */
	return ret;
}
int virtio_net_jdriver::fill_empty_buffers(net_virtio_queue *queue) {
	queue->BulkAddToQueue(0, 128, 0);
}
int virtio_net_jdriver::net_attach_device() {
	unsigned long addr;
	uint32_t features = 0;
	uint32_t guest_features = 0;
	uint32_t mask_features = 0;
	int i, k;
	pci_dev_header_t *pci_hdr = &device->pci_device.pci_header;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;
	uint32_t msi_vector;
	unsigned char name[MAX_DEVICE_NAME];

	ut_snprintf(name, MAX_DEVICE_NAME, "net%d", net_devices);
	ut_strcpy(device->name, name);
	arch_spinlock_init(&virtionet_lock, device->name);
	net_devices++;

	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	VirtioNet: Initializing VIRTIO PCI NET status :%x : pcioaddr:%x\n",
			virtio_get_pcistatus(pci_ioaddr),pci_ioaddr);

	virtio_set_pcistatus(pci_ioaddr,
			virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);
	addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	guest_features = features;
	mask_features = (0x4000ff);

	guest_features = guest_features & mask_features;

	INIT_LOG(
			"	VirtioNet:  HOSTfeatures :%x:  capabilitie:%x guestfeatures:%x mask_features:%x\n",
			features, pci_hdr->capabilities_pointer, guest_features,
			mask_features);
	display_virtiofeatures(features, vtnet_feature_desc);

	addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	outl(addr, guest_features);

	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = pci_read_msi(&device->pci_device.pci_addr,
				&device->pci_device.pci_header, &device->pci_device.pci_bars[0],
				device->pci_device.pci_bar_count, &device->pci_device.msix_cfg);

		if (msi_vector > 0)
			pci_enable_msix(&device->pci_device.pci_addr,
					&device->pci_device.msix_cfg,
					device->pci_device.pci_header.capabilities_pointer);
	} else {
		msi_vector = 0;
	}

	if (msi_vector == 0) {
		device->pci_device.msi_enabled = 0;
		addr = pci_ioaddr + 20;
	} else {
		device->pci_device.msi_enabled = 1;
		addr = pci_ioaddr + 24;
	}
	for (i = 0; i < 6; i++) {
		this->mac[i] = inb(addr + i);
	}
	this->max_vqs = 1;
	if ((features >> VIRTIO_NET_F_MQ) & 0x1) {
		this->max_vqs = inw(addr + 6 + 2);
		if (this->max_vqs > MAX_VIRT_QUEUES) {
			this->max_vqs = MAX_VIRT_QUEUES;
		}
	}
	INIT_LOG("	VIRTIONET:  pioaddr:%x MAC address : %x :%x :%x :%x :%x :%x mis_vector:%x   : max_vqs:%x\n",
			addr, this->mac[0], this->mac[1], this->mac[2], this->mac[3],
			this->mac[4], this->mac[5], msi_vector, this->max_vqs);

#if 1
	if (msi_vector > 0) {
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 1);
		//	outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0xffff);
	}
#endif

	INIT_LOG("	VIRTIONET: initializing MAX VQ's:%d\n", max_vqs);
	for (i = 0; i < max_vqs; i++) {
		if (i == 1) {/* by default only 3 queues will be present , at this point already 2 queues are configured */
			control_q = jnew_obj(virtio_queue, device, 2 * i, VQTYPE_RECV)
			;
			if (control_q) { /* send mq command */
				virtnet_send_command(control_q, max_vqs);
				//break;
			}
		}
		queues[i].recv = jnew_obj(net_virtio_queue, device, 2 * i, VQTYPE_RECV)
		;
		//ut_log(" virtio_queue obj: %x\n",queues[i].recv );
		if (msi_vector > 0) {
			outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, (2 * i) + 0);
		}
		queues[i].send = jnew_obj(net_virtio_queue, device, (2 * i) + 1,
				VQTYPE_SEND)
		;
		if (msi_vector > 0) {
			outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, (2 * i) + 1);
		}
	}

	send_waitq = jnew_obj(wait_queue, "waitq_net", 0);

	if (g_conf_net_send_int_disable == 1) {
		for (i = 0; i < max_vqs; i++) {
			queues[i].send->virtio_disable_cb();
		}
	}

	if (msi_vector > 0) {
		for (i = 0; i < max_vqs; i++) {
			char irq_name[MAX_DEVICE_NAME];

			ut_snprintf(irq_name, MAX_DEVICE_NAME, "%s_recv_msi", this->name);
			ar_registerInterrupt(msi_vector + 2 * i, virtio_net_recv_interrupt,
					irq_name, (void *) this);
#if 0
			// TODO : enabling sending side interrupts causes freeze in the buffer consumption on the sending side,
			// till  all the buffers are full for the first time. this happens especially on the smp

			if (i!=0) {
				ut_snprintf(irq_name,MAX_DEVICE_NAME,"%s_send_msi",jdev->name);
				ar_registerInterrupt(msi_vector + i, virtio_net_send_interrupt, irq_name, (void *) this);
			}
#endif
		}
	}

	for (k = 0; k < max_vqs; k++) {
		if (queues[k].recv == 0 || queues[k].send == 0) {
			break;
		}
		fill_empty_buffers(queues[k].recv);
	}
	inb(pci_ioaddr + VIRTIO_PCI_ISR);
	for (k = 0; k < max_vqs; k++) {
		if (queues[k].recv == 0 || queues[k].send == 0) {
			break;
		}
		queues[k].recv->virtio_queuekick();
	}
	pending_kick_onsend = 0;
	recv_interrupt_disabled = 0;

	send_mbuf_start = send_mbuf_len = 0;
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	INIT_LOG("		VirtioNet:  Initialization Completed status:%x\n",
			virtio_get_pcistatus(pci_ioaddr));

	return 1;
}
extern int register_netdevice(jdevice *device);
jdriver *virtio_net_jdriver::attach_device(class jdevice *jdev) {

	COPY_OBJ(virtio_net_jdriver, this, new_obj, jdev);
	((virtio_net_jdriver *) new_obj)->net_attach_device();
	jdev->driver = new_obj;
	register_netdevice(jdev);
	return (jdriver *) new_obj;
}

int virtio_net_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}
int virtio_net_jdriver::read(unsigned char *buf, int len, int rd_flags,
		int opt_flags) {
	return 0;
}
extern "C" {
int Bulk_free_pages(struct struct_mbuf *mbufs, int list_len);
}
int virtio_net_jdriver::burst_send() {
	int qno, i, ret = 0;
	int qret;
	unsigned long flags;
	int pkts;

	if (send_mbuf_len == 0) {
		return ret;
	}

	/* remove the used buffers from the previous send cycle */
	for (qno = 0; qno < max_vqs; qno++) {
#if 1
		pkts = queues[qno].send->BulkRemoveFromQueue(&temp_mbuf_list[0],
				MAX_BUF_LIST_SIZE);
		if (pkts > 0) {
			Bulk_free_pages(temp_mbuf_list, pkts);
		}
#else
		pkts = queues[qno].send->BulkRemoveFromQueue(0,MAX_BUF_LIST_SIZE * 4);
#endif
	}

//	spin_lock_irqsave(&virtionet_lock, flags);
	ret = 0;
	for (qno = 0; qno < max_vqs; qno++) { /* try to send from the same queue , if it full then try on the subsequent one, in this way kicks will be less */
		qret = queues[qno].send->BulkAddToQueue(
				&send_mbuf_list[send_mbuf_start + ret], send_mbuf_len - ret, 1);
		if (qret == 0) {
			continue;
		}
		if (queues[qno].send->notify_needed) {
			queues[qno].pending_send_kick = 1;
			pending_kick_onsend = 1;
		}
		ret = ret + qret;
		if (ret == send_mbuf_len) {
			break;
		}
	}

	if (ret < 0) {
		ret = 0;
	}
	if (ret != 0) {
		if (ret == send_mbuf_len) { /* empty the mbuf_list */
			send_mbuf_start = 0;
			send_mbuf_len = 0;
		} else {
			send_mbuf_start = send_mbuf_start + ret;
			send_mbuf_len = send_mbuf_len - ret;
		}
		stat_sends++;
	}
//	spin_unlock_irqrestore(&virtionet_lock, flags);
	return ret; /* Here Success indicates the buffer is freed or consumed */
}
int virtio_net_jdriver::write(unsigned char *data, int len, int wr_flags) {
	BUG()
	;
}

int virtio_net_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	unsigned char *arg_mac = (unsigned char *) arg2;
	int i;

	if (arg1 == NETDEV_IOCTL_GETMAC) {
		if (arg_mac == 0)
			return JFAIL;
		else {
			ut_memcpy(arg_mac, mac, 6);
			return JSUCCESS;
		}
	} else if (arg1 == NETDEV_IOCTL_FLUSH_SENDBUF) {
		if (pending_kick_onsend != 0) {
			unsigned long flags;
			int qno;
			spin_lock_irqsave(&virtionet_lock, flags);
			for (qno = 0; qno < max_vqs && qno < MAX_VIRT_QUEUES; qno++) {
				if (queues[qno].pending_send_kick == 1) {
					queues[qno].send->virtio_queuekick();
					queues[qno].pending_send_kick = 0;
				}
			}
			pending_kick_onsend = 0;
			spin_unlock_irqrestore(&virtionet_lock, flags);

			return JSUCCESS;
		} else {
			return JFAIL;
		}
	} else if (arg1 == NETDEV_IOCTL_DISABLE_RECV_INTERRUPTS) {
		queues[0].recv->virtio_disable_cb();
		recv_interrupt_disabled = 1;

	} else if (arg1 == NETDEV_IOCTL_ENABLE_RECV_INTERRUPTS) {
		queues[0].recv->virtio_enable_cb();
		recv_interrupt_disabled = 0;
	} else if (arg1 == NETDEV_IOCTL_PRINT_STAT) {
		ut_printf("Total queues: %d\n", max_vqs);
		for (i = 0; i < max_vqs && i < MAX_VIRT_QUEUES; i++) {
			if (queues[i].recv == 0 || queues[i].send == 0) {
				break;
			}
			ut_printf("VQ-%d :\n", i);
			queues[i].recv->print_stats(arg2, 0);
			queues[i].send->print_stats(arg2, 0);
		}
		if (control_q) {
			ut_printf("Control-Q :\n");
			control_q->print_stats(0, 0);
		} else {
			ut_printf("No Control-Q present\n");
		}

	}
}
