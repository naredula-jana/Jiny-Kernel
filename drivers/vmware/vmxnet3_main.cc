/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   vmware/vmxnet3.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "mach_dep.h"

#include "vmxnet3.h"
extern int g_conf_net_send_int_disable;
}

#define VMXNET3_DEF_TX_RING_SIZE    512
#define VMXNET3_DEF_RX_RING_SIZE    256
#define VMXNET3_DEF_RX_RING2_SIZE   128

#define MAX_TX_RINGS 8
#define MAX_RX_RINGS 8

struct tx_queue {
		struct Vmxnet3_TxDesc *ring;
		struct Vmxnet3_TxDataDesc *data_ring;
		struct Vmxnet3_TxCompDesc *comp_ring;
		int gen;
		int next2refill;  /* empty buffers */
		int next2complete; /* consumed buffers */
		unsigned long stat_refills;
		unsigned long stat_completes;
	};
struct rx_queue {
	struct Vmxnet3_RxDesc *ring1,*ring2;
	struct Vmxnet3_RxCompDesc *comp_ring;
	int gen;
	int next2refill;  /* empty buffers */
	int next2complete; /* consumed buffers */
	unsigned long stat_refills;
	unsigned long stat_completes;
};
struct Vmxnet3_Adapter{
	unsigned char num_tx_queues,num_rx_queues;
	unsigned int  tx_ring_size,rx_ring_size;

	struct tx_queue tx_queues[MAX_TX_RINGS];
	struct rx_queue rx_queues[MAX_RX_RINGS];

	struct Vmxnet3_TxQueueDesc *tx_qdesc;
	struct Vmxnet3_RxQueueDesc *rx_qdesc;
};
class vmxnet3_jdriver: public jnetdriver {
	struct bar0{
		 unsigned char unused1[0x600];
		 unsigned int tx_notify; /* VMXNET3_REG_TXPROD */
		 unsigned char unsused2[0x100];
	}*bar0_struct;
	struct bar1{
		unsigned char version[8];
		unsigned char upt_version[8];
		unsigned int shared_addr_low[2];  /* one will be unused */
		unsigned int shared_addr_high[2];  /* one will be unused */
		unsigned int command;
		unsigned int unused;
		unsigned int mac[4];
	}*bar1_struct;

	unsigned long shared_addr;
	struct Vmxnet3_Adapter *adapter;

	int setup_shared();
	int net_attach_device();

	int refill_recv_queue();
	int BulkRemoveFromRecvQueue(struct struct_mbuf *mbuf_list, int list_len);

	int detach_usedbuf_from_sendqueue(int qno,struct struct_mbuf *mbuf_list, int list_len);
	int BulkAddToSendQueue(int qno,struct struct_mbuf *mbuf_list, int list_len);

public:
	int burst_recv(int total_pkts);
	int burst_send();
	int check_for_pkts();

	void print_stats(unsigned char *arg1,unsigned char *arg2);
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len, int flags, int opt_flags);
	int write(unsigned char *buf, int len, int flags);
	int ioctl(unsigned long arg1,unsigned long arg2);

	/* TODO : below unused
	wait_queue *send_waitq;
	unsigned char mac[7];
	int recv_interrupt_disabled;
	int send_kick_needed;*/
};
void vmxnet3_jdriver::print_stats(unsigned char *arg1,unsigned char *arg2) {
	ut_printf(" VMXNET3 :  ");
	ut_printf(" Recv refills:%d completes:%d\n",adapter->rx_queues[0].stat_refills,adapter->rx_queues[0].stat_completes);
	ut_printf("          Tx refills:%d completes:%d\n",adapter->tx_queues[0].stat_refills,adapter->tx_queues[0].stat_completes);
}

int vmxnet3_jdriver::check_for_pkts() {
	if (adapter == 0){
		return 0;
	}
	return 1; /* TODO */
}

int vmxnet3_jdriver::BulkAddToSendQueue(int qno,struct struct_mbuf *mbuf_list, int list_len) {
	unsigned long pkt;
	int k,i,len,ret=0;
	struct tx_queue *queue = &adapter->tx_queues[qno];

	for (k=0; k<list_len; k++){
		i = queue->next2refill;
		if (queue->ring[i].addr != 0){
			break;
		}

		pkt = mbuf_list[k].buf;
		len= mbuf_list[k].len;

		queue->ring[i].addr = __pa(pkt);
		queue->ring[i].len = len;
		queue->ring[i].eop = 1;
		queue->ring[i].gen = adapter->tx_queues[0].gen & (0x1);

		queue->next2refill++;
		queue->stat_refills++;
		if (queue->next2refill == adapter->tx_ring_size) {
			queue->next2refill = 0;
			queue->gen++;
		}
		ret++;
	}
	return ret;
}

