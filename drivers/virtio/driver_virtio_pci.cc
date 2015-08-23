
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

#include "virtio.h"
#include "virtio_ring.h"
#include "virtio_pci.h"
#include "net/virtio_net.h"
#include "mach_dep.h"
extern int p9_initFs(void *p);
extern void print_vq(struct virtqueue *_vq);
extern int init_tarfs(jdriver *driver_arg);
extern int g_conf_net_send_int_disable;

atomic_t  g_conf_stat_pio = ATOMIC_INIT(0);
}
#include "jdevice.h"
extern int register_netdevice(jdevice *device);

/************************   utilities used by vitio drivr ***************************/
struct virtio_feature_desc {
	int feature_bit;
	const char *name;
};
static void display_virtiofeatures(unsigned long feature, struct virtio_feature_desc *desc) {
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
static unsigned char virtio_get_pcistatus(unsigned long pci_ioaddr) {
	uint16_t addr = pci_ioaddr + VIRTIO_PCI_STATUS;
	return inb(addr);
}
static void virtio_set_pcistatus(unsigned long pci_ioaddr, unsigned char status) {
	uint16_t addr = pci_ioaddr + VIRTIO_PCI_STATUS;
	outb(addr, status);
}
/* the notify function used when creating a virt queue */
static void notify(struct virtqueue *vq) {
	atomic_inc(&g_conf_stat_pio);
	outw(vq->pci_ioaddr + VIRTIO_PCI_QUEUE_NOTIFY, vq->queue_number);
}
static void callback(struct virtqueue *vq) {
	DEBUG("  VIRTIO CALLBACK in VIRT queue :%x\n", vq);
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

/*******************************  virtio_jdriver ********************************/
void virtio_jdriver::print_stats(unsigned char *arg1,unsigned char *arg2) {
	ut_printf("		Network device: %s: Send(P,K,I): %d,%i,%d  Recv(P,K,I):%d,%i,%d allocs:%d free:%d ERR(send no space):%d TotalKick:%i\n", this->name, this->stat_sends,
			this->stat_send_kicks, this->stat_send_interrupts, this->stat_recvs, this->stat_recv_kicks, this->stat_recv_interrupts, this->stat_allocs,
			this->stat_frees, this->stat_err_nospace,this->stat_kicks);
}

void virtio_jdriver::queue_kick(struct virtqueue *vq){
	if (vq ==0 ) return;
	if (virtio_queuekick(vq) == 1){
		atomic_inc(&stat_kicks);
		if (vq->qType == VQTYPE_RECV){
			atomic_inc(&stat_recv_kicks);
		}else{
			atomic_inc(&stat_send_kicks);
		}
	}
}
struct virtqueue *virtio_jdriver::virtio_create_queue(uint16_t index, int qType) {
	int size;
	uint16_t num;
	unsigned long ring_addr;
	struct virtqueue *virt_q;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;

	outw(pci_ioaddr + VIRTIO_PCI_QUEUE_SEL, index);

	num = inw(pci_ioaddr + VIRTIO_PCI_QUEUE_NUM);
	max_qbuffers = num;

	//num = 1024;
	INIT_LOG("		New virtio create queue NUM-%d : num %x(%d)  :%x\n", index, num, num, vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	if (num == 0) {
		return 0;
	}
	size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));

	//vring_size(num);
	ring_addr = mm_getFreePages(MEM_CLEAR, get_order(size));

	/* activate the queue */
	outl(pci_ioaddr + VIRTIO_PCI_QUEUE_PFN, __pa(ring_addr) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	/* create the vring */
	INIT_LOG("		virtioqueue Creating queue:%x(pa:%x) size-order:%x  size:%x\n", ring_addr, __pa(ring_addr),get_order(size),size);
	virt_q = (struct virtqueue * )vring_new_virtqueue(num, VIRTIO_PCI_VRING_ALIGN, device->pci_device.pci_ioaddr, (void *) ring_addr, &notify,
			&callback, "VIRTQUEUE", index);
	virtqueue_enable_cb_delayed(virt_q);
	virt_q->qType = qType;


	return virt_q;
}
/******************************************* virtio net *********************************/

struct virtio_feature_desc vtnet_feature_desc[] = { { VIRTIO_NET_F_CSUM, "TxChecksum" },
		{ VIRTIO_NET_F_GUEST_CSUM, "RxChecksum" }, { VIRTIO_NET_F_MAC, "MacAddress" }, { VIRTIO_NET_F_GSO, "TxAllGSO" }, {
				VIRTIO_NET_F_GUEST_TSO4, "RxTSOv4" }, { VIRTIO_NET_F_GUEST_TSO6, "RxTSOv6" }, { VIRTIO_NET_F_GUEST_ECN, "RxECN" },
		{ VIRTIO_NET_F_GUEST_UFO, "RxUFO" }, { VIRTIO_NET_F_HOST_TSO4, "TxTSOv4" }, { VIRTIO_NET_F_HOST_TSO6, "TxTSOv6" }, {
				VIRTIO_NET_F_HOST_ECN, "TxTSOECN" }, { VIRTIO_NET_F_HOST_UFO, "TxUFO" }, { VIRTIO_NET_F_MRG_RXBUF, "MrgRxBuf" }, {
				VIRTIO_NET_F_STATUS, "Status" }, { VIRTIO_NET_F_CTRL_VQ, "ControlVq" }, { VIRTIO_NET_F_CTRL_RX, "RxMode" }, {
				VIRTIO_NET_F_CTRL_VLAN, "VLanFilter" }, { VIRTIO_NET_F_CTRL_RX_EXTRA, "RxModeExtra" }, { VIRTIO_NET_F_MQ, "Multi queue" }, { 0, NULL } };

extern "C"{
extern int virtio_BulkRemoveFromQueue(struct virtqueue *_vq, struct struct_mbuf *mbuf_list, int list_len);
extern int virtio_add_Bulk_to_queue(struct virtqueue *_vq,  struct struct_mbuf *mbuf_list, int list_len);
}
int virtio_net_jdriver::dequeue_burst(int total_pkts){
	unsigned char *addr;
	unsigned int list_len = 32;
	int i;
	int ret = 0;
	int recv_pkts=0;

	if (total_pkts < list_len){
		list_len = total_pkts;
	}

	recv_pkts=virtio_BulkRemoveFromQueue(queues[0].recv, &recv_mbuf_list[0], list_len);

	if (recv_pkts <= 0){
		return 0;
	}

	for (i = 0; i < recv_pkts; i++) {
		addBufToNetQueue(0,VQTYPE_RECV, 0, 4096);
	}
	if (recv_pkts > 0) {
		queue_kick(queues[0].recv);
	}

	for (i = 0; i < recv_pkts; i++) {
		net_sched.netif_rx(recv_mbuf_list[i].buf, recv_mbuf_list[i].len);
	}

	return ret;
}

int virtio_net_jdriver::virtio_net_poll_device( int total_pkts) {
	unsigned char *addr;
	unsigned int len = 0;
	int i;
	int ret = 0;

	for (i = 0; i < total_pkts; i++) {
		addr = virtio_removeFromQueue(queues[0].recv,(int *)&len);
		if (addr != 0) {
			stat_recvs++;

			recv_mbuf_list[ret].buf = addr;
			recv_mbuf_list[ret].len = len;
			addBufToNetQueue(0,VQTYPE_RECV, 0, 4096);
			ret = ret + 1;

		} else {
			break;
		}
	}
	if (ret > 0) {
		queue_kick(queues[0].recv);
	}

	for (i = 0; i < ret; i++) {
		net_sched.netif_rx(recv_mbuf_list[i].buf, recv_mbuf_list[i].len);
	}

	return ret;
}


static int netdriver_xmit(unsigned char* data, unsigned int len, void *private_data) {
	virtio_net_jdriver *net_driver = (virtio_net_jdriver *) private_data;
	return net_driver->write(data, len, 0);
}

extern int g_net_bh_active;
static int virtio_net_recv_interrupt(void *private_data) {
	jdevice *dev;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi_enabled == 0){
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	}

	driver->stat_recv_interrupts++;

	if (driver->recv_interrupt_disabled == 0){
	  virtio_disable_cb(driver->queues[0].recv); /* disabling interrupts have Big negative impact on packet recived when smp enabled */
	}

	g_net_bh_active = 1;
	return 0;

}
static int virtio_net_send_interrupt(void *private_data) {
	jdevice *dev;
	virtio_net_jdriver *driver = (virtio_net_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi_enabled == 0){
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	}

	driver->stat_send_interrupts++;
	virtio_disable_cb(driver->queues[0].send);
	driver->send_waitq->wakeup();
	return 0;

}

int virtio_net_jdriver::probe_device(class jdevice *jdev) {

	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& (jdev->pci_device.pci_header.device_id == VIRTIO_PCI_NET_DEVICE_ID)) {
		ut_log(" Matches inside the NETPROBE.... \n");
		return JSUCCESS;
	}
	return JFAIL;
}
static int net_devices=0;

