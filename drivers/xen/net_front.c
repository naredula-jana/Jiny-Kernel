
#include "xen.h"
#include <xen/io/netif.h>
#include <xen/io/ring.h>
/* Minimal network driver for Mini-OS.
 * Copyright (c) 2006-2007 Jacob Gorm Hansen, University of Copenhagen.
 * Based on netfront.c from Xen Linux.
 * Modified by Naredula Janardhana Reddy to suit Jiny OS
 * Does not handle fragments or extras.
 */

struct net_buffer {
	void* page;
	grant_ref_t gref;
};
#define offsetof __builtin_offsetof

#define NET_TX_RING_SIZE __CONST_RING_SIZE(netif_tx, PAGE_SIZE)
#define NET_RX_RING_SIZE __CONST_RING_SIZE(netif_rx, PAGE_SIZE)
#define GRANT_INVALID_REF 0
#define MAX_MAC_ADDR 100
struct netfront_dev {
	domid_t dom;

	unsigned short tx_freelist[NET_TX_RING_SIZE + 1];
	struct semaphore tx_sem;

	struct net_buffer rx_buffers[NET_RX_RING_SIZE];
	struct net_buffer tx_buffers[NET_TX_RING_SIZE];

	struct netif_tx_front_ring tx;
	struct netif_rx_front_ring rx;
	grant_ref_t tx_ring_ref;
	grant_ref_t rx_ring_ref;
	evtchn_port_t evtchn;

	char *nodename;
	char *backend;
	char mac[MAX_MAC_ADDR + 1];
	int init_completed;
	// xenbus_event_queue events; TODO

	void (*netif_rx)(unsigned char* data, int len);
};

__attribute__((weak)) void netif_rx(unsigned char* data, int len);

static struct netfront_dev g_netfront_dev;
static void netfront_evtchn_handler(evtchn_port_t port, struct pt_regs *regs,
		void *data) {
	int ret;
	struct netfront_dev *dev = data;

	DEBUG(" Xenbus netfront handler :%d \n", ret);
	if (dev->init_completed == 1)
		network_rx(dev);
}

static inline void add_id_to_freelist(unsigned int id, unsigned short* freelist) {
	freelist[id + 1] = freelist[0];
	freelist[0] = id;
}

