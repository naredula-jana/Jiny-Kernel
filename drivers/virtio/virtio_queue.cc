/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/virtio_queue.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "jdevice.h"

#define virtio_mb()    asm volatile("mfence":::"memory")  /* super of the below */
#define virtio_rmb()   asm volatile("lfence":::"memory")
#define virtio_wmb()   asm volatile("sfence":::"memory")
#define uninitialized_var(x) x = x

#define START_USE(_vq)						\
	do {							\
		if ((_vq)->in_use)	{			\
			DEBUG("%s:in_use = %d\n",		\
			      (_vq)->vq.name, (_vq)->in_use);	\
			      BUG();} \
		(_vq)->in_use = __LINE__;			\
	} while (0)
#define END_USE(_vq) \
	do { BUG_ON(!(_vq)->in_use); (_vq)->in_use = 0; } while(0)

void virtio_queue::sync_avial_idx(struct vring_queue *vq) {
	uint16_t old;
	/* Descriptors and available array need to be set before we expose the
	 * new available array entries. */
	//virtio_wmb();
	virtio_mb();

	old = vq->vring.avail->idx;
	vq->vring.avail->idx = old + vq->num_added;
	vq->num_added = 0;

	/* Need to update avail index before checking if we should notify */
	virtio_mb();
}

