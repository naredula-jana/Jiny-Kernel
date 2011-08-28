#include "xen.h"
#include <xen/io/netif.h>
#include <xen/io/ring.h>
struct net_buffer {
	void* page;
	grant_ref_t gref;
};
#define offsetof __builtin_offsetof

#define NET_TX_RING_SIZE __CONST_RING_SIZE(netif_tx, PAGE_SIZE)
#define NET_RX_RING_SIZE __CONST_RING_SIZE(netif_rx, PAGE_SIZE)

struct netfront_dev {
	domid_t dom;

	unsigned short tx_freelist[NET_TX_RING_SIZE + 1];
	//  struct semaphore tx_sem;

	struct net_buffer rx_buffers[NET_RX_RING_SIZE];
	struct net_buffer tx_buffers[NET_TX_RING_SIZE];

	struct netif_tx_front_ring tx;
	struct netif_rx_front_ring rx;
	grant_ref_t tx_ring_ref;
	grant_ref_t rx_ring_ref;
	evtchn_port_t evtchn;

	char *nodename;
	char *backend;
	char *mac;

	// xenbus_event_queue events;

	void (*netif_rx)(unsigned char* data, int len);
};

__attribute__((weak)) void netif_rx(unsigned char* data,int len);

static struct netfront_dev g_netfront_dev;
static void netfront_evtchn_handler(evtchn_port_t port, struct pt_regs *regs,
		void *ign) {
	int ret;

	ut_printf(" Xenbus netfront handler :%d \n", ret);
}
/* Unfortunate confusion of terminology: the port is unbound as far
 as Xen is concerned, but we automatically bind a handler to it
 from inside mini-os. */

int evtchn_alloc_unbound(uint32_t pal, void *handler, void *data,
		evtchn_port_t *port) {
	int rc;
	evtchn_alloc_unbound_t op;
	op.dom = DOMID_SELF;
	op.remote_dom = pal;

	rc = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound, &op);
	if (rc) {
		printk("ERROR: alloc_unbound failed with rc=%d", rc);
		return rc;
	}
	*port = bind_evtchn((uint32_t) op.port, handler, 0);
	return rc;
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

		buf->gref = req->gref = gnttab_grant_access(dev->dom, __pa(buf->page)>>PAGE_SHIFT,
				0);

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
static char path[256],value[256];
void network_rx(struct netfront_dev *dev);
int init_netfront() {
	static int init_done=0;
	int i;
	struct netif_tx_sring *txs;
	struct netif_rx_sring *rxs;
	struct netfront_dev *dev;
	uint32_t remote_domid = 0;

	if (init_done==1)
	{
		network_rx( &g_netfront_dev);
		return 1;
	}

	init_done=1;
	dev = &g_netfront_dev;
	dev->dom = 0; /* TODO  : later remove the hard coded backend id , read xenstore */
	ut_strcpy(nodename,"device/vif/0");

	DEBUG("NEW net TX ring size %d\n", NET_TX_RING_SIZE);
	DEBUG("net RX ring size %d\n", NET_RX_RING_SIZE);
	//init_SEMAPHORE(&dev->tx_sem, NET_TX_RING_SIZE);
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

	dev->tx_ring_ref = gnttab_grant_access(dev->dom, __pa(txs)>>PAGE_SHIFT, 0);
	dev->rx_ring_ref = gnttab_grant_access(dev->dom, __pa(rxs)>>PAGE_SHIFT, 0);

	init_rx_buffers(dev);

	 evtchn_alloc_unbound(remote_domid, netfront_evtchn_handler,0, &dev->evtchn);

	ut_snprintf(path,256,"%s/tx-ring-ref",nodename);
	ut_snprintf(value,256,"%d",dev->tx_ring_ref);
	xen_writecmd(path,value);

	ut_snprintf(path,256,"%s/rx-ring-ref",nodename);
	ut_snprintf(value,256,"%d",dev->rx_ring_ref);
	xen_writecmd(path,value);

	ut_snprintf(path,256,"%s/event-channel",nodename);
	ut_snprintf(value,256,"%d",dev->evtchn);
	xen_writecmd(path,value);

	ut_snprintf(path,256,"%s/request-rx-copy",nodename);
	xen_writecmd(path,"1");

	unmask_evtchn(dev->evtchn);
	ut_snprintf(path,256,"%s/state",nodename);
	xen_writecmd(path,"4");

	dev->netif_rx = netif_rx;
	ut_printf(" Net front driver initialization completed \n");

	return 0;
}
/************************************************************/
__attribute__((weak)) void netif_rx(unsigned char* data,int len)
{
    ut_printf("%d bytes incoming at %x\n",len,data);
}
static inline int xennet_rxidx(RING_IDX idx)
{
    return idx & (NET_RX_RING_SIZE - 1);
}
void network_rx(struct netfront_dev *dev)
{
    RING_IDX rp,cons,req_prod;
    struct netif_rx_response *rx;
    int nr_consumed, some, more, i, notify;


moretodo:
    rp = dev->rx.sring->rsp_prod;
    rmb(); /* Ensure we see queued responses up to 'rp'. */
    cons = dev->rx.rsp_cons;

    for (nr_consumed = 0, some = 0;
         (cons != rp) && !some;
         nr_consumed++, cons++)
    {
        struct net_buffer* buf;
        unsigned char* page;
        int id;

        rx = RING_GET_RESPONSE(&dev->rx, cons);

        if (rx->flags & NETRXF_extra_info)
        {
            printk("+++++++++++++++++++++ we have extras!\n");
            continue;
        }


        if (rx->status == NETIF_RSP_NULL) continue;

        id = rx->id;
        BUG_ON(id >= NET_TX_RING_SIZE);

        buf = &dev->rx_buffers[id];
        page = (unsigned char*)buf->page;
        gnttab_end_access(buf->gref);

        if(rx->status>0)
        {
		dev->netif_rx(page+rx->offset,rx->status);
        }
    }
    dev->rx.rsp_cons=cons;

    RING_FINAL_CHECK_FOR_RESPONSES(&dev->rx,more);
    if(more && !some) goto moretodo;

    req_prod = dev->rx.req_prod_pvt;

    for(i=0; i<nr_consumed; i++)
    {
        int id = xennet_rxidx(req_prod + i);
        netif_rx_request_t *req = RING_GET_REQUEST(&dev->rx, req_prod + i);
        struct net_buffer* buf = &dev->rx_buffers[id];
        void* page = buf->page;

        /* We are sure to have free gnttab entries since they got released above */
        buf->gref = req->gref =
            gnttab_grant_access(dev->dom,__pa(page)>>PAGE_SHIFT,0);

        req->id = id;
    }

    wmb();

    dev->rx.req_prod_pvt = req_prod + i;

    RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&dev->rx, notify);
    if (notify)
        notify_remote_via_evtchn(dev->evtchn);

}
