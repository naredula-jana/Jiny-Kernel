#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include <virtio.h>
#include <virtio_ring.h>
#include <virtio_pci.h>
#include <virtio_net.h>
#include "9p.h"

extern virtio_dev_t virtio_devices[];
extern int virtio_dev_count;
static struct wait_struct p9_waitq;

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

int init_virtio_9p_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev) {
	unsigned long addr;
	unsigned long features;

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("Initializing VIRTIO PCI p9 status :%x :  \n",vp_get_status(dev));

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG(" driver Initialising VIRTIO PCI 9P hostfeatures :%x:\n",features);

	virtio_createQueue(0, dev, 2);

	vp_set_status(dev, vp_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG(" NEW Initialising..9P INPUT  VIRTIO PCI COMPLETED with driver ok :%x \n",vp_get_status(dev));
	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);

	sc_register_waitqueue(&p9_waitq);
}
unsigned long p9_write_rpc(p9_client_t *client, const char *fmt, ...) { /* The call will be blocked till the reply is receivied */
	struct p9_fcall pdu;
	int ret;
	unsigned long addr;
	va_list ap;
	va_start(ap,fmt);

	p9pdu_init(&pdu, client->type, client->tag, client->pkt_buf, client->pkt_len);
	ret = p9pdu_write(&pdu, fmt, ap);
	va_end(ap);
	p9pdu_finalize(&pdu);

	struct scatterlist sg[4];
	unsigned int out, in;
	sg[0].page_link = pdu.sdata;
	sg[0].length = 1024;
	sg[0].offset = 0;
	out = 1;
	if (client->type == P9_TYPE_TREAD) {
		sg[1].page_link = pdu.sdata + 1024;
		sg[1].length = 11; /* exactly 11 bytes for read response header , data will be from user buffer*/
		sg[1].offset = 0;
		sg[2].page_link = client->user_data;
		sg[2].length = client->userdata_len;
		sg[2].offset = 0;
		in = 2;
	} else {
		sg[1].page_link = pdu.sdata + 1024;
		sg[1].length = 1024;
		sg[1].offset = 0;
		in = 1;
	}
	virtqueue_add_buf_gfp(virtio_devices[0].vq[0], sg, out, in,
			sg[0].page_link, 0);
	virtqueue_kick(virtio_devices[0].vq[0]);

	sc_wait(&p9_waitq, 100);
	unsigned int len;
	len = 0;
	addr = virtio_removeFromQueue(virtio_devices[0].vq[0], &len);
	if (addr == 0) {
		DEBUG("9p cmd EMPTY\n");
		return 0;
	}
	client->addr = addr;
	return addr;
}

int p9_read_rpc(p9_client_t *client, const char *fmt, ...) {
	unsigned char *recv;
	struct p9_fcall pdu;
	int ret;
	uint32_t total_len;
	unsigned char type;
	uint16_t tag;
	unsigned long addr = client->addr;

	va_list ap;
	va_start(ap,fmt);

	recv = addr + 1024;
	p9pdu_init(&pdu, 0, 0, recv, 1024);
	ret = p9pdu_read_v(&pdu, "dbw", &total_len, &type, &tag);
	client->recv_type = type;
	ret = p9pdu_read(&pdu, fmt, ap);
	va_end(ap);
	DEBUG("Recv Header ret:%x total len :%x stype:%x rtype:%x tag:%x \n",ret ,total_len,client->type,type,tag);
	DEBUG(" : c:%x:%x:%x:%x :%x:%x:%x:%x data:%x:%x:%x:%x\n", recv[0], recv[1], recv[2], recv[3], recv[4], recv[5], recv[6], recv[7],recv[12], recv[13], recv[14], recv[15]);
	return ret;
}

static p9_client_t client;
int p9_clientInit() {
static int init=0;
uint32_t msg_size;
int ret;
unsigned char version[200];
unsigned long addr;
	if (init != 0) return 1;

	client.pkt_buf = (unsigned char *)mm_getFreePages(MEM_CLEAR, 0);
	client.pkt_len = 4098;
    init=1;

	client.type = P9_TYPE_TVERSION;
	client.tag = 0xffff;

	addr = p9_write_rpc(&client, "ds", 0x2040, "9P2000.u");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "ds", &msg_size, version);
	}
	DEBUG("New cmd:%x size:%x version:%s \n",ret, msg_size,version);

	client.type = P9_TYPE_TATTACH;
	client.tag = 0x13;
	client.root_fid = 132;
	addr = p9_write_rpc(&client, "ddss", client.root_fid, ~0, "jana", "");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
	}
	return 1;
}

static unsigned char data[100];
int p9_cmd(char *arg1, char *arg2) {
	unsigned long addr;
	uint32_t msg_size;
	int ret;

	p9_clientInit();

	client.type = P9_TYPE_TWALK;
	addr = p9_write_rpc(&client, "ddws", client.root_fid, 201, 1, "testabc");
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
	}

	client.type = P9_TYPE_TOPEN;
	addr = p9_write_rpc(&client, "db", 201, 0);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "");
	}

	client.type = P9_TYPE_TREAD;
	client.user_data = data;
	client.userdata_len = 100;
	unsigned long offset = 0;
	uint32_t read_len;
	addr = p9_write_rpc(&client, "dqd", 201, offset, 200);
	if (addr != 0) {
		ret = p9_read_rpc(&client, "d", &read_len);
	}
	data[10] = 0;
	DEBUG("read len :%d  new DATA  :%s:\n",read_len,data);

	return 1;
}
void virtio_9p_interrupt(registers_t regs) {
	unsigned char isr;
	int ret;

	isr = inb(virtio_devices[0].pci_ioaddr + VIRTIO_PCI_ISR);
	ret = sc_wakeUp(&p9_waitq, NULL); /* wake all the waiting processes */
}
