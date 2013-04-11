
#include "xen.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static struct xenstore_domain_interface *xenstore_buf;
typedef unsigned long xenbus_transaction_t;
static spinlock_t req_lock = SPIN_LOCK_UNLOCKED("xen_req");
static spinlock_t xenstore_lock = SPIN_LOCK_UNLOCKED("xenstore");

#define NR_REQS 32
static int nr_live_reqs;

#define XBT_NIL ((xenbus_transaction_t)0)
/* Watch event queue */
struct xenbus_event {
	/* Keep these two as this for xs.c */
	char *path;
	char *token;
	struct xenbus_event *next;
};
struct write_req {
	const void *data;
	unsigned len;
};

typedef struct xenbus_event *xenbus_event_queue;

xenbus_event_queue xenbus_events;
static struct watch {
	char *token;
	xenbus_event_queue *events;
	struct watch *next;
}*watches;

#define MAX_RESPONSE_SIZE 1024
struct xenbus_req_info {
	int in_use;
	int resp_ready;
	struct wait_struct waitq;
	struct xsd_sockmsg res_msg;
	unsigned char data[MAX_RESPONSE_SIZE];
};
static struct xenbus_req_info req_info[NR_REQS];
#define min(x,y) ({                       \
        typeof(x) tmpx = (x);                 \
        typeof(y) tmpy = (y);                 \
        tmpx < tmpy ? tmpx : tmpy;            \
        })
queue_t xb_waitq;

static void xenbus_evtchn_handler(evtchn_port_t port, struct pt_regs *regs,
		void *ign) {
	int ret;
	ret=sc_wakeUp(&xb_waitq); /* wake all the waiting processes */
	DEBUG(" Xenbus wait handler: Waking up the waiting process :%d \n",ret);
}

static uint32_t store_evtchannel;
/* Initialise xenbus. */
void init_xenbus(unsigned long xenstore_phyaddr, uint32_t evtchannel) {
	int err;
	DEBUG("Initialising xenbus\n");

	sc_register_waitqueue(&xb_waitq,"xen");
#define HOST_XEN_STORE_ADDR 0xe3000000
	xenstore_buf = HOST_XEN_STORE_ADDR;
	store_evtchannel = evtchannel;
	vm_mmap(0, xenstore_buf, PAGE_SIZE, PROT_WRITE, MAP_FIXED, xenstore_phyaddr);
	// create_thread("xenstore", xenbus_thread_func, NULL);
	DEBUG("buf at %p.\n", xenstore_buf);
	err = bind_evtchn(evtchannel, xenbus_evtchn_handler, NULL);
	unmask_evtchn(evtchannel);

	DEBUG("xenbus on irq %d\n", err);
}
static void process_responses() {
	struct xsd_sockmsg *resp;
	uint32_t req_id;
	XENSTORE_RING_IDX prod,cons;
	int processed_len=0;

	spin_lock(&xenstore_lock);
	prod=MASK_XENSTORE_IDX(xenstore_buf->rsp_prod);
	cons=MASK_XENSTORE_IDX(xenstore_buf->rsp_cons);


	resp = &xenstore_buf->rsp[cons];
	DEBUG("XEN xb_write buf:%x: req:%x:%x res:%x:%x  rsp:%x:%x:%x:%x \n",
			xenstore_buf, xenstore_buf->req_cons, xenstore_buf->req_prod,
			xenstore_buf->rsp_cons, xenstore_buf->rsp_prod,resp->type,resp->tx_id,resp->req_id,resp->len);

	while (prod != cons && processed_len<XENSTORE_RING_SIZE)
	{
		unsigned char *data;
		resp = &xenstore_buf->rsp[cons];

		cons=cons+sizeof(struct xsd_sockmsg) +resp->len;
		req_id=resp->req_id ;
		if (req_id > NR_REQS || req_id < 0)
		{
			DEBUG("XEN ERROR in process responser sp:%x:%x:%x:%x \n",resp->type,resp->tx_id,resp->req_id,resp->len);
			goto last ;
		}
		DEBUG("XEN  sp:%x:%x:%x:%x id:%d in_use:%d: \n",resp->type,resp->tx_id,resp->req_id,resp->len,req_id,req_info[req_id].in_use);
		if (req_info[req_id].in_use == 1)
		{
				ut_memcpy((uint8_t *)&req_info[req_id].res_msg,(uint8_t *)resp,sizeof(struct xsd_sockmsg));
				data=(unsigned char *)resp+sizeof(struct xsd_sockmsg);
				if (resp->len > 0)
				    ut_memcpy(req_info[req_id].data,data,resp->len);
				req_info[req_id].resp_ready=1;
		}else
		{
			DEBUG(" XEN ERROR:  no corresponding request\n");
		}
		processed_len=processed_len+resp->len;
	}
	xenstore_buf->rsp_cons=cons;


last:
	spin_unlock(&xenstore_lock);
	return;
}