struct virtio_net_ctrl_mq {
         uint16_t virtqueue_pairs;
 };
#define VIRTIO_NET_CTRL_MQ   4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0
static int virtnet_send_command(struct virtqueue *vq,
                                uint16_t mq_len )
{
	struct scatterlist sg[2];
	int ret;
	int len;

	unsigned char *buf = 0;

	if (buf == 0) {
		buf = (unsigned char *) jalloc_page(MEM_NETBUF);
		len = 4096; /* page size */
	}
	if (buf == 0) {
		BRK;
	}

	ut_memset(buf, 0, sizeof(struct virtio_net_hdr));
	struct virtio_net_ctrl_hdr *ctrl;
	struct virtio_net_ctrl_mq *mq;

	ctrl = (struct virtio_net_ctrl_hdr *)buf;
	ctrl->var_class = VIRTIO_NET_CTRL_MQ;
	ctrl->cmd = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
	mq = (struct virtio_net_ctrl_mq *)(buf + 1024);
	mq->virtqueue_pairs = mq_len;

	sg[0].page_link = (unsigned long) buf;
	sg[0].length = sizeof(struct virtio_net_ctrl_hdr);
	sg[0].offset = 0;
	sg[1].page_link = (unsigned long) (mq);
	sg[1].length = sizeof(struct virtio_net_ctrl_mq);
	sg[1].offset = 0;
	sg[2].page_link = buf+2048;
	sg[2].length = 1024;
	sg[2].offset = 0;
	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));

	ret = virtio_add_buf_to_queue(vq, sg, 2, 1, (void *) sg[0].page_link, 0);/* send q */
	virtio_queuekick(vq);
	//sc_sleep(1000);
	ut_log(" New111 SEND the maxq control command \n");
/* TODO:  wait for the response and free the buf */
	return ret;
}

int virtio_net_jdriver::net_attach_device() {
	unsigned long addr;
	uint32_t features=0;
	uint32_t guest_features=0;
	uint32_t mask_features=0;
	int i,k;
	pci_dev_header_t *pci_hdr = &device->pci_device.pci_header;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;
	uint32_t msi_vector;
	unsigned char name[MAX_DEVICE_NAME];

	ut_snprintf(name,MAX_DEVICE_NAME,"net%d",net_devices);
	ut_strcpy(device->name,name);
	arch_spinlock_init(&virtionet_lock, device->name );
	net_devices++;

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	VirtioNet: Initializing VIRTIO PCI NET status :%x : \n", virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);

	addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	guest_features = features;
	mask_features = (0x4000ff);

	guest_features = guest_features & mask_features;

	INIT_LOG("	VirtioNet:  HOSTfeatures :%x:  capabilitie:%x guestfeatures:%x mask_features:%x\n", features, pci_hdr->capabilities_pointer,guest_features,mask_features);
	display_virtiofeatures(features, vtnet_feature_desc);

	 addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	 outl(addr,guest_features);

	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = pci_read_msi(&device->pci_device.pci_addr, &device->pci_device.pci_header, &device->pci_device.pci_bars[0],
				device->pci_device.pci_bar_count, &device->pci_device.msix_cfg);

		if (msi_vector > 0)
			pci_enable_msix(&device->pci_device.pci_addr, &device->pci_device.msix_cfg,
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
	for (i = 0; i < 6; i++){
		this->mac[i] = inb(addr + i);
	}
	this->max_vqs = 1;
	if ((features >> VIRTIO_NET_F_MQ) & 0x1){
		this->max_vqs = inw(addr + 6+2);
		if (this->max_vqs > MAX_VIRT_QUEUES){
			this->max_vqs = MAX_VIRT_QUEUES;
		}
	}
	INIT_LOG("	VIRTIONET:  pioaddr:%x MAC address : %x :%x :%x :%x :%x :%x mis_vector:%x   : max_vqs:%x\n", addr, this->mac[0], this->mac[1],
			this->mac[2], this->mac[3], this->mac[4], this->mac[5], msi_vector,this->max_vqs);


#if 1
	if (msi_vector > 0) {
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 1);
	//	outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0xffff);
	}
#endif

	INIT_LOG("	VIRTIONET: initializing MAX VQ's:%d\n",max_vqs);
	for (i=0; i<max_vqs; i++){
		if (i==1){/* by default only 3 queues will be present , at this point already 2 queues are configured */
			control_q = virtio_create_queue(2*i,VQTYPE_RECV);
			if (control_q){ /* send mq command */
				virtnet_send_command(control_q,max_vqs);
				//break;
			}
		}
		queues[i].recv = virtio_create_queue(2*i,VQTYPE_RECV);
		if (msi_vector > 0) {
			outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, (2*i)+0);
		}
		queues[i].send = virtio_create_queue((2*i)+1,VQTYPE_SEND);
		if (msi_vector > 0) {
			outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, (2*i)+1);
		}
	}

	send_waitq = jnew_obj(wait_queue, "waitq_net", 0);

	if (g_conf_net_send_int_disable == 1){
		for (i=0; i<max_vqs; i++){
			virtio_disable_cb(queues[i].send); /* disable interrupts on sending side */
		}
	}

	if (msi_vector > 0) {
		for (i = 0; i < max_vqs; i++){
			char irq_name[MAX_DEVICE_NAME];
			//if (i==0){
				ut_snprintf(irq_name,MAX_DEVICE_NAME,"%s_recv_msi",this->name);
				ar_registerInterrupt(msi_vector + 2*i, virtio_net_recv_interrupt, irq_name, (void *) this);
			//}
#if 0
			// TODO : enabling sending side interrupts causes freeze in the buffer consumption on the sending side,
			// till  all the buffers are full for the first time. this happens especially on the smp

			if (i!=0){
				ut_snprintf(irq_name,MAX_DEVICE_NAME,"%s_send_msi",jdev->name);
				ar_registerInterrupt(msi_vector + i, virtio_net_send_interrupt, irq_name, (void *) this);
			}
#endif
		}
	}

	for (k = 0; k < max_vqs; k++) {
		if (queues[k].recv == 0 || queues[k].send ==0){
			break;
		}
		for (i = 0; i < max_qbuffers/2; i++){ /* add buffers to recv q */
			addBufToNetQueue(k, VQTYPE_RECV, 0, 4096);
		}
		ut_log("    virtio_netq:%d  recv addbuffers:%d \n",k,max_qbuffers/2);
	}
	inb(pci_ioaddr + VIRTIO_PCI_ISR);
	for (k = 0; k < max_vqs; k++) {
		if (queues[k].recv == 0 || queues[k].send ==0){
			break;
		}
		queue_kick(queues[k].recv);
//	queue_kick(queues[k].send);
	}
	pending_kick_onsend =0;
	recv_interrupt_disabled = 0;

	send_mbuf_start = send_mbuf_len = 0;
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	INIT_LOG("		VirtioNet:  Initialization Completed status:%x\n", virtio_get_pcistatus(pci_ioaddr));

	return 1;
}

