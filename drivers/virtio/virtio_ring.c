/* Virtio ring implementation.
 *
 *  Copyright 2007 Rusty Russell IBM Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#define DEBUG_ENABLE 1
#include "virtio.h"
#include "virtio_pci.h"
#include "virtio_ring.h"
#include "mach_dep.h"
//#include "virtio_config.h"

#define CONFIG_SMP 1
#define smp_mb()    asm volatile("mfence":::"memory")  /* super of the below */
#define smp_rmb()   asm volatile("lfence":::"memory")
#define smp_wmb()   asm volatile("sfence" ::: "memory")

/* virtio guest is communicating with a virtual "device" that actually runs on
 * a host processor.  Memory barriers are used to control SMP effects. */
#ifdef CONFIG_SMP
/* Where possible, use SMP barriers which are more lightweight than mandatory
 * barriers, because mandatory barriers control MMIO effects on accesses
 * through relaxed memory I/O windows (which virtio does not use). */
#define virtio_mb() smp_mb()
#define virtio_rmb() smp_rmb()
#define virtio_wmb() smp_wmb()
#else
/* We must force memory ordering even if guest is UP since host could be
 * running on another CPU, but SMP barriers are defined to barrier() in that
 * configuration. So fall back to mandatory barriers instead. */
#define virtio_mb() mb()
#define virtio_rmb() rmb()
#define virtio_wmb() wmb()
#endif

