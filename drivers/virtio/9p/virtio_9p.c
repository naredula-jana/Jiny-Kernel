//#define DEBUG_ENABLE 1
#include "common.h"
#include "device.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "../virtio.h"
#include "../virtio_ring.h"
#include "../virtio_pci.h"
#include "9p.h"
#include "mach_dep.h"

static wait_queue_t p9_waitq;
virtio_dev_t *p9_dev = 0;
static int stat_request=0;
static int stat_intr=0;
static void virtio_9p_interrupt(registers_t regs);
static int virtio_addToP9Queue(struct virtqueue *vq, unsigned long buf,
		unsigned long out_len, unsigned long in_len);

static unsigned char vp_get_status(virtio_dev_t *dev) {
	uint16_t addr = dev->pci_ioaddr + VIRTIO_PCI_STATUS;
	return inb(addr);
}
static void vp_set_status(virtio_dev_t *dev, unsigned char status) {
	uint16_t addr = dev->pci_ioaddr + VIRTIO_PCI_STATUS;
	outb(addr, status);
}
static int probe_virtio_9p_pci(device_t *dev) {
	if (dev->pci_hdr.device_id == VIRTIO_PCI_9P_DEVICE_ID) {
		return 1;
	}
	return 0;
}

static int attach_virtio_9p_pci(device_t *pci_dev) {
//	pci_dev_header_t *pci_hdr = &pci_dev->pci_hdr;
	virtio_dev_t *virtio_dev = (virtio_dev_t *) pci_dev->private_data;
	uint32_t msi_vector = pci_dev->msix_cfg.isr_vector;

	unsigned long addr;
	unsigned long features;

	vp_set_status(virtio_dev,
			vp_get_status(virtio_dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO PCI p9 status :%x :  \n", vp_get_status(virtio_dev));

	vp_set_status(virtio_dev,
			vp_get_status(virtio_dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = virtio_dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initializing VIRTIO PCI 9P hostfeatures :%x:\n", features);

	virtio_createQueue(0, virtio_dev, 2);
	if (msi_vector > 0) {
#if 0
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0);
		outw(virtio_dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR,0xffff);
		ar_registerInterrupt(msi_vector, virtio_9p_interrupt, "virtio_p9_msi");
#endif
	}
	virtio_dev->isr = virtio_9p_interrupt;
	vp_set_status(virtio_dev,
			vp_get_status(virtio_dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initializing.9P INPUT  VIRTIO PCI COMPLETED with driver ok :%x \n", vp_get_status(virtio_dev));
	inb(virtio_dev->pci_ioaddr + VIRTIO_PCI_ISR);

	ipc_register_waitqueue(&p9_waitq, "waitq_p9");
	p9_dev = virtio_dev;
	p9_initFs();
	return 1;
}
unsigned long p9_write_rpc(p9_client_t *client, const char *fmt, ...) { /* The call will be blocked till the reply is receivied */
	p9_fcall_t pdu;
	int ret, i;
	unsigned long addr;
	va_list ap;
	va_start(ap, fmt);

	p9_pdu_init(&pdu, client->type, client->tag, client, client->pkt_buf,
			client->pkt_len);
	stat_request++;
	ret = p9_pdu_write(&pdu, fmt, ap);
	va_end(ap);
	p9_pdu_finalize(&pdu);

	struct scatterlist sg[4];
	unsigned int out, in;
	sg[0].page_link = (unsigned long)client->pkt_buf;
	sg[0].length = 1024;
	//sg[0].length = client->pkt_len/2;
	sg[0].offset = 0;
	out = 1;
	if (client->type == P9_TYPE_TREAD) {
		sg[1].page_link = client->pkt_buf + 1024;
		sg[1].length = 11; /* exactly 11 bytes for read response header , data will be from user buffer*/
		sg[1].offset = 0;
		sg[2].page_link = client->user_data;
		sg[2].length = client->userdata_len;
		sg[2].offset = 0;
		in = 2;
	} else if (client->type == P9_TYPE_TWRITE) {
		sg[1].page_link = (unsigned long)client->user_data;
		sg[1].length = client->userdata_len;
		sg[1].offset = 0;
		sg[0].length = 23; /* this for header , eventhough it is having space pick the data from sg[1] */

		sg[2].page_link = client->pkt_buf + 1024;
		sg[2].length = client->pkt_len-1024;
		sg[2].offset = 0;
		out = 2;
		in = 1;
	} else {
		sg[1].page_link = client->pkt_buf + 1024;
		sg[1].length = client->pkt_len-1024;
		sg[1].offset = 0;
		in = 1;
	}
	virtio_enable_cb(p9_dev->vq[0]);
	virtio_add_buf_to_queue(p9_dev->vq[0], sg, out, in, sg[0].page_link, 0);
	virtio_queue_kick(p9_dev->vq[0]);

	ipc_waiton_waitqueue(&p9_waitq, 50);
	unsigned int len;
	len = 0;
	i = 0;
	addr = 0;
	while (i < 30 && addr == 0) {
		addr = virtio_removeFromQueue(p9_dev->vq[0], &len); /* TODO : here sometime returns zero because of some race condition, the packet is not recevied */
		i++;
		if (addr == 0) {
			ut_log("sleep in P9 so sleeping for while requests:%d intr:%d\n",stat_request,stat_intr);
			//sc_sleep(300);
			ipc_waiton_waitqueue(&p9_waitq, 30);
		}
	}
	if (addr != (unsigned long)client->pkt_buf) {
		DEBUG("9p write : got invalid address : %x \n", addr);
		return 0;
	}
	return client->pkt_buf;
}

int p9_read_rpc(p9_client_t *client, const char *fmt, ...) {
	unsigned char *recv;
	p9_fcall_t pdu;
	int ret;
	uint32_t total_len;
	unsigned char type;
	uint16_t tag;

	va_list ap;
	va_start(ap, fmt);

	recv = client->pkt_buf + 1024;
	p9_pdu_init(&pdu, 0, 0, client, recv, 1024);
	ret = p9_pdu_read_v(&pdu, "dbw", &total_len, &type, &tag);
	client->recv_type = type;
	ret = p9_pdu_read(&pdu, fmt, ap);
	va_end(ap);
	//ut_log("Recv Header ret:%x:%d total len :%x stype:%x(%d) rtype:%x(%d) tag:%x \n", ret,ret, total_len, client->type, client->type, type, type, tag);
	if (type == 107) { // TODO better way of handling other and this error
		recv[100] = '\0';
		DEBUG(" recv error data :%s: \n ", &recv[9]);
	}
	return ret;
}
static int dettach_virtio_9p_pci(device_t *pci_dev) {
	return 0;
}
static void virtio_9p_interrupt(registers_t regs) { // TODO: handling similar  type of interrupt generating while serving P9 interrupt.
	unsigned char isr;
	int ret;

	if (p9_dev->msi == 0)
		isr = inb(p9_dev->pci_ioaddr + VIRTIO_PCI_ISR);
	stat_intr++;
	ret = ipc_wakeup_waitqueue(&p9_waitq); /* wake all the waiting processes */
	if (ret==0){
		//ut_log("ERROR: p9 wait No one is waiting...\n");
		ut_log("ERROR:New  p9 wait No one is waiting requests:%d intr:%d\n",stat_request,stat_intr);
	}
}
DEFINE_DRIVER(virtio_9p_pci, virtio_pci, probe_virtio_9p_pci, attach_virtio_9p_pci, dettach_virtio_9p_pci);