jdriver *virtio_net_jdriver::attach_device(class jdevice *jdev) {
	stat_allocs = 0;
	stat_frees = 0;
	stat_err_nospace = 0;
	COPY_OBJ(virtio_net_jdriver, this, new_obj, jdev);
	((virtio_net_jdriver *) new_obj)->net_attach_device();
	jdev->driver = new_obj;
	register_netdevice(jdev);
	return (jdriver *) new_obj;
}

int virtio_net_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}
int virtio_net_jdriver::read(unsigned char *buf, int len, int rd_flags, int opt_flags) {
	return 0;
}

int virtio_net_jdriver::addBufToNetQueue(int qno, int type, unsigned char *buf, unsigned long len) {
	struct scatterlist sg[2];
	int ret;
	struct virtqueue *vq;

	if (type == VQTYPE_RECV){
		vq= queues[qno].recv;
	}else{
		vq= queues[qno].send;
	}

	if (buf == 0) {
		buf = (unsigned char *) jalloc_page(MEM_NETBUF);
		len = 4096; /* page size */
	}
	if (buf == 0){
		BRK;
	}

	ut_memset(buf, 0, sizeof(struct virtio_net_hdr));

	sg[0].page_link = (unsigned long) buf;
	sg[0].length = sizeof(struct virtio_net_hdr);
	sg[0].offset = 0;
	sg[1].page_link = (unsigned long) (buf + sizeof(struct virtio_net_hdr));
	sg[1].length = len - sizeof(struct virtio_net_hdr);
	sg[1].offset = 0;
	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (vq->qType == VQTYPE_RECV) {
		ret = virtio_add_buf_to_queue(vq, sg, 0, 2, (void *) sg[0].page_link, 0);/* recv q*/
	} else {
		ret = virtio_add_buf_to_queue(vq, sg, 2, 0, (void *) sg[0].page_link, 0);/* send q */
	}

	return ret;
}

