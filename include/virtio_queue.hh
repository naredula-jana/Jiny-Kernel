/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   include/virtio_queue.hh
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#ifndef __JVIRTIOQUEUE_H__
#define __JVIRTIOQUEUE_H__
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
#include "mach_dep.h"
}



/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
/* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER      2
/* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_DRIVER_OK   4
/* We've given up on this device. */
#define VIRTIO_CONFIG_S_FAILED      0x80
/* Some virtio feature bits (currently bits 28 through 31) are reserved for the
 * transport being used (eg. virtio_ring), the rest are per-device feature
 * bits. */
#define VIRTIO_TRANSPORT_F_START    28
#define VIRTIO_TRANSPORT_F_END      32

/* Do we get callbacks when the ring is completely used, even if we've
 * suppressed them? */
#define VIRTIO_F_NOTIFY_ON_EMPTY    24
/* A 32-bit r/o bitmask of the features supported by the host */
#define VIRTIO_PCI_HOST_FEATURES	0
/* A 32-bit r/w bitmask of features activated by the guest */
#define VIRTIO_PCI_GUEST_FEATURES	4
/* A 32-bit r/w PFN for the currently selected queue */
#define VIRTIO_PCI_QUEUE_PFN		8
/* A 16-bit r/o queue size for the currently selected queue */
#define VIRTIO_PCI_QUEUE_NUM		12
/* A 16-bit r/w queue selector */
#define VIRTIO_PCI_QUEUE_SEL		14
/* A 16-bit r/w queue notifier */
#define VIRTIO_PCI_QUEUE_NOTIFY		16
/* An 8-bit device status register.  */
#define VIRTIO_PCI_STATUS		18

/* An 8-bit r/o interrupt status register.  Reading the value will return the
 * current contents of the ISR and will also clear it.  This is effectively
 * a read-and-acknowledge. */
#define VIRTIO_PCI_ISR			19
/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_PCI_ISR_CONFIG		0x2

/* MSI-X registers: only enabled if MSI-X is enabled. */
/* A 16-bit vector for configuration changes. */
#define VIRTIO_MSI_CONFIG_VECTOR        20
/* A 16-bit vector for selected queue notifications. */
#define VIRTIO_MSI_QUEUE_VECTOR         22
/* Vector value used to disable MSI for queue */
#define VIRTIO_MSI_NO_VECTOR            0xffff

/* The remaining space is defined by each driver as the per-driver
 * configuration space */
#define VIRTIO_PCI_CONFIG(dev)		((dev)->msix_enabled ? 24 : 20)

/* Virtio ABI version, this must match exactly */
#define VIRTIO_PCI_ABI_VERSION		0

/* How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size. */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT	12

/* The alignment to use between consumer and producer parts of vring.
 * x86 pagesize again. */
#define VIRTIO_PCI_VRING_ALIGN		4096


/* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_NEXT	1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE	2
/* This means the buffer contains a list of buffer descriptors. */
#define VRING_DESC_F_INDIRECT	4
/* The Host uses this in used->flags to advise the Guest: don't kick me when
 * you add a buffer.  It's unreliable, so it's simply an optimization.  Guest
 * will still kick if it's out of buffers. */
#define VRING_USED_F_NO_NOTIFY	1
/* The Guest uses this in avail->flags to advise the Host: don't interrupt me
 * when you consume a buffer.  It's unreliable, so it's simply an
 * optimization.  */
#define VRING_AVAIL_F_NO_INTERRUPT	1
/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC	28
/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field. */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field. */
#define VIRTIO_RING_F_EVENT_IDX		29

#define vring_used_event(vr) ((vr)->avail->ring[(vr)->num])
#define vring_avail_event(vr) (*(__u16 *)&(vr)->used->ring[(vr)->num])



/* Virtio ring descriptors: 16 bytes.  These can chain together via "next". */
struct mem_vring_desc {
	/* Address (guest-physical). */
	uint64_t addr;
	/* Length. */
	uint32_t len;
	/* The flags as indicated above. */
	uint16_t flags;
	/* We chain unused descriptors via this, too */
	uint16_t next;
};

struct mem_vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];
};

/* u32 is used here for ids for padding reasons. */
struct mem_vring_used_elem {
	/* Index of start of used descriptor chain. */
	uint32_t id;
	/* Total length of the descriptor chain which was used (written to) */
	uint32_t len;
};

struct mem_vring_used {
	uint16_t flags;
	uint16_t idx;
	struct mem_vring_used_elem ring[];
};

struct mem_vring {
	unsigned int num;
	struct mem_vring_desc *desc;
	struct mem_vring_avail *avail;
	struct mem_vring_used *used;
};

struct vring_queue{
	const char *name;
	int queue_number;
	unsigned long pci_ioaddr;
	/* Actual memory layout for this queue */
	struct mem_vring vring;
	/* Other side has made a mess, don't try any more. */
	bool broken;
	/* Host supports indirect buffers */
	bool indirect;
	/* Host publishes avail event idx */
	bool event;

	/* Number of free buffers */
	unsigned int num_free;
	/* Head of free buffer list. */
	unsigned int free_head;
	unsigned int free_tail;