void init_rx_buffers(struct netfront_dev *dev) {
	int i, requeue_idx;
	netif_rx_request_t *req;
	int notify;

	/* Rebuild the RX buffer freelist and the RX ring itself. */
	for (requeue_idx = 0, i = 0; i < NET_RX_RING_SIZE; i++) {
		struct net_buffer* buf = &dev->rx_buffers[requeue_idx];
		req = RING_GET_REQUEST(&dev->rx, requeue_idx);

		buf->gref = req->gref = gnttab_grant_access(dev->dom, __pa(buf->page)
				>> PAGE_SHIFT, 0);

		req->id = requeue_idx;

		requeue_idx++;
	}

	dev->rx.req_prod_pvt = requeue_idx;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&dev->rx, notify);

	if (notify)
		notify_remote_via_evtchn(dev->evtchn);

	dev->rx.sring->rsp_event = dev->rx.rsp_cons + 1;
}
static char nodename[256];
static char path[256], value[256];
void network_rx(struct netfront_dev *dev);
void * init_netfront( void(*thenetif_rx)(unsigned char* data,
		int len), unsigned char *rawmac, char **ip) {
	static int init_done = 0;
	int i;
	struct netif_tx_sring *txs;
	struct netif_rx_sring *rxs;
	struct netfront_dev *dev;
	uint32_t remote_domid = 0;

	if (init_done == 1) return 0;

	init_done = 1;
	dev = &g_netfront_dev;

	dev->init_completed = 0;
	dev->dom = 0; /* TODO  : later remove the hard coded backend id , read xenstore */
	ut_strcpy(nodename, "device/vif/0");

	DEBUG("NEW net TX ring size %d\n", NET_TX_RING_SIZE);
	DEBUG("net RX ring size %d\n", NET_RX_RING_SIZE);

	sys_sem_new(&dev->tx_sem, NET_TX_RING_SIZE);  /* TODO need to free the sem */
	dev->tx_sem.count=NET_TX_RING_SIZE;
	for (i = 0; i < NET_TX_RING_SIZE; i++) {
		add_id_to_freelist(i, dev->tx_freelist);
		dev->tx_buffers[i].page = NULL;
	}

	for (i = 0; i < NET_RX_RING_SIZE; i++) {
		/* TODO: that's a lot of memory */
		dev->rx_buffers[i].page = (char*) mm_getFreePages(0, 0);
	}
	txs = (struct netif_tx_sring *) alloc_page();
	rxs = (struct netif_rx_sring *) alloc_page();
	memset(txs, 0, PAGE_SIZE);
	memset(rxs, 0, PAGE_SIZE);

	SHARED_RING_INIT(txs);
	SHARED_RING_INIT(rxs);
	FRONT_RING_INIT(&dev->tx, txs, PAGE_SIZE);
	FRONT_RING_INIT(&dev->rx, rxs, PAGE_SIZE);

	dev->tx_ring_ref
			= gnttab_grant_access(dev->dom, __pa(txs) >> PAGE_SHIFT, 0);
	dev->rx_ring_ref
			= gnttab_grant_access(dev->dom, __pa(rxs) >> PAGE_SHIFT, 0);

	init_rx_buffers(dev);

	evtchn_alloc_unbound(remote_domid, netfront_evtchn_handler, dev,
			&dev->evtchn);

	ut_snprintf(path, 256, "%s/tx-ring-ref", nodename);
	ut_snprintf(value, 256, "%d", dev->tx_ring_ref);
	xen_writecmd(path, value);

	ut_snprintf(path, 256, "%s/rx-ring-ref", nodename);
	ut_snprintf(value, 256, "%d", dev->rx_ring_ref);
	xen_writecmd(path, value);

	ut_snprintf(path, 256, "%s/event-channel", nodename);
	ut_snprintf(value, 256, "%d", dev->evtchn);
	xen_writecmd(path, value);

	ut_snprintf(path, 256, "%s/request-rx-copy", nodename);
	xen_writecmd(path, "1");

	ut_snprintf(path, 256, "%s/mac", nodename);
	xen_readcmd(path, dev->mac, MAX_MAC_ADDR);
	DEBUG("mac Address :%s: \n", dev->mac);
	if (rawmac)
		sscanf(dev->mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &rawmac[0],
				&rawmac[1], &rawmac[2], &rawmac[3], &rawmac[4], &rawmac[5]);

	unmask_evtchn(dev->evtchn);
	ut_snprintf(path, 256, "%s/state", nodename);
	xen_writecmd(path, "4");/* TODO : need to check before updating state */

	dev->netif_rx = thenetif_rx;
	dev->init_completed = 1;
	DEBUG(" NET FRONT driver initialization completed  :%s\n", dev->mac);

	return dev;
}

/************************************************************/