int virtio_net_jdriver::free_send_bufs(){
	int i;
	unsigned long flags;
	unsigned long addr;
	int len;
	int qno;
	int ret=0;

	for (qno = 0; qno < max_vqs; qno++) {
		i = 0;
		while (i < 50) {
			i++;
			spin_lock_irqsave(&virtionet_lock, flags);
			addr = virtio_removeFromQueue(queues[qno].send, &len);
			spin_unlock_irqrestore(&virtionet_lock, flags);
			if (addr != 0) {
				free_page(addr);
				stat_frees++;
				ret++;
			} else {
				goto next;
			}
		}
		next: ;
	}
	return ret;
}
int virtio_net_jdriver::send_burst() {
	int qno,i, ret=0;
	unsigned long flags;

	if (send_mbuf_len == 0 ){
		return ret;
	}

	spin_lock_irqsave(&virtionet_lock, flags);
	for (qno=0; qno<max_vqs; qno++){  /* try to send from the same queue , if it full then try on the subsequent one, in this way kicks will be less */
		ret = virtio_add_Bulk_to_queue(queues[qno].send, &send_mbuf_list[send_mbuf_start],send_mbuf_len);
	//	ret = virtio_add_Bulk_to_queue(queues[qno].send, &send_mbuf_list[send_mbuf_start],1);
#if 0
		ret = addBufToNetQueue(qno,VQTYPE_SEND, (unsigned char *) send_mbuf_list[send_mbuf_start].buf-10, send_mbuf_list[send_mbuf_start].len + 10);
		if (ret == -ERROR_VIRTIO_ENOSPC){
				continue;
			}
		ret =1;
#endif
		if (ret == 0){
			continue;
		}
		queues[qno].pending_send_kick = 1;
		break;
	}

	if (ret < 0) {
		ret=0;
	}
	if (ret == 0) {
		stat_err_nospace++;
	}else{
		if (ret == send_mbuf_len){ /* empty the mbuf_list */
			send_mbuf_start = 0;
			send_mbuf_len = 0;
		}else{
			send_mbuf_start = send_mbuf_start + ret;
			send_mbuf_len = send_mbuf_len - ret;
		}
		pending_kick_onsend = 1;
		stat_sends++;
	}
	spin_unlock_irqrestore(&virtionet_lock, flags);

	free_send_bufs();
	return ret;  /* Here Sucess indicates the buffer is freed or consumed */
}
int virtio_net_jdriver::write(unsigned char *data, int len, int wr_flags) {
	jdevice *dev;
	int i, ret;
	unsigned long flags;
	unsigned long addr;

	dev = (jdevice *) this->device;
	if (dev == 0 || data == 0)
		return JFAIL;

	//if (wr_flags & WRITE_BUF_CREATED) {
	addr = data-10;
	ret = -ERROR_VIRTIO_ENOSPC;

	//current_send_q++;  /* alternate packet on each */
	spin_lock_irqsave(&virtionet_lock, flags);
	int qno;
	for (qno=0; qno<max_vqs; qno++){  /* try to send from the same queue , if it full then try on the subsequent one, in this way kicks will be less */
		//spin_lock_irqsave(&virtionet_lock, flags);
		ret = addBufToNetQueue(qno,VQTYPE_SEND, (unsigned char *) addr, len + 10);
		//spin_unlock_irqrestore(&virtionet_lock, flags);
		if (ret == -ERROR_VIRTIO_ENOSPC){
			continue;
		}
		queues[qno].pending_send_kick = 1;
		break;
	}

	if (ret == -ERROR_VIRTIO_ENOSPC) {
		stat_err_nospace++;
		if ((wr_flags & WRITE_SLEEP_TILL_SEND) == 0){
			free_page((unsigned long) addr);
			stat_frees++;
			ret = JSUCCESS;
			pending_kick_onsend = 1;
		}
	}else{
		pending_kick_onsend = 1;
		stat_sends++;
		ret = JSUCCESS;
	}
	spin_unlock_irqrestore(&virtionet_lock, flags);

	free_send_bufs();
	return ret;  /* Here Sucess indicates the buffer is freed or consumed */
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
		if (pending_kick_onsend!=0){
			unsigned long flags;
			int qno;
			spin_lock_irqsave(&virtionet_lock, flags);
			for (qno=0; qno<max_vqs && qno<MAX_VIRT_QUEUES; qno++){
				if (queues[qno].pending_send_kick == 1){
					queue_kick(queues[qno].send);
					queues[qno].pending_send_kick=0;
				}
			}
			pending_kick_onsend=0;
			spin_unlock_irqrestore(&virtionet_lock, flags);

			return JSUCCESS;
		}else{
			return JFAIL;
		}
	} else if (arg1 == NETDEV_IOCTL_DISABLE_RECV_INTERRUPTS){
		virtio_disable_cb(queues[0].recv);
		recv_interrupt_disabled = 1;

	} else if (arg1 == NETDEV_IOCTL_ENABLE_RECV_INTERRUPTS){
		virtio_enable_cb(queues[0].recv);
		recv_interrupt_disabled = 0;
	} else if (arg1 == NETDEV_IOCTL_PRINT_STAT){
		ut_printf("Total queues: %d\n",max_vqs);
		for (i=0; i<max_vqs && i<MAX_VIRT_QUEUES; i++){
			if (queues[i].recv==0 || queues[i].send==0){
				break;
			}
			ut_printf("VQ-%d :\n",i);
			print_vq(queues[i].recv);
			print_vq(queues[i].send);
		}
		ut_printf("Control-Q :\n");
		print_vq(control_q);

	}
}
/*****************************  Virtio Disk ********************************************/
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_SCSI_CMD 2
#define VIRTIO_BLK_T_SCSI_CMD_OUT 3
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_FLUSH_OUT 5
#define VIRTIO_BLK_T_BARRIER 0x80000000

#define DISK_READ 0
#define DISK_WRITE 1

#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

static wait_queue *disk_thread_waitq;
static int virtio_disk_interrupt(void *private_data) {

	jdevice *dev;
	virtio_disk_jdriver *driver = (virtio_disk_jdriver *) private_data;

	dev = (jdevice *) driver->device;
	if (dev->pci_device.msi_enabled == 0){
		inb(dev->pci_device.pci_ioaddr + VIRTIO_PCI_ISR);
	}

	driver->stat_recv_interrupts++;
	if (driver->waitq != 0){
		driver->waitq->wakeup();
	}
	if (disk_thread_waitq != 0){
		disk_thread_waitq->wakeup();
	}
	return 0;
}

struct virtio_scsi_blk_req {
	/* out hdr-1, common to block and scsi */
	uint32_t type ;
	uint32_t ioprio ;
	uint64_t sector ;

	/* out hdr-2, common to block and scsi */
	uint8_t scsi_cmd[100];

	/* in for read /out for write , common to block and scsi */
	uint8_t data[VIRTIO_BLK_DATA_SIZE]; /*TODO:  currently it made fixed, actually it is  variable size data here like data[][512]; */

	/* in hdr-1, common to block and scsi */
	#define SCSI_SENSE_BUFFERSIZE 96
	uint8_t sense[SCSI_SENSE_BUFFERSIZE] ;

	/* in hdr-2, common to block and scsi */
uint32_t errors;
uint32_t data_len;
uint32_t sense_len ;
uint32_t residual ;

    /* in hdr-3, common to block and scsi */
	uint8_t status ;
	int len;
};

enum {
SCSI_CMD_TEST_UNIT_READY = 0x00,
SCSI_CMD_REQUEST_SENSE = 0x03,
SCSI_CMD_INQUIRY = 0x12,
SCSI_CMD_READ_16 = 0x88,
SCSI_CMD_WRITE_16 = 0x8A,
SCSI_CMD_READ_CAPACITY = 0x9E,
SCSI_CMD_SYNCHRONIZE_CACHE_10 = 0x35,
SCSI_CMD_SYNCHRONIZE_CACHE_16 = 0x91,
SCSI_CMD_REPORT_LUNS = 0xA0,
};
struct cdb_readwrite_16 {
	uint8_t command;
	uint8_t flags;
	uint64_t lba;
	uint32_t count;
	uint8_t group_number;
	uint8_t control;
} __attribute__((packed));