static void xb_write(int type, int req_id, xenbus_transaction_t trans_id,
		const struct write_req *req,int nr_reqs) {
	XENSTORE_RING_IDX prod;
	int len = 0;
	const struct write_req *cur_req;
	int r,req_off;
	int total_off;
	int this_chunk;
	int ret;
	struct xsd_sockmsg m =
			{ .type = type, .req_id = req_id, .tx_id = trans_id };
	struct write_req header_req = { &m, sizeof(m) };

    for (r = 0; r < nr_reqs; r++)
        len += req[r].len;

	m.len = len;
	len += sizeof(m);

	cur_req = &header_req;

	BUG_ON(len > XENSTORE_RING_SIZE);
	/* Wait for the ring to drain to the point where we can send the
	 message. */
	prod = xenstore_buf->req_prod;
	if (prod + len - xenstore_buf->req_cons > XENSTORE_RING_SIZE) {
		/* Wait for there to be space on the ring */
		DEBUG("prod %d, len %d, cons %d, size %d; waiting.\n", prod, len,
				xenstore_buf->req_cons, XENSTORE_RING_SIZE);
		//  wait_event(xb_waitq,
		//        xenstore_buf->req_prod + len - xenstore_buf->req_cons <=
		//      XENSTORE_RING_SIZE);
		DEBUG("Back from wait.\n");
		prod = xenstore_buf->req_prod;
	}
	/* We're now guaranteed to be able to send the message without
	 overflowing the ring.  Do so. */
	total_off = 0;
	req_off = 0;
	while (total_off < len) {
		this_chunk = min(cur_req->len - req_off,
				XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(prod));
		ut_memcpy((char *) xenstore_buf->req + MASK_XENSTORE_IDX(prod),
				(char *) cur_req->data + req_off, this_chunk);
		prod += this_chunk;
		req_off += this_chunk;
		total_off += this_chunk;
		if (req_off == cur_req->len) {
			req_off = 0;
			if (cur_req == &header_req)
				cur_req = req;
			else
				cur_req++;
		}
	}

	DEBUG("Complete main loop of xb_write total_off:%d .\n",total_off);
	BUG_ON(req_off != 0);
	BUG_ON(total_off != len);
	BUG_ON(prod > xenstore_buf->req_cons + XENSTORE_RING_SIZE);

	/* Remote must see entire message before updating indexes */
	wmb();

	xenstore_buf->req_prod += len;

	/* Send evtchn to notify remote : TODO failing to send the remote event */
	ret = notify_remote_via_evtchn(store_evtchannel);
	DEBUG(" RETURN value:%d :%x  negative:%d store_CHN:%x  \n", ret, ret,
			-ret, store_evtchannel);
}

static void release_xenbus_id(int id) {
	BUG_ON(!req_info[id].in_use);
	spin_lock(&req_lock);
	req_info[id].in_use = 0;
	nr_live_reqs--;
	req_info[id].in_use = 0;
	req_info[id].resp_ready=0;
	// if (nr_live_reqs == NR_REQS - 1)
	//   wake_up(&req_wq);
	spin_unlock(&req_lock);
}