int vmxnet3_jdriver::detach_usedbuf_from_sendqueue(int qno,struct struct_mbuf *mbuf_list, int list_len) {
	int i,k;
	int ret=0;
	struct tx_queue *queue = &adapter->tx_queues[qno];

	for (i=0; i < list_len; i++) {
		k=queue->next2complete;
		if (queue->comp_ring[k].gen == queue->ring[k].gen && queue->ring[k].addr!=0){
			mbuf_list[i].buf = __va(queue->ring[k].addr);
			mbuf_list[i].len = queue->ring[k].len;
			queue->ring[k].addr = 0;
			queue->next2complete++;
			queue->stat_completes++;
			if (queue->next2complete >=adapter->tx_ring_size){
				queue->next2complete = 0;
			}
			ret++;
		}else{
			break;
		}
	}
	return ret;
}
int vmxnet3_jdriver::refill_recv_queue() {
	unsigned long pkt;
	int i,count=0;

	while (count < MAX_BUF_LIST_SIZE) {
		i = adapter->rx_queues[0].next2refill;
		if (adapter->rx_queues[0].ring1[i].addr != 0){
			break;
		}

		pkt = (unsigned char *) jalloc_page(MEM_NETBUF);
		adapter->rx_queues[0].ring1[i].addr = __pa(pkt+10); /* TODO: 10 bytes header is added to make consistent with vitio */
		adapter->rx_queues[0].ring1[i].len = PAGE_SIZE;
		adapter->rx_queues[0].ring1[i].gen = adapter->rx_queues[0].gen & (0x1);
		adapter->rx_queues[0].next2refill++;
		adapter->rx_queues[0].stat_refills++;
		if (adapter->rx_queues[0].next2refill == adapter->rx_ring_size) {
			adapter->rx_queues[0].next2refill = 0;
			adapter->rx_queues[0].gen++;
		}
		count++;
	}
	return 1;
}
int vmxnet3_jdriver::BulkRemoveFromRecvQueue(struct struct_mbuf *mbuf_list, int list_len) {
	int i,k;
	int ret=0;

	for (i=0; i < list_len; i++) {
		k=adapter->rx_queues[0].next2complete;
		if (adapter->rx_queues[0].comp_ring[k].gen == adapter->rx_queues[0].ring1[k].gen && adapter->rx_queues[0].ring1[k].addr!=0 ){
			mbuf_list[i].buf = __va(adapter->rx_queues[0].ring1[k].addr-10);
			mbuf_list[i].len = adapter->rx_queues[0].comp_ring[k].len+10;
			adapter->rx_queues[0].ring1[k].addr = 0;
			adapter->rx_queues[0].next2complete++;
			adapter->rx_queues[0].stat_completes++;
			if (adapter->rx_queues[0].next2complete >=adapter->rx_ring_size){
				adapter->rx_queues[0].next2complete = 0;
			}
			ret++;
		}else{
			break;
		}
	}
	return ret;
}
int vmxnet3_jdriver::burst_recv(int total_pkts) {
	unsigned int list_len = MAX_BUF_LIST_SIZE;
	int i;
	int recv_pkts = 0;

	if (total_pkts < list_len) {
		list_len = total_pkts;
	}

	recv_pkts = BulkRemoveFromRecvQueue(&recv_mbuf_list[0], list_len);
	if (recv_pkts <= 0) {
		return 0;
	}

	refill_recv_queue();

	for (i = 0; i < recv_pkts; i++) {
		netif_rx(recv_mbuf_list[i].buf, recv_mbuf_list[i].len);
		recv_mbuf_list[i].buf = 0;
	}
	return recv_pkts;
}
extern "C" {
int Bulk_free_pages(struct struct_mbuf *mbufs, int list_len);
}
int vmxnet3_jdriver::burst_send() {
	int qno, i, ret = 0;
	int qret;
	unsigned long flags;
	int pkts;

	if (send_mbuf_len == 0) {
		return ret;
	}

	/* remove the used buffers from the previous send cycle */
	for (qno = 0; qno < adapter->num_tx_queues; qno++) {
		pkts = detach_usedbuf_from_sendqueue(qno,&temp_mbuf_list[0], MAX_BUF_LIST_SIZE);
		if (pkts > 0) {
			Bulk_free_pages(temp_mbuf_list, pkts);
		}
	}

	ret = 0;
	for (qno = 0; qno< adapter->num_tx_queues; qno++) { /* try to send from the same queue , if it full then try on the subsequent one, in this way kicks will be less */
		qret = BulkAddToSendQueue(qno,&send_mbuf_list[send_mbuf_start + ret], send_mbuf_len - ret);
		if (qret == 0) {
			continue;
		}
		ret = ret + qret;
		if (ret == send_mbuf_len) {
			break;
		}
	}

	if (ret <= 0) {
		ret = 0;
	}else{ /* notify the backend */
		adapter->tx_qdesc->ctrl.txNumDeferred =0;
		bar0_struct->tx_notify = 0;
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
	return ret; /* Here Success indicates the buffer is freed or consumed */
}

int vmxnet3_jdriver::read(unsigned char *buf, int len, int rd_flags, int opt_flags) {
	return 0;
}
int vmxnet3_jdriver::write(unsigned char *data, int len, int wr_flags) {
	BUG();
	return 0;
}
int vmxnet3_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	return 0;
}