static __inline uint32_t bswap32(uint32_t __x)
{
return (__x>>24) | (__x>>8&0xff00) | (__x<<8&0xff0000) | (__x<<24);
}
static __inline uint64_t bswap64(uint64_t __x)
{
return ((bswap32(__x)+0ULL)<<32) | bswap32(__x>>32);
}
static void req_construct(unsigned char *buf, unsigned long offset, unsigned long len){
	struct cdb_readwrite_16 *req = (struct cdb_readwrite_16 *)buf;
	uint64_t lba;
	uint32_t count;

	lba = offset/512;
	count = len/512;
	req->lba = bswap64(lba);
	req->count = bswap32(count);
	req->command = SCSI_CMD_READ_16;
}
void *virtio_disk_jdriver::scsi_addBufToQueue(int type, unsigned char *buf_arg, uint64_t len, uint64_t sector, uint64_t data_len) {
	struct virtqueue *tmp_vq = this->queues[0].send;
	struct virtio_scsi_blk_req *req;
	struct scatterlist sg[8];
	int ret;
	int transfer_len=len;
	int out,in;
	unsigned char *buf=0;
ut_printf("Sending the SCSI request sector:%x len:%d  :%d \n",sector,len,data_len);
	if (buf == 0) {
		//buf = (unsigned char *) jalloc_page(0);
		buf = mm_getFreePages(0,1);  /* TODO: for write request does not work, need to copy the data buf */
	}
	if (buf == 0 ){
		BRK;
	}

	req = (struct virtio_scsi_blk_req *)buf;
	ut_memset(buf, 0, sizeof(struct virtio_scsi_blk_req));
	req->sector = sector;
	if (type == DISK_READ){
		req->type = VIRTIO_BLK_T_SCSI_CMD;
		out = 2;
		in = 4;
	}else{
		req->type = VIRTIO_BLK_T_OUT;
		if (data_len>VIRTIO_BLK_DATA_SIZE){
			ut_memcpy(buf+16, buf_arg, VIRTIO_BLK_DATA_SIZE);
		}else{
			ut_memcpy(buf+16, buf_arg, data_len);
		}
		out = 2;
		in = 1;

	}
	req->status = 0xff;
	int cmd_len=100;
	int scsi_sense_hdr =  SCSI_SENSE_BUFFERSIZE;
	int resp_len=4*4;  /* scsi response */
	if (transfer_len < blk_size){
		transfer_len=blk_size;
	}

	sg[0].page_link = (unsigned long) buf;
	sg[0].length = 16;
	sg[0].offset = 0;

	req_construct(buf+16, sector*512, len );
	sg[1].page_link = (unsigned long) buf + 16;
	sg[1].length = 16;
	sg[1].offset = 0;

	sg[2].page_link = (unsigned long) (buf + 16 + cmd_len);
	sg[2].length =  transfer_len;
	sg[2].offset = 0;


	sg[3].page_link = (unsigned long) (buf + 16 + cmd_len + VIRTIO_BLK_DATA_SIZE);
	sg[3].length =  scsi_sense_hdr;
	sg[3].offset = 0;

	sg[4].page_link = (unsigned long) (buf + 16 + cmd_len + VIRTIO_BLK_DATA_SIZE + scsi_sense_hdr);
	sg[4].length =  resp_len;
	sg[4].offset = 0;

	sg[5].page_link = (unsigned long) (buf + 16 + cmd_len +  VIRTIO_BLK_DATA_SIZE + scsi_sense_hdr + resp_len);
	sg[5].length =  1;
	sg[5].offset = 0;

	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));

	virtio_disable_cb(tmp_vq);
	ret = virtio_add_buf_to_queue(tmp_vq, sg, out, in, (void *) sg[0].page_link, 0);/* send q */
	queue_kick(queues[0].send);

	virtio_enable_cb(tmp_vq);
sc_sleep(2000);
	return (void *)buf;
}
struct virtio_blk_req *virtio_disk_jdriver::createBuf(int type, unsigned char *user_buf,  uint64_t sector, uint64_t data_len) {
	unsigned char *buf=0;
	struct virtio_blk_req *req;
	int donot_copy=0;

	if (user_buf >= pc_startaddr && user_buf<pc_endaddr){
		donot_copy = 1;
		buf = mm_getFreePages(0,0);
		req = (struct virtio_blk_req *)buf;
		ut_memset(buf, 0, sizeof(struct virtio_blk_req));
		req->user_data =user_buf;
	}else{
		buf = mm_getFreePages(0,1);
		req = (struct virtio_blk_req *)buf;
		ut_memset(buf, 0, sizeof(struct virtio_blk_req));
		req->user_data =0;
	}

	req->sector = sector;
	if (type == DISK_READ){
		req->type = VIRTIO_BLK_T_IN;
	}else{
		req->type = VIRTIO_BLK_T_OUT;
		if (donot_copy == 0){
			ut_memcpy(&req->data[0], user_buf, data_len);
		}
	}
	req->status = 0xff;
	req->len = data_len;
	return req;
}
void virtio_disk_jdriver::addBufToQueue(struct virtio_blk_req *req, int transfer_len) {
	struct virtqueue *tmp_vq = this->queues[0].send;
	struct scatterlist sg[4];
	int ret;
	int out,in;
	unsigned char *buf=(unsigned char *)req;

	if (req->type == VIRTIO_BLK_T_IN){
		out = 1;
		in = 2;
	}else{
		out = 2;
		in = 1;
	}
	sg[0].page_link = (unsigned long) buf;
	sg[0].length = 16;
	sg[0].offset = 0;

	if (req->user_data == 0){

		sg[1].page_link = (unsigned long) (buf + 16 + 8 + 8);
	}else{
		sg[1].page_link = req->user_data;
	}
	if (transfer_len < blk_size){
		transfer_len=blk_size;
	}
	//ut_log(" buf:%x  len:%d  blk_size:%d\n",sg[1].page_link,transfer_len, blk_size);
	sg[1].length =  transfer_len;
	sg[1].offset = 0;

	sg[2].page_link = (unsigned long) (buf + 16 );
	sg[2].length =  1;
	sg[2].offset = 0;

	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));

	virtio_disable_cb(tmp_vq);
	ret = virtio_add_buf_to_queue(tmp_vq, sg, out, in, (void *) sg[0].page_link, 0);/* send q */
	queue_kick(queues[0].send);

	if (interrupts_disabled == 0){
		virtio_enable_cb(tmp_vq);
	}

	return ;
}
static uint64_t virtio_config64(unsigned long pcio_addr){
	uint64_t ret;
	auto addr = pcio_addr  + VIRTIO_MSI_CONFIG_VECTOR ;
	ret = inl(addr);
	ret=ret + (inl(addr+4)* (0x1 << 32));
	return ret;
}
static uint32_t virtio_config32(unsigned long pcio_addr){
	uint64_t ret;
	auto addr = pcio_addr  + VIRTIO_MSI_CONFIG_VECTOR;
	ret = inl(addr);
	return ret;
}

static uint16_t virtio_config16(unsigned long pcio_addr){
	uint64_t ret;
	auto addr = pcio_addr  + VIRTIO_MSI_CONFIG_VECTOR;
	ret = inw(addr);
	return ret;
}

 /* Feature bits */
#define VIRTIO_BLK_F_SIZE_MAX   1       /* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX    2       /* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY   4       /* Legacy geometry available  */
#define VIRTIO_BLK_F_RO         5       /* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE   6       /* Block size of disk is available*/
#define VIRTIO_BLK_F_TOPOLOGY   10      /* Topology information is available */
#define VIRTIO_BLK_F_MQ         12      /* support more than one vq */