static inline void detach_buf(struct vring_queue *vq, unsigned int head) {
	unsigned int i;

	/* Put back on free list: find end */
	i = head;

	/* Free the indirect table */
	if (vq->vring.desc[i].flags & VRING_DESC_F_INDIRECT) {
		BRK; /* currently we are not supporting this */
	}

	while (vq->vring.desc[i].flags & VRING_DESC_F_NEXT) {
		vq->vring.desc[i].addr = 0;
		i = vq->vring.desc[i].next;
		vq->num_free++;
	}
	vq->vring.desc[i].addr = 0;

	if (vq->free_head == -1){
		vq->free_head = head;
	}else{
		vq->vring.desc[vq->free_tail].next = head;
	}
	vq->free_tail = i;

	/* Plus final descriptor */
	vq->num_free++;
}
static inline void vring_init(struct mem_vring *vr, unsigned int num, void *p,
		unsigned long align) {
	vr->num = num;
	vr->desc = (struct mem_vring_desc *) p;
	vr->avail = (struct mem_vring_avail *) ((unsigned char *) p
			+ num * sizeof(struct mem_vring_desc));
	vr->used = (struct mem_vring_used *) (((unsigned long) &vr->avail->ring[num]
			+ align - 1) & ~(align - 1));
}
void virtio_queue::init_virtqueue(unsigned int num, unsigned int vring_align,
		unsigned long pci_ioaddr, void *pages,
		void (*callback)(struct virtio_queue *), const char *name,
		int queue_number) {
	struct vring_queue *vq;
	unsigned int i;

	/* We assume num is a power of 2. */
	if (num & (num - 1)) {
		DEBUG("Bad virtqueue length %u\n", num);
		return;
	}

	vq = mm_malloc(sizeof(*vq) + sizeof(void *) * num, 1);
	if (!vq) {
		return;
	}

	vring_init(&vq->vring, num, pages, vring_align);
	INIT_LOG("		vring  desc:%x avail:%x used:%x \n", vq->vring.desc,
			vq->vring.avail, vq->vring.used);

	vq->pci_ioaddr = pci_ioaddr;
	vq->queue_number = queue_number;

	vq->name = name;
	vq->broken = false;
	vq->last_used_idx = 0;
	vq->num_added = 0;
	vq->stat_alloc = 0;
	vq->stat_free = 0;
	vq->in_use = false;

	/* No callback?  Tell other side not to bother us. */
	if (!callback)
		vq->vring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;

	/* Put everything in free lists. */
	vq->num_free = num;
	vq->free_head = 0;
	for (i = 0; i < num - 1; i++) {
		vq->vring.desc[i].next = i + 1;
	}
	vq->free_tail = num-1;
	INIT_LOG("	virtqueue Initialized vq:%x  \n",vq);
	queue = vq;

	stat_add_success=1;
	stat_add_fails=0;
	stat_add_pkts=0;
	stat_rem_success=1;
	stat_rem_fails=0;
	stat_rem_pkts=0;
}
extern int get_order(unsigned long size);
static inline unsigned vring_size(unsigned int num, unsigned long align) {
	return ((sizeof(struct mem_vring_desc) * num + sizeof(uint16_t) * (2 + num)
			+ align - 1) & ~(align - 1)) + sizeof(uint16_t) * 3
			+ sizeof(struct mem_vring_used_elem) * num;
}
bool virtio_queue::virtqueue_enable_cb_delayed() {
	struct vring_queue *vq = this->queue;
	uint16_t bufs;

	START_USE(vq);
	vq->vring.avail->flags &= ~VRING_AVAIL_F_NO_INTERRUPT;
	/* TODO: tune this threshold */
	bufs = (uint16_t) (vq->vring.avail->idx - vq->last_used_idx) * 3 / 4;
	vring_used_event(&vq->vring) = vq->last_used_idx + bufs;
	virtio_mb();
	if (((uint16_t) (vq->vring.used->idx - vq->last_used_idx) > bufs)) {
		END_USE(vq);
		return false;
	}
	END_USE(vq);

	return true;
}
void virtio_queue::notify() {
	struct vring_queue *vq = this->queue;

	outw(vq->pci_ioaddr + VIRTIO_PCI_QUEUE_NOTIFY, vq->queue_number);
}
int virtio_queue::virtio_queuekick() {
	int ret = 0;
	struct vring_queue *vq = this->queue;
	uint16_t old;

	START_USE(vq);
	sync_avial_idx(vq);
	if (!(vq->vring.used->flags & VRING_USED_F_NO_NOTIFY)) {
		notify();
		ret = 1;
		atomic_inc(&stat_kicks);
	}
	END_USE(vq);
	return ret;
}
virtio_queue::virtio_queue(jdevice *device, uint16_t index, int queue_type) {
	int size;
	uint16_t num;
	unsigned long ring_addr;
	unsigned long pci_ioaddr = device->pci_device.pci_ioaddr;
	unsigned char *qname;

	outw(pci_ioaddr + VIRTIO_PCI_QUEUE_SEL, index);
	num = inw(pci_ioaddr + VIRTIO_PCI_QUEUE_NUM);
	//INIT_LOG("		New virtio c++ create queue NUM-%d : num %x(%d)  :%x\n", index, num, num, vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	if (num == 0) {
		return;
	}
	size = PAGE_ALIGN(vring_size(num, VIRTIO_PCI_VRING_ALIGN));
	ring_addr = mm_getFreePages(MEM_CLEAR, get_order(size));

	/* activate the queue */
	outl(pci_ioaddr + VIRTIO_PCI_QUEUE_PFN,
			__pa(ring_addr) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	qname="send_queue";
	if (queue_type == VQTYPE_RECV){
		qname="recv_queue";
	}
	/* create the vring */
	INIT_LOG("		virtioqueuec++  Creating queue:%x(pa:%x) size-order:%x  size:%x  pcioaddr:%x\n",
			ring_addr, __pa(ring_addr), get_order(size), size,pci_ioaddr);
	init_virtqueue(num, VIRTIO_PCI_VRING_ALIGN, device->pci_device.pci_ioaddr,
			(void *) ring_addr,  0, qname, index);
	virtqueue_enable_cb_delayed();
	this->qType = queue_type;
}
void virtio_queue::print_stats(unsigned char *arg1,unsigned char *arg2) {
	uint16_t diff;
	struct vring_queue *vq = this->queue;
	int i;

	diff = vq->vring.used->idx - vq->vring.avail->idx;
	if (diff < 0){
		diff = diff * (-1);
	}
	if (arg1 && ut_strcmp(arg1, "all") == 0) {
		for (i = 0; i < vq->vring.num; i++) {
			ut_printf("%d: addr:%x next:%d len:%d flags:%d avail:%d used:%d\n",
					i, vq->vring.desc[i].addr, vq->vring.desc[i].next,
					vq->vring.desc[i].len, vq->vring.desc[i].flags,
					vq->vring.avail->ring[i], vq->vring.used->ring[i]);
		}
	}
	ut_printf("		%s :%x size:%i num_free:%i  free_head:%d  tail:%d used:%u(%i) avail:%u(%i) diff:%u last_use_idx:%u alloc:%i free:%i kicks:%i empty_err:%d\n",
			vq->name, vq, vq->vring.num, vq->num_free, vq->free_head, vq->free_tail,vq->vring.used->idx,
			vq->vring.used->idx, vq->vring.avail->idx, vq->vring.avail->idx,
			diff, vq->last_used_idx, vq->stat_alloc, vq->stat_free, stat_kicks.counter,stat_error_empty_bufs);
	ut_printf("		VQueue adding success:%d fails:%d pkts:%d rate of pkt/sucees:%d\n",stat_add_success,stat_add_fails,stat_add_pkts,stat_add_pkts/stat_add_success);
	ut_printf("		VQueue remove success:%d fails:%d pkts:%d rate of pkt/sucees:%d\n",stat_rem_success,stat_rem_fails,stat_rem_pkts,stat_rem_pkts/stat_rem_success);
#if 0
	if (arg1 && (ut_strcmp(arg1, "clean") == 0)) {
		stat_add_success=1;
		stat_add_fails=0;
		stat_add_pkts=0;
		stat_rem_success=1;
		stat_rem_fails=0;
		stat_rem_pkts=0;
		ut_printf("clean stats done\n");
	}
#endif
}
int virtio_queue::virtio_disable_cb(){
	struct vring_queue *vq = this->queue;

	vq->vring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
	return 1;
}
static inline bool more_used(const struct vring_queue *vq) {
	return vq->last_used_idx != vq->vring.used->idx;
}
bool virtio_queue::virtio_enable_cb() {
	struct vring_queue *vq = this->queue;

	START_USE(vq);
	vq->vring.avail->flags &= ~VRING_AVAIL_F_NO_INTERRUPT;
	vring_used_event(&vq->vring) = vq->last_used_idx;
	virtio_mb();
	if ((more_used(vq))) {
		END_USE(vq);
		return false;
	}
	END_USE(vq);

	return true;
}
int virtio_queue::check_recv_pkt() {
	struct vring_queue *vq = this->queue;
	return vq->vring.used->idx - vq->last_used_idx;
}
void *virtio_queue::virtio_removeFromQueue(unsigned int *len){
	struct vring_queue *vq = this->queue;
	void *ret;
	unsigned int i;

	START_USE(vq);

	if (unlikely(vq->broken)) {
		END_USE(vq);
		stat_rem_fails++;
		return NULL;
	}

	if (!more_used(vq)) {
	//	pr_debug("No more buffers in queue\n");
		END_USE(vq);
		stat_rem_fails++;
		return NULL;
	}

	/* Only get used array entries after they have been exposed by host. */
	virtio_rmb();

	i = vq->vring.used->ring[vq->last_used_idx%vq->vring.num].id;
	*len = vq->vring.used->ring[vq->last_used_idx%vq->vring.num].len;
	ret = __va(vq->vring.desc[i].addr);

	if (unlikely(i >= vq->vring.num)) {
		BRK;
		return NULL;
	}

	/* detach_buf clears data, so grab it now. */
	vq->stat_free++;
	detach_buf(vq, i);
	vq->last_used_idx++;
	/* If we expect an interrupt for the next entry, tell host
	 * by writing event index and flush out the write before
	 * the read in the next get_buf call. */
	if (!(vq->vring.avail->flags & VRING_AVAIL_F_NO_INTERRUPT)) {
		vring_used_event(&vq->vring) = vq->last_used_idx;
		virtio_mb();
	}
	stat_rem_success++;
	stat_rem_pkts = stat_rem_pkts+1;
	END_USE(vq);
	return ret;
}
#define ERROR_VIRTIO_ENOSPC 2 /* TODO duplicate definition */