/* Allocate an identifier for a xenbus request.  Blocks if none are
 available. */
static int allocate_xenbus_id(void) {
	static int probe;
	int o_probe;

	while (1) {
		spin_lock(&req_lock);
		if (nr_live_reqs < NR_REQS)
			break;
		spin_unlock(&req_lock);

		//   wait_event(req_wq, (nr_live_reqs < NR_REQS));
	}

	o_probe = probe;
	for (;;) {
		if (!req_info[o_probe].in_use)
			break;
		o_probe = (o_probe + 1) % NR_REQS;
		BUG_ON(o_probe == probe);
	}
	nr_live_reqs++;
	req_info[o_probe].in_use = 1;
	req_info[o_probe].resp_ready=0;
	probe = (o_probe + 1) % NR_REQS;
	spin_unlock(&req_lock);

	return o_probe;
}

int xenbus_read(const char *path, char *reply, int reply_len) {
	struct write_req req[] = {{ path, ut_strlen(path) + 1 }};
	struct xsd_sockmsg *rep;
	char *res, *msg;
	int id,iter;
	int ret=0;

	id = allocate_xenbus_id();
	DEBUG(" BEFORE msg_repli id:%x Before  xb_write\n", id);
	xb_write(XS_READ, id, XBT_NIL, req,1);

	iter=0;
	while(iter<10 && req_info[id].resp_ready!=1)
	{
		process_responses();
		sc_wait(&xb_waitq,100);
		iter++;
	}
	DEBUG("AFTER NEW AFTER  wait xenbus read \n");


	if (req_info[id].resp_ready == 1 && req_info[id].res_msg.len > 0 )
	{
		ret=req_info[id].res_msg.len;
		if (ret < reply_len )
		     ut_memcpy(reply,req_info[id].data,req_info[id].res_msg.len);
		reply[ret]='\0';
	}else
	{
	   DEBUG(" NO DATA \n");
	}
	release_xenbus_id(id);
	return ret;
}

#define MAX_REPLY_LEN 100
int xenbus_write(const char *path, char *val) {
	struct write_req req[] = {{ path, ut_strlen(path) + 1 },{ val, ut_strlen(val) }};
	struct xsd_sockmsg *rep;
	char reply[MAX_REPLY_LEN];
	int id,iter;
	int ret=0;
    int reply_len=MAX_REPLY_LEN;

	id = allocate_xenbus_id();
	DEBUG(" BEFORE msg_repli id:%x Before  xb_write\n", id);
	xb_write(XS_WRITE, id, XBT_NIL, req,2);
	iter=0;
	while(iter<10 && req_info[id].resp_ready!=1)
	{
		process_responses();
		sc_wait(&xb_waitq,100);
		iter++;
	}
	DEBUG("AFTER    NEW WAIT  xb_write \n");

	if (req_info[id].resp_ready == 1 && req_info[id].res_msg.len > 0 )
	{
		ret=req_info[id].res_msg.len;
		if (ret < reply_len )
		     ut_memcpy(reply,req_info[id].data,req_info[id].res_msg.len);
		reply[ret]='\0';
		DEBUG(" reply data :%s: \n",reply);
	}else
	{
		DEBUG(" NO DATA \n");
	}
	release_xenbus_id(id);
	return ret;
}


int xen_readcmd(char *arg1, char *data, int max_len) {
	if (data == 0 ) return 0;
	data[0]='\0';
	DEBUG("arg1 :%s: \n",arg1);
	xenbus_read(arg1,data,max_len);
	DEBUG(" READ REQUEST  : %s -> %s \n",arg1,data);
	return 1;
}

int xen_writecmd(char *arg1, char *arg2) {
	int ret;
	DEBUG("arg1 :%s: arg2:%s:\n",arg1,arg2);
	ret=xenbus_write(arg1,arg2);
	DEBUG(" WRITE  REQUEST  :ret:%d \n",ret);
	return 1;
}