/* Legacy feature bits */
//#ifndef VIRTIO_BLK_NO_LEGACY
#define VIRTIO_BLK_F_BARRIER    0       /* Does host support barriers? */
#define VIRTIO_BLK_F_SCSI       7       /* Supports scsi command passthru */
#define VIRTIO_BLK_F_WCE        9       /* Writeback mode enabled after reset */
#define VIRTIO_BLK_F_CONFIG_WCE 11      /* Writeback mode available in config */
/*
 * struct v i r t i o _ b l k _ c o n f i g {
	u64 capacity ;
	u32 size_max ;
	u32 seg_max ;
	s t r u c t v i r t i o _ b l k _ g e o m e t r y {
		u16 cylinders ;
		u8 heads;
		u8 sectors;
	} g e o m e t r y ;
	u32 blk_size;

	 the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY
        exponent for physical block per logical block.
         __u8 physical_block_exp;
        **  alignment offset in logical blocks. --
         __u8 alignment_offset;
         ** minimum I/O size without performance penalty in logical blocks. --/
         __u16 min_io_size;
         ** optimal sustained I/O size in logical blocks. --
         __u32 opt_io_size;
        ** writeback mode (if VIRTIO_BLK_F_CONFIG_WCE) --
         __u8 wce;
         __u8 unused;

        ** number of vqs, only available when VIRTIO_BLK_F_MQ is set **
         __u16 num_queues;
}
 */
int virtio_disk_jdriver::disk_attach_device(class jdevice *jdev) {
	auto pci_ioaddr = jdev->pci_device.pci_ioaddr;
	pci_dev_header_t *pci_hdr = &device->pci_device.pci_header;
	uint32_t features;
	uint32_t guest_features = 0;
	uint32_t mask_features = 0;
	uint32_t msi_vector;

	this->device = jdev;
	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	ut_log("	Virtio disk: Initializing status :%x : \n",virtio_get_pcistatus(pci_ioaddr));

	virtio_set_pcistatus(pci_ioaddr, virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER);
	auto addr = pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	ut_log("	Virtio disk: Initializing VIRTIO PCI hostfeatures :%x: status :%x :\n", features, virtio_get_pcistatus(pci_ioaddr));
	guest_features = features;
	mask_features = (0x0007ff);

	guest_features = guest_features & mask_features;
	addr = pci_ioaddr + VIRTIO_PCI_GUEST_FEATURES;
	outl(addr, features);

	if (pci_hdr->capabilities_pointer != 0) {
		msi_vector = pci_read_msi(&device->pci_device.pci_addr,
				&device->pci_device.pci_header, &device->pci_device.pci_bars[0],
				device->pci_device.pci_bar_count, &device->pci_device.msix_cfg);
		if (msi_vector > 0) {
#if 1
			pci_enable_msix(&device->pci_device.pci_addr, &device->pci_device.msix_cfg,
					device->pci_device.pci_header.capabilities_pointer);
#else
			msi_vector = 0;
#endif
			ut_log("  virtio_disk  MSI available  :%d:%x \n", msi_vector,
					msi_vector);
		}
	} else {
		msi_vector = 0;
	}

	int i;
	unsigned long config_data;
	disk_size = 0;

	if (jdev->pci_device.pci_header.device_id != VIRTIO_PCI_SCSI_DEVICE_ID) {
		auto cfg_addr = pci_ioaddr;
		if (msi_vector != 0){
			cfg_addr = cfg_addr + 4;
		}
		disk_size = virtio_config64(cfg_addr + 0) * 512;
		blk_size = virtio_config32(cfg_addr + 20);
		max_vqs = virtio_config16(cfg_addr + 34);
		ut_log(" 	Virtio-blk Disk size:%d(%x)  blk_size:%d   max_vqs: %d \n",
				disk_size, disk_size, blk_size, max_vqs);
	} else {
		ut_log("	virtio-scsi  Num of Reques Queues: %d \n",
				virtio_config32(pci_ioaddr + 0));
		INIT_LOG("	SCSI seg max: %d \n", virtio_config32(pci_ioaddr + 4));
		INIT_LOG("	SCSI max sector: %d \n", virtio_config32(pci_ioaddr + 8));
		INIT_LOG("	SCSI cmd_per_lun: %d \n", virtio_config32(pci_ioaddr + 12));
		INIT_LOG("	SCSI event_info_size: %d \n",
				virtio_config32(pci_ioaddr + 16));
		INIT_LOG("	SCSI sense size: %d \n", virtio_config32(pci_ioaddr + 20));
		INIT_LOG("	SCSI cdb size: %d \n", virtio_config32(pci_ioaddr + 24));
	}
	if (jdev->pci_device.pci_header.device_id != VIRTIO_PCI_SCSI_DEVICE_ID) {
		this->queues[0].send = this->virtio_create_queue(0, VQTYPE_SEND);
	} else {
		this->queues[0].send = this->virtio_create_queue(0, VQTYPE_SEND);
	}
	if (msi_vector > 0) {
		ut_log("  virtio disk :  msi vectors: %d\n",jdev->pci_device.msix_cfg.isr_vector);
		outw(pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0);
		ar_registerInterrupt(msi_vector, virtio_disk_interrupt, "virtio_disk_msi", this);
		inb(pci_ioaddr + VIRTIO_PCI_ISR);
	} else {
		INIT_LOG("	Virtio disk:  VIRTIO PCI COMPLETED with driver ok :%x \n", virtio_get_pcistatus(pci_ioaddr));
		inb(pci_ioaddr + VIRTIO_PCI_ISR);
		ar_registerInterrupt(32 + jdev->pci_device.pci_header.interrupt_line, virtio_disk_interrupt, "virt_disk_irq", (void *) this);
	}

	INIT_LOG("		driver status:  %x :\n", virtio_get_pcistatus(pci_ioaddr));
	virtio_set_pcistatus(pci_ioaddr,virtio_get_pcistatus(pci_ioaddr) + VIRTIO_CONFIG_S_DRIVER_OK);
	INIT_LOG("		second time	Virtio disk:  VIRTIO PCI COMPLETED with driver ok :%x \n",virtio_get_pcistatus(pci_ioaddr));
	interrupts_disabled =0;

//	virtio_disable_cb(this->queues[0].send); /* disable interrupts on sending side */
	return 1;
}

int virtio_disk_jdriver::probe_device(class jdevice *jdev) {
	if ((jdev->pci_device.pci_header.vendor_id == VIRTIO_PCI_VENDOR_ID)
			&& ((jdev->pci_device.pci_header.device_id == VIRTIO_PCI_BLOCK_DEVICE_ID) ||
		(jdev->pci_device.pci_header.device_id == VIRTIO_PCI_SCSI_DEVICE_ID))){
		ut_log("		Matches the disk Probe :%d\n",jdev->pci_device.pci_header.device_id);
		return JSUCCESS;
	}
	return JFAIL;
}
extern jdriver *disk_drivers[];