#ifdef DEBUG
#define dev_err(x)
/* For development, we want to crash whenever the ring is screwed. */
#define BAD_RING(_vq, fmt, args...)				\
	do {							\
		dev_err(&(_vq)->vq.vdev->dev,			\
			"%s:"fmt, (_vq)->vq.name, ##args);	\
		BUG();						\
	} while (0)
/* Caller is supposed to guarantee no reentry. */
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
#else

#define dev_err(x)
#define BAD_RING(_vq, fmt, args...)				\
	do {							\
		dev_err(&_vq->vq.vdev->dev,			\
			"%s:"fmt, (_vq)->vq.name, ##args);	\
		(_vq)->broken = true;				\
	} while (0)
#define START_USE(vq)
#define END_USE(vq)
#endif


struct vring_virtqueue
{
	struct virtqueue vq;

	/* Actual memory layout for this queue */
	struct vring vring;

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
	/* Number we've added since last sync. */
	unsigned int num_added;

	/* Last used index we've seen. */
	u16 last_used_idx;

	/* How to notify other side. FIXME: commonalize hcalls! */
	void (*notify)(struct virtqueue *vq);

#ifdef DEBUG
	/* They're supposed to lock for us. */
	//unsigned int in_use;
	unsigned long in_use;
#endif
	unsigned int stat_alloc,stat_free; /* Jana added */
	/* Tokens for callbacks. */
	void *data[];
};

#define to_vvq(_vq) container_of(_vq, struct vring_virtqueue, vq)

/* Set up an indirect table of descriptors and add it to the queue. */
static int vring_add_indirect(struct vring_virtqueue *vq,
			      struct scatterlist sg[],
			      unsigned int out,
			      unsigned int in,
			      gfp_t gfp)
{
	struct vring_desc *desc;
	unsigned head;
	int i;

	desc = kmalloc((out + in) * sizeof(struct vring_desc), gfp);
	if (!desc)
		return -ENOMEM;

	/* Transfer entries from the sg list into the indirect page */
	for (i = 0; i < out; i++) {
		desc[i].flags = VRING_DESC_F_NEXT;
		desc[i].addr = sg_phys(sg);
		desc[i].len = sg->length;
		desc[i].next = i+1;
		sg++;
	}
	for (; i < (out + in); i++) {
		desc[i].flags = VRING_DESC_F_NEXT|VRING_DESC_F_WRITE;
		desc[i].addr = sg_phys(sg);
		desc[i].len = sg->length;
		desc[i].next = i+1;
		sg++;
	}

	/* Last one doesn't continue. */
	desc[i-1].flags &= ~VRING_DESC_F_NEXT;
	desc[i-1].next = 0;

	/* We're about to use a buffer */
	vq->num_free--;

	/* Use a single buffer which doesn't continue */
	head = vq->free_head;
	vq->vring.desc[head].flags = VRING_DESC_F_INDIRECT;
	vq->vring.desc[head].addr = virt_to_phys(desc);
	vq->vring.desc[head].len = i * sizeof(struct vring_desc);

	/* Update free pointer */
	vq->free_head = vq->vring.desc[head].next;

	return head;
}
extern int netbh_state;
void print_vq(struct virtqueue *_vq) {
	uint16_t diff;
	if (_vq == 0) {
		ut_printf("		vq Empty \n");
		return;
	}
	struct vring_virtqueue *vq = to_vvq(_vq);
	diff = vq->vring.used->idx - vq->vring.avail->idx;
	if (diff < 0)
		diff = diff * (-1);
	ut_printf("		vq:%x size:%i num_free:%i  free_head:%d used:%u(%i) avail:%u(%i) diff:%u last_use_idx:%u alloc:%i free:%i\n", vq,vq->vring.num,  vq->num_free, vq->free_head,
			vq->vring.used->idx, vq->vring.used->idx, vq->vring.avail->idx, vq->vring.avail->idx, diff, vq->last_used_idx,vq->stat_alloc,vq->stat_free);

}
static sync_avial_idx(struct vring_virtqueue *vq){
	u16 new, old;
	/* Descriptors and available array need to be set before we expose the
	 * new available array entries. */
	virtio_wmb();

	old = vq->vring.avail->idx;
	new = vq->vring.avail->idx = old + vq->num_added;
	vq->num_added = 0;

	/* Need to update avail index before checking if we should notify */
	virtio_mb();
}
int virtio_add_buf_to_queue(struct virtqueue *_vq,
			  struct scatterlist sg[],
			  unsigned int out,
			  unsigned int in,
			  void *data,
			  gfp_t gfp)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	unsigned int i, avail, uninitialized_var(prev);
	int head;

	START_USE(vq);

	BUG_ON(data == NULL);


	/* If the host supports indirect descriptor tables, and we have multiple
	 * buffers, then go indirect. FIXME: tune this threshold */
	if (vq->indirect && (out + in) > 1 && vq->num_free) {
		head = vring_add_indirect(vq, sg, out, in, gfp);
		if (likely(head >= 0))
			goto add_head;
	}

	BUG_ON(out + in > vq->vring.num);
	BUG_ON(out + in == 0);

	if (vq->num_free < (out + in)) {
#if 0
		pr_debug("Can't add buf len %i - avail = %i  in:%d vringnum:%d\n",
			 out + in, vq->num_free, in,  vq->vring.num);
		print_vq(_vq);
	//	while(1);
#endif
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
	/* Set token. */
	if (vq->data[head] != 0){
		BRK;
	}
	vq->data[head] = data;
	vq->stat_alloc++;

	/* Put entry in available array (but don't update avail->idx until they
	 * do sync).  FIXME: avoid modulus here? */
	avail = (vq->vring.avail->idx + vq->num_added++) % vq->vring.num;
	vq->vring.avail->ring[avail] = head;

	sync_avial_idx(vq);
	//pr_debug("Added buffer head %i to %p\n", head, vq);
	END_USE(vq);

	return vq->num_free;
}


int virtio_queuekick(struct virtqueue *_vq)
{
	int ret =0;
	struct vring_virtqueue *vq = to_vvq(_vq);
	u16 new, old;

	START_USE(vq);

	sync_avial_idx(vq);

	if (!(vq->vring.used->flags & VRING_USED_F_NO_NOTIFY)){
		vq->notify(&vq->vq);
		ret =1;
	}

	END_USE(vq);
	return ret;
}

static void detach_buf(struct vring_virtqueue *vq, unsigned int head)
{
	unsigned int i;

	/* Clear data ptr. */
	vq->data[head] = NULL;

	/* Put back on free list: find end */
	i = head;

	/* Free the indirect table */
	if (vq->vring.desc[i].flags & VRING_DESC_F_INDIRECT)
		kfree(phys_to_virt(vq->vring.desc[i].addr));

	while (vq->vring.desc[i].flags & VRING_DESC_F_NEXT) {
		i = vq->vring.desc[i].next;
		vq->num_free++;
	}

	vq->vring.desc[i].next = vq->free_head;
	vq->free_head = head;
	/* Plus final descriptor */
	vq->num_free++;
}

static inline bool more_used(const struct vring_virtqueue *vq)
{
	return vq->last_used_idx != vq->vring.used->idx;
}

void *virtio_removeFromQueue(struct virtqueue *_vq, unsigned int *len)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	void *ret;
	unsigned int i;

	START_USE(vq);

	if (unlikely(vq->broken)) {
		END_USE(vq);
		return NULL;
	}

	if (!more_used(vq)) {
	//	pr_debug("No more buffers in queue\n");
		END_USE(vq);
		return NULL;
	}

	/* Only get used array entries after they have been exposed by host. */
	virtio_rmb();

	i = vq->vring.used->ring[vq->last_used_idx%vq->vring.num].id;
	*len = vq->vring.used->ring[vq->last_used_idx%vq->vring.num].len;

	if (unlikely(i >= vq->vring.num)) {
		//JANA removed BAD_RING(vq, "id %u out of range\n", i);
		pr_debug("BAD RING -11 vq:%x \n",vq);
		BRK;
		return NULL;
	}
	if (unlikely(!vq->data[i])) {
		// JANA removed BAD_RING(vq, "id %u is not a head!\n", i);
		pr_debug("BAD RING -22 vq:%x \n",vq);
		BRK;
		return NULL;
	}

	/* detach_buf clears data, so grab it now. */
	ret = vq->data[i];
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

	END_USE(vq);
	return ret;
}


void virtio_disable_cb(struct virtqueue *_vq)
{
	if (_vq == 0){
		return;
	}
	struct vring_virtqueue *vq = to_vvq(_vq);

	vq->vring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
}


bool virtio_enable_cb(struct virtqueue *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);

	START_USE(vq);

	/* We optimistically turn back on interrupts, then check if there was
	 * more to do. */
	/* Depending on the VIRTIO_RING_F_EVENT_IDX feature, we need to
	 * either clear the flags bit or point the event index at the next
	 * entry. Always do both to keep code simple. */
	vq->vring.avail->flags &= ~VRING_AVAIL_F_NO_INTERRUPT;
	vring_used_event(&vq->vring) = vq->last_used_idx;
	virtio_mb();
	if (unlikely(more_used(vq))) {
		END_USE(vq);
		return false;
	}

	END_USE(vq);
	return true;
}