	/* Number we've added since last sync. */
	unsigned int num_added;
	/* Last used index we've seen. */
	uint16_t last_used_idx;
	/* They're supposed to lock for us. */
	unsigned long in_use;

	unsigned int stat_alloc,stat_free;
};


/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0	/* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1	/* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GSO	6	/* Host handles pkts w/ any GSO type */
#define VIRTIO_NET_F_GUEST_TSO4	7	/* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6	8	/* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN	9	/* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO	10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6	12	/* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN	13	/* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO	14	/* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	16	/* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ	17	/* Control channel available */
#define VIRTIO_NET_F_CTRL_RX	18	/* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN	19	/* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20	/* Extra RX mode control support */
#define VIRTIO_NET_F_MQ 22	/* Multi queue support */

#define VIRTIO_ID_NET		1 /* virtio net */
#define VIRTIO_ID_BLOCK		2 /* virtio block */
#define VIRTIO_ID_CONSOLE	3 /* virtio console */
#define VIRTIO_ID_RNG		4 /* virtio ring */
#define VIRTIO_ID_BALLOON	5 /* virtio balloon */
#define VIRTIO_ID_9P		9 /* 9p virtio console */

/* This is the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header. */
struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1	// Use csum_start, csum_offset
#define VIRTIO_NET_HDR_F_DATA_VALID	2	// Csum is valid
	uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE		0	// Not a GSO frame
#define VIRTIO_NET_HDR_GSO_TCPV4	1	// GSO frame, IPv4 TCP (TSO)
#define VIRTIO_NET_HDR_GSO_UDP		3	// GSO frame, IPv4 UDP (UFO)
#define VIRTIO_NET_HDR_GSO_TCPV6	4	// GSO frame, IPv6 TCP
#define VIRTIO_NET_HDR_GSO_ECN		0x80	// TCP has ECN set
	uint8_t gso_type;
	uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
	uint16_t csum_start;	/* Position to start checksumming from */
	uint16_t csum_offset;	/* Offset after that to place checksum */
};
/*
 * Control virtqueue data structures
 *
 * The control virtqueue expects a header in the first sg entry
 * and an ack/status response in the last entry.  Data for the
 * command goes in between.
 */
struct virtio_net_ctrl_hdr {
	uint8_t var_class;
	uint8_t cmd;
} __attribute__((packed));

struct scatterlist {
        unsigned long   page_link;
        unsigned int    offset;
        unsigned int    length;
        int      dma_address;

};

struct scatter_buf {
		uint64_t addr;
        unsigned int    offset;
        uint32_t    length;
        int read_only;
};
class virtio_queue: public jobject {

public:
	bool virtqueue_enable_cb_delayed();
	void sync_avial_idx(struct vring_queue *vq);
	void detach_buf( unsigned int head);
	void notify();
	void init_virtqueue(unsigned int num,  unsigned int vring_align,  unsigned long pci_ioaddr, void *pages,
					      void (*callback)(struct virtio_queue *), const char *name, int queue_number);

	struct vring_queue *queue;
	unsigned long stat_add_success,stat_add_fails,stat_add_pkts;
	unsigned long stat_rem_success,stat_rem_fails,stat_rem_pkts;
	unsigned long stat_error_empty_bufs;
	atomic_t stat_kicks;
	int qType;
	int virtio_type;
	int scatter_list_size;

	void print_stats(unsigned char *arg1,unsigned char *arg2);
	int check_recv_pkt();
	int virtio_disable_cb();
	bool virtio_enable_cb();
	int virtio_queuekick();
	virtio_queue(jdevice *device, uint16_t index, int type);
	int virtio_add_buf_to_queue(struct scatterlist sg[], unsigned int out,
				  unsigned int in, void *data, int gfp);
	void *virtio_queue::virtio_removeFromQueue(unsigned int *len);
	int BulkAddToQueue(struct struct_mbuf *mbuf_list, int list_len, int read_only);
	int BulkRemoveFromQueue(struct struct_mbuf *mbuf_list, int list_len);
	int MaxBufsSpace();  /* numbers of that can be added */
};

class net_virtio_queue: public virtio_queue {

public:
	net_virtio_queue(jdevice *device, uint16_t index, int queue_type):
			virtio_queue(device, index, queue_type){
		virtio_type = VIRTIO_ID_NET;

		scatter_list_size = 2;
	}
};


#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_SCSI_CMD 2
#define VIRTIO_BLK_T_SCSI_CMD_OUT 3
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_FLUSH_OUT 5
#define VIRTIO_BLK_T_BARRIER 0x80000000
#define VIRTIO_BLK_DATA_SIZE (4096)
struct virtio_blk_req {
	uint32_t type;
	uint32_t ioprio;
	uint64_t sector;

	uint8_t status;
	uint8_t pad[3];
	uint32_t len;

	char *user_data; /* this memory block can be used directly to avoid the mem copy, this is if it from pagecache */

	char data[2];  /* here data can be one byte or  1 page depending on the user_data */
};
class disk_virtio_queue: public virtio_queue {

public:
	disk_virtio_queue(jdevice *device, uint16_t index, int queue_type):
			virtio_queue(device, index, queue_type){
		virtio_type = VIRTIO_ID_BLOCK;
		scatter_list_size = 3;
	}

};


#endif
