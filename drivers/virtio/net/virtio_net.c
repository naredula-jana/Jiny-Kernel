#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "../virtio.h"
#include "../virtio_ring.h"
#include "../virtio_pci.h"
#include "virtio_net.h"

int test_virtio_nob = 0;
static virtio_dev_t *net_dev = 0;
void virtio_net_interrupt(registers_t regs);
static struct virtio_feature_desc vtnet_feature_desc[] = {
    { VIRTIO_NET_F_CSUM,        "TxChecksum"    },
    { VIRTIO_NET_F_GUEST_CSUM,  "RxChecksum"    },
    { VIRTIO_NET_F_MAC,     "MacAddress"    },
    { VIRTIO_NET_F_GSO,     "TxAllGSO"  },
    { VIRTIO_NET_F_GUEST_TSO4,  "RxTSOv4"   },
    { VIRTIO_NET_F_GUEST_TSO6,  "RxTSOv6"   },
    { VIRTIO_NET_F_GUEST_ECN,   "RxECN"     },
    { VIRTIO_NET_F_GUEST_UFO,   "RxUFO"     },
    { VIRTIO_NET_F_HOST_TSO4,   "TxTSOv4"   },
    { VIRTIO_NET_F_HOST_TSO6,   "TxTSOv6"   },
    { VIRTIO_NET_F_HOST_ECN,    "TxTSOECN"  },
    { VIRTIO_NET_F_HOST_UFO,    "TxUFO"     },
    { VIRTIO_NET_F_MRG_RXBUF,   "MrgRxBuf"  },
    { VIRTIO_NET_F_STATUS,      "Status"    },
    { VIRTIO_NET_F_CTRL_VQ,     "ControlVq" },
    { VIRTIO_NET_F_CTRL_RX,     "RxMode"    },
    { VIRTIO_NET_F_CTRL_VLAN,   "VLanFilter"    },
    { VIRTIO_NET_F_CTRL_RX_EXTRA,   "RxModeExtra"   },
    { 0, NULL }
};

void print_virtio_net() {
	print_vq(net_dev->vq[0]);
	print_vq(net_dev->vq[1]);
}
int addBufToQueue(struct virtqueue *vq, unsigned char *buf, unsigned long len) {
	struct scatterlist sg[2];
	int ret;

	if (buf == 0) {
		buf = mm_getFreePages(0, 0);
		ut_memset(buf, 0, sizeof(struct virtio_net_hdr));
#if 1
		if (test_virtio_nob == 1) { /* TODO: this is introduced to burn some cpu cycles, otherwise throughput drops drastically  from 1.6G to 500M , with vhost this is not a issue*/
			unsigned long buf1 = mm_getFreePages(MEM_CLEAR, 0);
			mm_putFreePages(buf1, 0);
		}
#endif
		len = 4096; /* page size */
	}
	sg[0].page_link = buf;
	sg[0].length = sizeof(struct virtio_net_hdr);
	sg[0].offset = 0;
	sg[1].page_link = buf + sizeof(struct virtio_net_hdr);
	sg[1].length = len - sizeof(struct virtio_net_hdr);
	sg[1].offset = 0;
	//DEBUG(" scatter gather-0: %x:%x sg-1 :%x:%x \n",sg[0].page_link,__pa(sg[0].page_link),sg[1].page_link,__pa(sg[1].page_link));
	if (vq->qType == 1) {
		ret = virtqueue_add_buf_gfp(vq, sg, 0, 2, sg[0].page_link, 0);/* recv q*/
	} else {
		ret = virtqueue_add_buf_gfp(vq, sg, 2, 0, sg[0].page_link, 0);/* send q */
	}

	return ret;
}