bool virtqueue_enable_cb_delayed(struct virtqueue *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	u16 bufs;

	START_USE(vq);

	/* We optimistically turn back on interrupts, then check if there was
	 * more to do. */
	/* Depending on the VIRTIO_RING_F_USED_EVENT_IDX feature, we need to
	 * either clear the flags bit or point the event index at the next
	 * entry. Always do both to keep code simple. */
	vq->vring.avail->flags &= ~VRING_AVAIL_F_NO_INTERRUPT;
	/* TODO: tune this threshold */
	bufs = (u16)(vq->vring.avail->idx - vq->last_used_idx) * 3 / 4;
	vring_used_event(&vq->vring) = vq->last_used_idx + bufs;
	virtio_mb();
	if (unlikely((u16)(vq->vring.used->idx - vq->last_used_idx) > bufs)) {
		END_USE(vq);
		return false;
	}

	END_USE(vq);
	return true;
}

#if 0
void *virtqueue_detach_unused_buf(struct virtqueue *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	unsigned int i;
	void *buf;

	START_USE(vq);

	for (i = 0; i < vq->vring.num; i++) {
		if (!vq->data[i])
			continue;
		/* detach_buf clears data, so grab it now. */
		buf = vq->data[i];
		detach_buf(vq, i);
		vq->vring.avail->idx--;
		END_USE(vq);
		return buf;
	}
	/* That should have freed everything. */
	BUG_ON(vq->num_free != vq->vring.num);

	END_USE(vq);
	return NULL;
}
#endif