static inline unsigned long sg_phys(struct scatterlist *sg)
{
	 return __pa(sg->page_link)+ sg->offset;
}
int virtio_queue::virtio_add_buf_to_queue(struct scatterlist sg[],
			  unsigned int out,
			  unsigned int in,
			  void *data, int gfp)
{
	struct vring_queue *vq = this->queue;
	unsigned int i, avail, uninitialized_var(prev);
	int head;

	START_USE(vq);
	BUG_ON(data == NULL);

	BUG_ON(out + in > vq->vring.num);
	BUG_ON(out + in == 0);

	if (vq->num_free < (out + in)) {
		/* FIXME: for historical reasons, we force a notify here if
		 * there are outgoing parts to the buffer.  Presumably the
		 * host should service the ring ASAP. */
		if (out){
		//	vq->notify(&vq->vq);  /* this is commented out since , notify is called seperately */
		}
		END_USE(vq);
		return -ERROR_VIRTIO_ENOSPC;
	}else{
		//print_vq(_vq);
	}

	/* We're about to use some buffers from the free list. */
	vq->num_free -= (out + in);

	head = vq->free_head;
	for (i = vq->free_head; out; i = vq->vring.desc[i].next, out--) {
		vq->vring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vring.desc[i].addr = sg_phys(sg);
		vq->vring.desc[i].len = sg->length;
		prev = i;
		sg++;
	}
	for (; in; i = vq->vring.desc[i].next, in--) {
		vq->vring.desc[i].flags = VRING_DESC_F_NEXT|VRING_DESC_F_WRITE;
		vq->vring.desc[i].addr = sg_phys(sg);
		vq->vring.desc[i].len = sg->length;
		prev = i;
		sg++;
	}
	/* Last one doesn't continue. */
	vq->vring.desc[prev].flags &= ~VRING_DESC_F_NEXT;

	/* Update free pointer */
	vq->free_head = i;

add_head:

	vq->stat_alloc++;

	/* Put entry in available array (but don't update avail->idx until they
	 * do sync).  FIXME: avoid modulus here? */
	avail = (vq->vring.avail->idx + vq->num_added++) % vq->vring.num;
	vq->vring.avail->ring[avail] = head;

	sync_avial_idx(vq);
	stat_add_success++;
	stat_add_pkts = stat_add_pkts+1;

	//stat_add_fails++;
	END_USE(vq);
	return vq->num_free;
}