int init_virtio_net_pci(pci_dev_header_t *pci_hdr, virtio_dev_t *dev,
		uint32_t *msi_vector) {
	unsigned long addr;
	unsigned long features;
	int i;

	net_dev = dev;

	virtio_set_status(dev,
			virtio_get_status(dev) + VIRTIO_CONFIG_S_ACKNOWLEDGE);
	DEBUG("VirtioNet: Initializing VIRTIO PCI NET status :%x :  \n", virtio_get_status(dev));

	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_DRIVER);

	addr = dev->pci_ioaddr + VIRTIO_PCI_HOST_FEATURES;
	features = inl(addr);
	DEBUG("VirtioNet:  hostfeatures :%x:\n", features);
	display_virtiofeatures(features,vtnet_feature_desc);
	if (*msi_vector == 0) {
		addr = dev->pci_ioaddr + 20;
	} else {
		addr = dev->pci_ioaddr + 24;
	}
	DEBUG("VirtioNet:  pioaddr:%x MAC address : %x :%x :%x :%x :%x :%x status: %x:%x  :\n", addr, inb(addr), inb(addr+1), inb(addr+2), inb(addr+3), inb(addr+4), inb(addr+5), inb(addr+6), inb(addr+7));
	for (i = 0; i < 6; i++)
		dev->mac[i] = inb(addr + i);
	virtio_createQueue(0, dev, 1);
	if (*msi_vector > 0) {
		outw(dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0);
	}
	virtio_createQueue(1, dev, 2);
	if (*msi_vector > 0) {
		outw(dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 1);
		outw(dev->pci_ioaddr + VIRTIO_MSI_QUEUE_VECTOR, 0xffff);
	}
	virtqueue_disable_cb(dev->vq[1]); /* disable interrupts on sending side */
	if (*msi_vector > 0) {
		for (i = 0; i < 3; i++)
			ar_registerInterrupt(*msi_vector + i, virtio_net_interrupt,
					"virt_net_msi");

	}

	virtio_set_status(dev, virtio_get_status(dev) + VIRTIO_CONFIG_S_DRIVER_OK);
	DEBUG("VirtioNet:  Initilization Completed \n");

	for (i = 0; i < 120; i++) /* add buffers to recv q */
		addBufToQueue(dev->vq[0], 0, 4096);

	inb(dev->pci_ioaddr + VIRTIO_PCI_ISR);
	virtqueue_kick(dev->vq[0]);

}

static int s_i = 0;
static int s_r = 0;
int stop_queueing_bh = 0;

extern queue_t nbh_waitq;
extern int netbh_started, netbh_flag;
void virtio_net_interrupt(registers_t regs) {
	/* reset the irq by resetting the status  */
	unsigned char isr;

	if (net_dev->msi == 0)
		isr = inb(net_dev->pci_ioaddr + VIRTIO_PCI_ISR);

	if (netbh_started == 1) {
		virtqueue_disable_cb(net_dev->vq[0]);

		sc_wakeUp(&nbh_waitq);
		netbh_flag = 1;
	}
}

int virtio_send_errors = 0;
int netfront_xmit(virtio_dev_t *dev, unsigned char* data, int len) {
	static int send_pkts = 0;
	int i, ret;
	if (dev == 0)
		return 0;

	unsigned long addr;

	addr = data;
	ret = -ENOSPC;

	ret = addBufToQueue(dev->vq[1], addr, len);
	send_pkts++;
	if (ret == -ENOSPC) {
		mm_putFreePages(addr, 0);
		virtio_send_errors++;
	}

	send_pkts = 0;
	virtqueue_kick(dev->vq[1]);
	//}
#if 1
	i = 0;
	while (i < 50) {
		i++;
		addr = virtio_removeFromQueue(dev->vq[1], &len);
		if (addr) {
			mm_putFreePages(addr, 0);
		} else {
			return 1;
		}
	}
#endif
	return 1;
}

void *init_netfront(void (*net_rx)(unsigned char* data, int len),
		unsigned char *rawmac, char **ip) { /* if succesfull return dev */
	int j;

	if (net_dev == 0)
		return 0;

	if (rawmac != 0) {
		for (j = 0; j < 6; j++)
			rawmac[j] = net_dev->mac[j];
	}

	return net_dev;
}

int shutdown_netfront(void *dev) {

}