irqreturn_t vring_interrupt(int irq, void *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);

	if (!more_used(vq)) {
		pr_debug("virtqueue interrupt with no work for %p\n", vq);
		return IRQ_NONE;
	}

	if (unlikely(vq->broken))
		return IRQ_HANDLED;

	pr_debug("virtqueue callback for %p (%p)\n", vq, vq->vq.callback);
	if (vq->vq.callback)
		vq->vq.callback(&vq->vq);

	return IRQ_HANDLED;
}


struct virtqueue *vring_new_virtqueue(unsigned int num,
				      unsigned int vring_align,
				 //     virtio_dev_t *vdev,
				      unsigned long pci_ioaddr,
				      void *pages,
				      void (*notify)(struct virtqueue *),
				      void (*callback)(struct virtqueue *),
				      const char *name, int queue_number)
{
	struct vring_virtqueue *vq;
	unsigned int i;

	/* We assume num is a power of 2. */
	if (num & (num - 1)) {
		//dev_warn(&vdev->dev, "Bad virtqueue length %u\n", num);
		DEBUG("Bad virtqueue length %u\n", num);
		return NULL;
	}

	vq = kmalloc(sizeof(*vq) + sizeof(void *)*num, GFP_KERNEL);
	if (!vq)
		return NULL;

	vring_init(&vq->vring, num, pages, vring_align);
	INIT_LOG("		vring  desc:%x avail:%x used:%x \n",vq->vring.desc,vq->vring.avail,vq->vring.used);
	vq->vq.callback = callback;
#if 1
	vq->vq.pci_ioaddr = pci_ioaddr;
	vq->vq.queue_number = queue_number;
#else
	//vq->vq.vdev = vdev;
#endif
	vq->vq.name = name;
	vq->notify = notify;
	vq->broken = false;
	vq->last_used_idx = 0;
	vq->num_added = 0;
	vq->stat_alloc =0;
	vq->stat_free=0;
	//list_add_tail(&vq->vq.list, &vdev->vqs);
#ifdef DEBUG
	vq->in_use = false;
#endif


//JANA	vq->indirect = virtio_has_feature(vdev, VIRTIO_RING_F_INDIRECT_DESC);
//JANA  vq->event = virtio_has_feature(vdev, VIRTIO_RING_F_EVENT_IDX);


	/* No callback?  Tell other side not to bother us. */
	if (!callback)
		vq->vring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;

	/* Put everything in free lists. */
	vq->num_free = num;
	vq->free_head = 0;
	for (i = 0; i < num-1; i++) {
		vq->vring.desc[i].next = i+1;
		vq->data[i] = NULL;
	}
	vq->data[i] = NULL;
	INIT_LOG("		virtqueue Initialized  \n");
	return &vq->vq;
}


void vring_del_virtqueue(struct virtqueue *vq)
{
	list_del(&vq->list);
	kfree(to_vvq(vq));
}


/* Manipulates transport-specific feature bits. */
void vring_transport_features(struct virtio_device *vdev)
{
	unsigned int i;

	for (i = VIRTIO_TRANSPORT_F_START; i < VIRTIO_TRANSPORT_F_END; i++) {
		switch (i) {
		case VIRTIO_RING_F_INDIRECT_DESC:
			break;
		case VIRTIO_RING_F_EVENT_IDX:
			break;
		default:
			/* We don't understand this bit. */
			clear_bit(i, vdev->features);
		}
	}
}


/* return the size of the vring within the virtqueue */
unsigned int virtqueue_get_vring_size(struct virtqueue *_vq)
{

	struct vring_virtqueue *vq = to_vvq(_vq);

	return vq->vring.num;
}
/**********************************************************************************/