jdriver *virtio_disk_jdriver::attach_device(class jdevice *jdev) {
	int i;

	stat_allocs = 0;
	stat_frees = 0;
	stat_err_nospace = 0;
	COPY_OBJ(virtio_disk_jdriver, this, new_obj, jdev);
	((virtio_disk_jdriver *) new_obj)->disk_attach_device(jdev);
	for (i=0; i<5; i++){
		if (disk_drivers[i]==0){
			disk_drivers[i]=(jdriver *) new_obj;
			break;
		}
	}

	((virtio_disk_jdriver *) new_obj)->waitq = jnew_obj(wait_queue, "waitq_disk", 0);
	//spin_lock_init(&((virtio_disk_jdriver *)new_obj)->io_lock);
	init_tarfs((jdriver *)new_obj);
	return (jdriver *) new_obj;
}

int virtio_disk_jdriver::dettach_device(jdevice *jdev) {
	/*TODO:  Need to free the resources */
	return JFAIL;
}



#define MAX_REQS 10
static int diskio_submit_requests(struct virtio_blk_req **buf,int len,virtio_disk_jdriver *dev, unsigned char *user_buf,int user_len,int intial_skip, int read_ahead);
int virtio_disk_jdriver::disk_io(int type,unsigned char *buf, int len, int offset, int read_ahead) {
	struct virtio_blk_req *reqs[MAX_REQS],*tmp_req;
	int sector;
	int i,req_count,data_len,curr_len,max_reqs;
	int initial_skip, blks;
	unsigned long addr,flags;
	int qlen, ret;
	ret = 0;
	int curr_offset;
	int scsi_type=0;

	if (device->pci_device.pci_header.device_id == VIRTIO_PCI_SCSI_DEVICE_ID){
		scsi_type=1;
		ut_printf(" scsi reading ..\n");
		//BRK;
	}

	sector = offset / blk_size;
	initial_skip = offset - sector * blk_size;

	data_len = len + initial_skip;
	curr_offset=offset-initial_skip;
	curr_len=data_len;
	max_reqs = 5;

	for (req_count = 0; req_count < max_reqs  && curr_len>0; req_count++) {
		int req_len = curr_len;
		if (req_len > VIRTIO_BLK_DATA_SIZE) {
			req_len = VIRTIO_BLK_DATA_SIZE;
		}
		if ((req_len + curr_offset) >= disk_size) {
			req_len = disk_size - curr_offset;
		}

		blks = req_len / blk_size;
		if ((blks * blk_size) != req_len) {
			req_len = (blks + 1) * blk_size;
		}
		reqs[req_count] = createBuf(type, buf+(req_count*VIRTIO_BLK_DATA_SIZE), sector, req_len);
		curr_offset = curr_offset + VIRTIO_BLK_DATA_SIZE;
		curr_len = curr_len - VIRTIO_BLK_DATA_SIZE;
	}
	ret = diskio_submit_requests(reqs, req_count,this, buf, len, initial_skip, read_ahead);

//		ut_printf("%d -> %d  DATA :%x :%x  disksize:%d blksize:%d\n",i,disk_bufs[i]->status,disk_bufs[i]->data[0],disk_bufs[i]->data[1],disk_size,blk_size);
	return ret;
}

int virtio_disk_jdriver::read(unsigned char *buf, int len, int offset, int read_ahead) {
	int ret;
//ut_log(" read len :  %d offset:%d \n",len, offset);
	ret = disk_io(DISK_READ,buf,len,offset,read_ahead);
	return ret;
}
int virtio_disk_jdriver::write(unsigned char *buf, int len, int offset) {
	int ret;

	ret = disk_io(DISK_WRITE,buf,len,offset,0);
	return ret;
}
int virtio_disk_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	if (arg1 == IOCTL_DISK_SIZE){
		return disk_size;
	}
	return JSUCCESS;
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

	this->vq[0] = this->virtio_create_queue(0, VQTYPE_SEND);
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
static unsigned long current_req_no=0;
static unsigned long current_txt_no=0;
static unsigned long stat_diskcopy_txts=0;  /*  for non-pagecache disk pages are copied to user space */
static unsigned long stat_read_ahead_txts=0;
#define MAX_DISK_REQS 100
struct diskio_req{
	struct virtio_blk_req *buf;  /* memory buffer, if this empty then rest of entries are invalid */
	int data_len;
	virtio_disk_jdriver *dev;
	int state;
	int read_ahead;
	unsigned long req_no,serial_no;
	int initial_skip;
	unsigned long hits;
	unsigned char *user_buf;
	int user_len;
};
struct diskio_req disk_reqs[MAX_DISK_REQS];
static spinlock_t diskio_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"diskio");

enum {
	STATE_REQ_QUEUED =1,
	STATE_REQ_COMPLETED = 2
};
extern "C" {
void Jcmd_stat_diskio(unsigned char *arg1, unsigned char *arg2){
	int i;
	ut_printf(" current req_no:%d  txt+no: %d diskcopies: %d read_ahead:%d \n",current_req_no,current_req_no,stat_diskcopy_txts,stat_read_ahead_txts);
	for (i=0; i<MAX_DISK_REQS; i++){
		if (disk_reqs[i].buf == 0 && disk_reqs[i].hits==0) continue;
		ut_printf(" %d: buf:%x hits:%d req_no:%d \n",i,disk_reqs[i].buf,disk_reqs[i].hits,disk_reqs[i].req_no);
	}

	ut_printf("  disk devices: \n");
	for (i=0; i<5; i++){
		if (disk_drivers[i]!=0){
			virtio_disk_jdriver *dev=disk_drivers[i];
			print_vq(dev->queues[0].send);
		}
	}
}
extern int pc_read_ahead_complete(unsigned long addr);
}

static int get_from_diskreq(int i, unsigned long req_no) {
	int ret;
	int initial_skip;
	int tlen;
	struct virtio_blk_req *req;

	ret = 0;
	if (disk_reqs[i].buf == 0) {
		return ret;
	}
	if (disk_reqs[i].req_no == req_no && disk_reqs[i].buf != 0) {
		unsigned long start_time = g_jiffies;
		while (disk_reqs[i].state != STATE_REQ_COMPLETED) {
			disk_reqs[i].dev->waitq->wait(5);
			if ((g_jiffies-start_time) > 300) { /* if it more then 3 seconds */
				return -1;
			}
		}
		initial_skip = disk_reqs[i].initial_skip;
		req = disk_reqs[i].buf;
		if ((VIRTIO_BLK_DATA_SIZE - initial_skip) > disk_reqs[i].user_len) {
			tlen = disk_reqs[i].user_len;
		} else {
			tlen = VIRTIO_BLK_DATA_SIZE - initial_skip;
		}
		if (req->user_data == 0) {
			ut_memcpy(disk_reqs[i].user_buf + (i * VIRTIO_BLK_DATA_SIZE),
					&req->data[initial_skip], tlen);
			stat_diskcopy_txts++;
			mm_putFreePages(disk_reqs[i].buf, 1);
		} else {
			mm_putFreePages(disk_reqs[i].buf, 0);
		}
		ret = ret + tlen;
		if (disk_reqs[i].read_ahead == 1) {
			pc_read_ahead_complete((unsigned long) disk_reqs[i].user_buf);
			stat_read_ahead_txts++;
			disk_reqs[i].read_ahead = 0;
		}
		disk_reqs[i].buf = 0;
	}

	return ret;
}