int vmxnet3_jdriver::setup_shared(){
	struct Vmxnet3_DriverShared *shared;
	unsigned long shared_pa;
	unsigned long addr;
	int qdescr_size;

	if (adapter != 0){
		return JFAIL;
	}
	adapter = ut_calloc(sizeof(struct Vmxnet3_Adapter));
	adapter->num_tx_queues = 1;
	adapter->num_rx_queues = 1;
	adapter->tx_ring_size = VMXNET3_DEF_TX_RING_SIZE;
	adapter->rx_ring_size = VMXNET3_DEF_RX_RING_SIZE;
//	adapter->rx_ring2_size = VMXNET3_DEF_RX_RING2_SIZE;
	qdescr_size = sizeof(struct Vmxnet3_TxQueueDesc) * adapter->num_tx_queues;
	qdescr_size += sizeof(struct Vmxnet3_RxQueueDesc) * adapter->num_rx_queues;

	adapter->tx_qdesc = ut_calloc(qdescr_size);
	addr = adapter->tx_qdesc;
	addr = addr + (sizeof(struct Vmxnet3_TxQueueDesc) * adapter->num_tx_queues);
    adapter->rx_qdesc = addr;

    adapter->tx_queues[0].ring = ut_calloc( adapter->tx_ring_size * sizeof(struct Vmxnet3_TxDesc));
    adapter->tx_queues[0].data_ring = ut_calloc( adapter->tx_ring_size * sizeof(struct Vmxnet3_TxDataDesc));
    adapter->tx_queues[0].comp_ring = ut_calloc( adapter->tx_ring_size * sizeof(struct Vmxnet3_TxCompDesc));
    adapter->tx_queues[0].gen = 1;

    adapter->rx_queues[0].ring1 = ut_calloc( adapter->rx_ring_size * sizeof(struct Vmxnet3_RxDesc));
    adapter->rx_queues[0].ring2 = ut_calloc( adapter->rx_ring_size * sizeof(struct Vmxnet3_RxDesc));
    adapter->rx_queues[0].comp_ring = ut_calloc( adapter->rx_ring_size * sizeof(struct Vmxnet3_RxCompDesc));
    adapter->rx_queues[0].gen = 1;

    adapter->tx_qdesc->conf.txRingBasePA = __pa(adapter->tx_queues[0].ring);
    adapter->tx_qdesc->conf.compRingBasePA = __pa(adapter->tx_queues[0].comp_ring);
    adapter->tx_qdesc->conf.dataRingBasePA = __pa(adapter->tx_queues[0].data_ring);
    adapter->tx_qdesc->conf.compRingSize = adapter->tx_ring_size;
    adapter->tx_qdesc->conf.dataRingSize = adapter->tx_ring_size;
    adapter->tx_qdesc->conf.txRingSize = adapter->tx_ring_size;

    adapter->rx_qdesc->conf.rxRingBasePA[0] = __pa(adapter->rx_queues[0].ring1);
    adapter->rx_qdesc->conf.rxRingBasePA[1] = __pa(adapter->rx_queues[0].ring2);
    adapter->rx_qdesc->conf.compRingBasePA = __pa(adapter->rx_queues[0].comp_ring);
    adapter->rx_qdesc->conf.rxRingSize[0] = adapter->rx_ring_size;
    adapter->rx_qdesc->conf.rxRingSize[1] = adapter->rx_ring_size;
    adapter->rx_qdesc->conf.compRingSize = adapter->rx_ring_size;

	shared_addr = ut_calloc(sizeof(struct Vmxnet3_DriverShared));
	shared_pa = __pa(shared_addr);
	shared = shared_addr;
	shared->devRead.rxFilterConf.rxMode =  VMXNET3_RXM_UCAST | VMXNET3_RXM_BCAST | VMXNET3_RXM_PROMISC;
	shared->magic = VMXNET3_REV1_MAGIC;

	//shared->devRead.misc.driverInfo.version =  VMXNET3_DRIVER_VERSION_NUM;

	shared->devRead.misc.numRxQueues = adapter->num_rx_queues;
	shared->devRead.misc.numTxQueues = adapter->num_tx_queues;
	shared->devRead.misc.queueDescPA = __pa(adapter->tx_qdesc);
	shared->devRead.misc.queueDescLen =  qdescr_size;
	bar1_struct->shared_addr_low[0] = shared_pa & 0xffffffff;
	bar1_struct->shared_addr_high[0] = (shared_pa>>32) & 0xffffffff;
	ut_log(" shared pa address: %x \n",shared_pa);

	bar1_struct->command =  VMXNET3_CMD_ACTIVATE_DEV;
	bar0_struct->tx_notify = 0; /* TODO :  mmio page table entries need to created in kernel thread only, otherwise page table are not created properly */
	refill_recv_queue();
	return JSUCCESS;
}
int vmxnet3_jdriver::net_attach_device() {
	pci_dev_header_t *pci_hdr = &device->pci_device.pci_header;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;

	INIT_LOG(" vmxnet3: Bar0:%x  Bar1:%x \n",device->pci_device.pci_bars[0].addr,device->pci_device.pci_bars[1].addr);
	bar0_struct = vm_create_kmap("vmxnet2_bar0",0x1000,PROT_READ|PROT_WRITE,MAP_FIXED,device->pci_device.pci_bars[0].addr);
	bar1_struct = vm_create_kmap("vmxnet2_bar1",0x1000,PROT_READ|PROT_WRITE,MAP_FIXED,device->pci_device.pci_bars[1].addr);
	INIT_LOG("vmxnet3  version:%x: upt_version:%x mac :%x:%x:%x:%x \n",bar1_struct->version[0],bar1_struct->upt_version[0], bar1_struct->mac[0],bar1_struct->mac[1],bar1_struct->mac[2],bar1_struct->mac[3]);
#if 0
	pci_enable_msix(device->pci_device.pci_bars[2].addr,
			&device->pci_device.msix_cfg,
			device->pci_device.pci_header.capabilities_pointer);
#endif

	setup_shared();

	return 0;
}
int vmxnet3_jdriver::dettach_device(jdevice *jdev) {
	return JFAIL;
}
extern int register_netdevice(jdevice *device);
vmxnet3_jdriver *vmxnet3_debug; /* TODO: remove later */
jdriver *vmxnet3_jdriver::attach_device(class jdevice *jdev) {
	COPY_OBJ(vmxnet3_jdriver, this, new_obj, jdev);
	((vmxnet3_jdriver *) new_obj)->net_attach_device();
	jdev->driver = new_obj;
	register_netdevice(jdev);
	vmxnet3_debug = new_obj;  /* TODO : remove later */
	return (jdriver *) new_obj;
}
extern "C" {

static vmxnet3_jdriver *net_vmxnet3_jdriver;
void init_vmxnet3(){
	net_vmxnet3_jdriver = jnew_obj(vmxnet3_jdriver);
	net_vmxnet3_jdriver->name = (unsigned char *) "net_vmxnet3_driver";
	register_jdriver(net_vmxnet3_jdriver);
}
}
int vmxnet3_jdriver::probe_device(class jdevice *jdev) {
	if ((jdev->pci_device.pci_header.vendor_id == PCI_VENDOR_ID_VMWARE) && (jdev->pci_device.pci_header.device_id
					== PCI_DEVICE_ID_VMWARE_VMXNET3)) {
		ut_log(" Matches inside the vmxnet3 NETPROBE.... \n");
		return JSUCCESS;
	}
	return JFAIL;
}