static inline int xennet_rxidx(RING_IDX idx) {
	return idx & (NET_RX_RING_SIZE - 1);
}
static inline unsigned short get_id_from_freelist(unsigned short* freelist) {
	unsigned int id = freelist[0];
	freelist[0] = freelist[id + 1];
	return id;
}
void network_tx_buf_gc(struct netfront_dev *dev) {

	RING_IDX cons, prod;
	unsigned short id;

	do {
		prod = dev->tx.sring->rsp_prod;
		rmb();
		/* Ensure we see responses up to 'rp'. */

		for (cons = dev->tx.rsp_cons; cons != prod; cons++) {
			struct netif_tx_response *txrsp;
			struct net_buffer *buf;

			txrsp = RING_GET_RESPONSE(&dev->tx, cons);
			if (txrsp->status == NETIF_RSP_NULL)
				continue;

			if (txrsp->status == NETIF_RSP_ERROR)
				printk("packet error\n");

			id = txrsp->id;
			BUG_ON(id >= NET_TX_RING_SIZE);
			buf = &dev->tx_buffers[id];
			gnttab_end_access(buf->gref);
			buf->gref = GRANT_INVALID_REF;

			add_id_to_freelist(id, dev->tx_freelist);
			sys_sem_signal(&dev->tx_sem);
		}

		dev->tx.rsp_cons = prod;

		/*
		 * Set a new event, then check for race with update of tx_cons.
		 * Note that it is essential to schedule a callback, no matter
		 * how few tx_buffers are pending. Even if there is space in the
		 * transmit ring, higher layers may be blocked because too much
		 * data is outstanding: in such cases notification from Xen is
		 * likely to be the only kick that we'll get.
		 */
		dev->tx.sring->rsp_event = prod + ((dev->tx.sring->req_prod - prod)
				>> 1) + 1;
		mb();
	} while ((cons == prod) && (prod != dev->tx.sring->rsp_prod));

}
void netfront_xmit(struct netfront_dev *dev, unsigned char* data, int len) /* TODO */
{
	int flags;
	struct netif_tx_request *tx;
	RING_IDX i;
	int notify;
	unsigned short id;
	struct net_buffer* buf;
	void* page;

	if (len > PAGE_SIZE)
		BUG();

	sys_arch_sem_wait(&dev->tx_sem, 0);

	//local_irq_save(flags);
	id = get_id_from_freelist(dev->tx_freelist);
	//local_irq_restore(flags);

	buf = &dev->tx_buffers[id];
	page = buf->page;
	if (!page)
		page = buf->page = (char*) alloc_page();

	i = dev->tx.req_prod_pvt;
	tx = RING_GET_REQUEST(&dev->tx, i);

	ut_memcpy(page, data, len);

	buf->gref = tx->gref = gnttab_grant_access(dev->dom, __pa(page)
			>> PAGE_SHIFT, 1);

	tx->offset = 0;
	tx->size = len;
	tx->flags = 0;
	tx->id = id;
	dev->tx.req_prod_pvt = i + 1;

	wmb();

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&dev->tx, notify);

	if (notify)
		notify_remote_via_evtchn(dev->evtchn);

	//local_irq_save(flags);
	network_tx_buf_gc(dev);
	//local_irq_restore(flags);
}
void shutdown_netfront(struct netfront_dev *dev)/* TODO */
{
	DEBUG(" shutdown_netfront :TODO ... \n");
}
void network_rx(struct netfront_dev *dev) {
	RING_IDX rp, cons, req_prod;
	struct netif_rx_response *rx;
	int nr_consumed, some, more, i, notify;

	moretodo: rp = dev->rx.sring->rsp_prod;
	rmb();
	/* Ensure we see queued responses up to 'rp'. */
	cons = dev->rx.rsp_cons;

	for (nr_consumed = 0, some = 0; (cons != rp) && !some; nr_consumed++, cons++) {
		struct net_buffer* buf;
		unsigned char* page;
		int id;

		rx = RING_GET_RESPONSE(&dev->rx, cons);

		if (rx->flags & NETRXF_extra_info) {
			printk("+++++++++++++++++++++ we have extras!\n");
			continue;
		}

		if (rx->status == NETIF_RSP_NULL)
			continue;

		id = rx->id;
		BUG_ON(id >= NET_TX_RING_SIZE);

		buf = &dev->rx_buffers[id];
		page = (unsigned char*) buf->page;
		gnttab_end_access(buf->gref);

		if (rx->status > 0) {
			dev->netif_rx(page + rx->offset, rx->status);
		}
	}
	dev->rx.rsp_cons = cons;

	RING_FINAL_CHECK_FOR_RESPONSES(&dev->rx,more);
	if (more && !some)
		goto moretodo;

	req_prod = dev->rx.req_prod_pvt;

	for (i = 0; i < nr_consumed; i++) {
		int id = xennet_rxidx(req_prod + i);
		netif_rx_request_t *req = RING_GET_REQUEST(&dev->rx, req_prod + i);
		struct net_buffer* buf = &dev->rx_buffers[id];
		void* page = buf->page;

		/* We are sure to have free gnttab entries since they got released above */
		buf->gref = req->gref = gnttab_grant_access(dev->dom, __pa(page)
				>> PAGE_SHIFT, 0);

		req->id = id;
	}

	wmb();

	dev->rx.req_prod_pvt = req_prod + i;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&dev->rx, notify);
	if (notify)
		notify_remote_via_evtchn(dev->evtchn);

}