extern "C"{
extern int  g_conf_read_ahead_pages;
}

static int diskio_submit_requests(struct virtio_blk_req **reqs, int req_count, virtio_disk_jdriver *dev, unsigned char *user_buf,int user_len,int initial_skip, int read_ahead){
	int i,k;
	int ret = -1;
	unsigned long flags;
	unsigned long req_no=0;

	k=0;
	if (read_ahead == 1 ){
		if ( req_count > 1){
			return 0;
		}else{
			if (!(user_buf >= pc_startaddr && user_buf<pc_endaddr)) { /* make sure the addr is from pc */
				return 0;
			}
		}
	}

	spin_lock_irqsave(&diskio_lock, flags);
	if (current_req_no == 0){
		current_req_no =1;
	}
	current_req_no++;
	req_no = current_req_no;
	for (i=0; i<MAX_DISK_REQS && k<req_count; i++){
		if (disk_reqs[i].buf!=0) { continue; }
		disk_reqs[i].dev = dev;
		disk_reqs[i].data_len = reqs[k]->len;
		disk_reqs[i].state = 0;
		disk_reqs[i].req_no = req_no;
		//ut_log("%d: user_buf :%x len :%d donotcopy:%d data:%x \n",current_req_no,user_buf,user_len,donot_copy,reqs[k]->user_data);
		disk_reqs[i].serial_no = k;
		if (k==0){
			disk_reqs[i].initial_skip  = initial_skip;
		}else{
			disk_reqs[i].initial_skip  = 0;
		}
		disk_reqs[i].read_ahead = read_ahead;
		disk_reqs[i].user_buf = user_buf;
		disk_reqs[i].user_len = user_len;
		disk_reqs[i].buf = reqs[k];
		disk_reqs[i].hits++;
		ret = i;
		k++;
		current_txt_no++;
	}
	spin_unlock_irqrestore(&diskio_lock, flags);
	if (ret >= 0){
		if (read_ahead == 1){
			int count=0;
			for (i=0; i<MAX_DISK_REQS ; i++){
				if (disk_reqs[i].buf!=0 && disk_reqs[i].state==0) { count++; }
			}
			if (count > (g_conf_read_ahead_pages/2)){
					disk_thread_waitq->wakeup();
			}
			ret = user_len;
		}else{
			disk_thread_waitq->wakeup(); /* check if the buffers are got updated by the disk thread */
			ret = get_from_diskreq(ret, req_no);
		}
	}
	return ret;
}

extern "C" {

static int extract_reqs_from_devices(virtio_disk_jdriver *dev) {
	int ret = 0;
	int loop = 10;
	int i, qlen;
	unsigned char *req;

	while (loop > 0) {
		loop--;
		req = virtio_removeFromQueue(dev->queues[0].send, &qlen);
		if (req == 0 ) return ret;
		for (i = 0; i < MAX_DISK_REQS; i++) {
			if (disk_reqs[i].buf == 0) {
				continue;
			}
			if (disk_reqs[i].buf == req) {
				disk_reqs[i].state = STATE_REQ_COMPLETED;
				ret++;
				break;
			}
		}
	}
	return ret;
}
int diskio_thread(void *arg1, void *arg2) {
	int i;
	int pending_req;
	int progress =0;
	int intr_disabled=0;
	while(1){
#if 1
		if (progress == 0 ){
			if (intr_disabled == 1){
				//disk_thread_waitq->wait(1);
				sc_schedule(); /* give chance other threads to run */
			}else{
				disk_thread_waitq->wait(50);
			}
		}
#else
		if (pending_req == 0){
			disk_thread_waitq->wait(400);
		}else if (progress == 0) {
			disk_thread_waitq->wait(1);
		}
#endif
		progress =0;
		pending_req=0;
		intr_disabled = 0;
		for (i=0; i<MAX_DISK_REQS; i++){
			if (disk_reqs[i].buf == 0) { continue; }
			pending_req++;
			if (disk_reqs[i].dev->interrupts_disabled == 1){
				intr_disabled = 1;
			}
			if (disk_reqs[i].state == 0){
				disk_reqs[i].dev->addBufToQueue(disk_reqs[i].buf, disk_reqs[i].data_len);
				disk_reqs[i].state = STATE_REQ_QUEUED;
				progress++;
			}
			if (disk_reqs[i].state == STATE_REQ_QUEUED){
				progress = progress +extract_reqs_from_devices(disk_reqs[i].dev);
			}
			if (disk_reqs[i].state == STATE_REQ_COMPLETED){
				if ((disk_reqs[i].read_ahead == 1) && disk_reqs[i].state == STATE_REQ_COMPLETED){
					get_from_diskreq(i,disk_reqs[i].req_no);
					progress++;
				}else{
					disk_reqs[i].dev->waitq->wakeup();
				}
			}
		}
	}
}

void init_virtio_drivers() {
	int pid,i;

	/* init p9 */
	p9_jdriver = jnew_obj(virtio_p9_jdriver);
	p9_jdriver->name = (unsigned char *) "p9_driver";
	register_jdriver(p9_jdriver);

	/* init net */
	net_jdriver = jnew_obj(virtio_net_jdriver);
	net_jdriver->name = (unsigned char *) "net_driver";
	register_jdriver(net_jdriver);

	for (i=0; i<MAX_DISK_REQS; i++){
		disk_reqs[i].buf=0;
	}
	disk_thread_waitq = jnew_obj(wait_queue, "disk_thread_waitq", 0);
	pid = sc_createKernelThread(diskio_thread, 0, (unsigned char *) "disk_io", 0);

	/* init disk */
	disk_jdriver = jnew_obj(virtio_disk_jdriver);
	disk_jdriver->name = (unsigned char *) "disk_driver";
	register_jdriver(disk_jdriver);
}

struct virtqueue *virtio_jdriver_getvq(void *driver, int index) {
	virtio_p9_jdriver *jdriver = (virtio_p9_jdriver *) driver;

	return jdriver->vq[index];
}
}