int virtio_queue::BulkRemoveFromQueue(struct struct_mbuf *mbuf_list, int list_len) {
	struct vring_queue *vq = this->queue;
	int ret = 0;
	unsigned int i,head;
	uint16_t pkts_length;
	int count;

	pkts_length = vq->vring.used->idx - vq->last_used_idx;
	if (pkts_length == 0) {
		return 0;
	}

	START_USE(vq);
	if ((vq->broken)) {
		END_USE(vq);
		return 0;
	}

	/* Prefetch available ring to retrieve head indexes. */
	ar_prefetch0(&vq->vring.used->ring[vq->last_used_idx % vq->vring.num]);

	if (pkts_length > list_len) {
		pkts_length = (uint16_t) list_len;
	}
	/* Only get used array entries after they have been exposed by host. */
	virtio_rmb();

	for (count = 0; count < pkts_length; count++) {
		i = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].id;

		ar_prefetch0(&vq->vring.desc[i]);
		if ((i >= vq->vring.num)) {
			//JANA removed BAD_RING(vq, "id %u out of range\n", i);
			ut_printf("BAD RING -11 vq:%x \n", vq);
			BRK;
			return 0;
		}
		if (vq->vring.desc[i].addr == 0) {
			BRK;
		}
		if (mbuf_list !=0 ){
			mbuf_list[count].buf = __va(vq->vring.desc[i].addr);
			mbuf_list[count].len = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].len;
		}else{
			free_page(__va(vq->vring.desc[i].addr));
		}
		vq->stat_free++;
#if 1
		head = i;
		/* Free the indirect table */
		if (vq->vring.desc[i].flags & VRING_DESC_F_INDIRECT) {
			BRK; /* currently we are not supporting this */
		}

		while (vq->vring.desc[i].flags & VRING_DESC_F_NEXT) {
			vq->vring.desc[i].addr = 0;
			i = vq->vring.desc[i].next;
			vq->num_free++;
		}
		vq->vring.desc[i].addr = 0;

		if (vq->free_head == -1){
			vq->free_head = head;
		}else{
			vq->vring.desc[vq->free_tail].next = head;
		}
		vq->free_tail = i;

		/* Plus final descriptor */
		vq->num_free++;
#else
		//detach_buf(vq, i);
