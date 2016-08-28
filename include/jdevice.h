#ifndef __JDEVICE_H__
#define __JDEVICE_H__
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
}
#include "file.hh"
#include "virtio_queue.hh"
class jdriver;
class virtio_queue;
#define MAX_DEVICE_NAME 100
class jdevice: public vinode {

public:
	unsigned char name[MAX_DEVICE_NAME];
	jdriver* driver;

	jdevice(unsigned char *name, int type);
	int read(unsigned long offset, unsigned char *data, int len, int flags, int opt_flags);
	int write(unsigned long offset, unsigned char *data, int len, int flags);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);

	/* pci details */
	pci_device_t pci_device;
	int init_pci(uint8_t bus, uint8_t device, uint8_t function);

	void print_stats(unsigned char *arg1,unsigned char *arg2);
};

#define VQTYPE_RECV 1
#define VQTYPE_SEND 2

class jdriver: public jobject {
public:
	unsigned char *name;
	jdevice *device;
	int instances;

	unsigned long stat_sends,stat_recvs,stat_recv_interrupts,stat_send_interrupts;
	/* TODO : Do not change the order of virtual functions, c++ linking is implemented in handcoded */
	virtual int probe_device(jdevice *dev)=0;
	virtual jdriver *attach_device(jdevice *dev)=0; /* attach the driver by creating a new driver if it is sucessfull*/
	virtual int dettach_device(jdevice *dev)=0;
	virtual int read(unsigned char *buf, int len, int flags, int opt_flags)=0;
	virtual int write(unsigned char *buf, int len, int flags)=0;
	virtual void print_stats(unsigned char *arg1,unsigned char *arg2)=0;
	virtual int ioctl(unsigned long arg1,unsigned long arg2)=0;
};

class jnetdriver: public jdriver {
public:
#define MAX_BUF_LIST_SIZE 64
	struct struct_mbuf recv_mbuf_list[MAX_BUF_LIST_SIZE];
	struct struct_mbuf send_mbuf_list[MAX_BUF_LIST_SIZE];
	int send_mbuf_start;
	int send_mbuf_len;
	struct struct_mbuf temp_mbuf_list[MAX_BUF_LIST_SIZE]; /* used to remove the send bufs to free */

	virtual int burst_recv(int total_pkts)=0;
	virtual int burst_send()=0;
	virtual int check_for_pkts()=0;
};

#define COPY_OBJ(CLASS_NAME,OBJECT_NAME, NEW_OBJ, jdev) jdriver *NEW_OBJ; NEW_OBJ=(jdriver *)ut_calloc(sizeof(CLASS_NAME)); \
	ut_memcpy((unsigned char *)NEW_OBJ,(unsigned char *) (OBJECT_NAME), sizeof(CLASS_NAME)); \
	NEW_OBJ->device = jdev;

class virtio_net_jdriver: public jnetdriver {
	int net_attach_device();
	int free_send_bufs();
	int fill_empty_buffers(net_virtio_queue *queue);

public:
#define MAX_VIRT_QUEUES 10
	struct virt_queue{
		 net_virtio_queue *recv,*send;
		int pending_send_kick;
	}queues[MAX_VIRT_QUEUES];
	net_virtio_queue *control_q;

	uint16_t max_vqs;
	uint32_t send_count;
	uint32_t current_send_q;
	spinlock_t virtionet_lock;
	unsigned char pending_kick_onsend;

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

	wait_queue *send_waitq;
	unsigned char mac[7];
	int recv_interrupt_disabled;
	int send_kick_needed;
};

class jdiskdriver: public jdriver {
public:
	int interrupts_disabled;
	unsigned long disk_size,blk_size;
	int req_blk_size; /* maximum size of the request */
	uint16_t max_vqs;
	wait_queue *waitq;
	unsigned long stat_diskcopy_reqs;
	virtual int burst_send(struct struct_mbuf *mbuf, int len)=0;
	virtual int burst_recv(struct struct_mbuf *mbuf, int len)=0;
	virtual int MaxBufsSpace()=0;
	int init_device(jdevice *dev);
};
extern "C" {
extern int init_tarfs(jdriver *driver_arg,int enable_read_ahead);
}
int disk_io(int type, unsigned char *buf, int len, int offset, int read_ahead, jdiskdriver *driver) ;
#define DISK_READ 0
#define DISK_WRITE 1
#define IOCTL_DISK_SIZE 1
class virtio_disk_jdriver: public jdiskdriver {
public:
	struct virt_queue{
		disk_virtio_queue *send;
	}queues[MAX_VIRT_QUEUES];
	uint16_t max_vqs;
	unsigned char *unfreed_req;

	virtio_disk_jdriver(class jdevice *jdev);
	int MaxBufsSpace();
	int burst_send(struct struct_mbuf *mbuf, int len);
	int burst_recv(struct struct_mbuf *mbuf, int len);
	int init_device(jdevice *dev);

	void print_stats(unsigned char *arg1,unsigned char *arg2);
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len, int flags, int opt_flags);
	int write(unsigned char *buf, int len, int flags);
	int ioctl(unsigned long arg1,unsigned long arg2);
};

class virtio_memballoon_jdriver: public jdriver {
public:

	virtio_queue *send_queues[2];
	virtio_queue *recv_queue;
	uint16_t max_vqs;

	virtio_memballoon_jdriver(class jdevice *jdev);

//	int init_device(jdevice *dev);

	void print_stats(unsigned char *arg1,unsigned char *arg2);
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len, int flags, int opt_flags);
	int write(unsigned char *buf, int len, int flags);
	int ioctl(unsigned long arg1,unsigned long arg2);
};

class virtio_p9_jdriver: public jdriver {
	int p9_attach_device(jdevice *dev);

public:
	virtio_queue *vq[5];

	atomic_t stat_send_kicks;
	atomic_t stat_recv_kicks;
	unsigned long stat_allocs,stat_frees,stat_err_nospace;

	void print_stats(unsigned char *arg1,unsigned char *arg2);
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len, int flags, int opt_flags);
	int write(unsigned char *buf, int len, int flags);
	int ioctl(unsigned long arg1,unsigned long arg2);
	void *virtio_dev; /* TODO : need to remove later */
};


void register_jdriver(class jdriver *driver);
#define MAX_DISK_DEVICES 5
extern "C" {

}
#endif