#endif
		vq->last_used_idx++;
		ret++;
	}
	/* If we expect an interrupt for the next entry, tell host
	 * by writing event index and flush out the write before
	 * the read in the next get_buf call. */
	if (!(vq->vring.avail->flags & VRING_AVAIL_F_NO_INTERRUPT)) {
		vring_used_event(&vq->vring) = vq->last_used_idx;
		virtio_mb();
	}
	END_USE(vq);

	if (ret>0){
		stat_rem_success++;
		stat_rem_pkts = stat_rem_pkts+ret;
	}else{
		stat_rem_fails++;
	}
	return ret;
}
int virtio_queue::MaxBufsSpace(){
	struct vring_queue *vq = this->queue;
	return vq->num_free/scatter_list_size ;
}
int virtio_queue::BulkAddToQueue(struct struct_mbuf *mbuf_list, int list_len,
		int read_only) {
	struct vring_queue *vq = this->queue;
	unsigned int i,k, avail, uninitialized_var(prev);
	int head, index, len, ret = 0;
	unsigned char *data;
	int in,out;
	struct scatter_buf scatter_list[4];

	START_USE(vq);
	for (index = 0; index < list_len; index++) {
		unsigned int total_bufs = scatter_list_size;
		if (vq->num_free < (scatter_list_size)) {
			goto last;
		}
		if (mbuf_list) {
			data = mbuf_list[index].buf;
			len = mbuf_list[index].len;
			//mbuf_list[index].buf = 0;
		} else {
			data = (unsigned char *) jalloc_page(MEM_NETBUF);
			len = 4096; /* page size */
			ut_memset(data, 0, 10);
		}
		if (data == 0) {
			stat_error_empty_bufs++;
			continue;
		}
		if (virtio_type == VIRTIO_ID_NET){
			if (mbuf_list){
				data = data - 10;
			}
			if (read_only == 1){
				out = scatter_list_size;
			}else{
				out = 0;
			}
			//if (total_bufs != 2) {BUG();}

			scatter_list[0].addr= __pa(data);
			scatter_list[0].length = 10;
			scatter_list[1].addr= scatter_list[0].addr+10;
			scatter_list[1].length = len;

		} else if (virtio_type == VIRTIO_ID_BLOCK){
			struct virtio_blk_req *disk_req=(struct virtio_blk_req *)data;
			//if (total_bufs != 3){ BUG(); }
			if (disk_req->type == VIRTIO_BLK_T_IN){
				out = 1;
				in = 2;
			}else{
				out = 2;
				in = 1;
			}
			scatter_list[0].addr= __pa(data);
			scatter_list[0].length = 16;

			if (disk_req->user_data == 0){
				scatter_list[1].addr= __pa(data + 16 + 8 + 8);
			}else{
				scatter_list[1].addr= __pa(disk_req->user_data);
			}
			if (len < 512){
				len = 512;
			}
			scatter_list[1].length = len;
			scatter_list[2].addr= __pa(data+16);
			scatter_list[2].length = 1;
		}

		/* We're about to use some buffers from the free list. */
		vq->num_free -= (scatter_list_size);
		head = vq->free_head;
		ar_prefetch0(&vq->vring.desc[head]);

		k=0;
		for (i = vq->free_head; total_bufs; i = vq->vring.desc[i].next, total_bufs--,k++) {
			if (out > k) {
				vq->vring.desc[i].flags = VRING_DESC_F_NEXT;
			} else {
				vq->vring.desc[i].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
			}
			if (vq->vring.desc[i].addr != 0) {
				BRK;
			}
			vq->vring.desc[i].addr = scatter_list[k].addr;
			vq->vring.desc[i].len = scatter_list[k].length;
			if (vq->vring.desc[i].addr ==0){
				BRK;
			}
			prev = i;
		}


		/* Last one doesn't continue. */
		vq->vring.desc[prev].flags &= ~VRING_DESC_F_NEXT;

		/* Update free pointer */
		vq->free_head = i;
		vq->stat_alloc++;

		/* Put entry in available array (but don't update avail->idx until they
		 * do sync).  FIXME: avoid modulus here? */
		avail = (vq->vring.avail->idx + vq->num_added) % vq->vring.num;
		vq->num_added++;

		vq->vring.avail->ring[avail] = head;
		ret++;
	}
last:
	if (ret > 0) {
		sync_avial_idx(vq);
		notify_needed = (!(vq->vring.used->flags & VRING_USED_F_NO_NOTIFY));
		stat_add_success++;
		stat_add_pkts = stat_add_pkts+ret;
	}else{
		stat_add_fails++;
	}
	END_USE(vq);
	return ret;
}
