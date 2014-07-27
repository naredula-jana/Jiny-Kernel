#ifndef __JDEVICE_H__
#define __JDEVICE_H__
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
}
#include "file.hh"
class jdriver;
#define MAX_DEVICE_NAME 100
class jdevice: public vinode {

public:
	unsigned char name[MAX_DEVICE_NAME];
	jdriver* driver;

	jdevice(unsigned char *name, int type);
	int read(unsigned long offset, unsigned char *data, int len, int flags);
	int write(unsigned long offset, unsigned char *data, int len, int flags);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);

	/* pci details */
	pci_device_t pci_device;
	int init_pci(uint8_t bus, uint8_t device, uint8_t function);

	void print_stats();
};

class jdriver: public jobject {
public:
	unsigned char *name;
	jdevice *device;

	unsigned long stat_sends,stat_recvs,stat_recv_interrupts,stat_send_interrupts;
	/* TODO : Do not change the order of virtual functions, c++ linking is implemented in handcoded */
	virtual int probe_device(jdevice *dev)=0;
	virtual jdriver *attach_device(jdevice *dev)=0; /* attach the driver by creating a new driver if it is sucessfull*/
	virtual int dettach_device(jdevice *dev)=0;
	virtual int read(unsigned char *buf, int len, int flags)=0;
	virtual int write(unsigned char *buf, int len, int flags)=0;
	virtual void print_stats()=0;
	virtual int ioctl(unsigned long arg1,unsigned long arg2)=0;
};

class virtio_jdriver: public jdriver {
public:
	unsigned long stat_send_kicks;
	unsigned long stat_recv_kicks;
	unsigned char pending_kick_onsend;

	int virtio_create_queue(uint16_t index, int qType);
	void print_stats();

	struct virtqueue *vq[5];
	unsigned long stat_allocs,stat_frees,stat_err_nospace;
};

#define COPY_OBJ(CLASS_NAME,OBJECT_NAME, NEW_OBJ, jdev) jdriver *NEW_OBJ; NEW_OBJ=(jdriver *)ut_calloc(sizeof(CLASS_NAME)); \
	ut_memcpy((unsigned char *)NEW_OBJ,(unsigned char *) (OBJECT_NAME), sizeof(CLASS_NAME)); \
	NEW_OBJ->device = jdev;

class virtio_net_jdriver: public virtio_jdriver {
	int net_attach_device(jdevice *dev);
	int free_send_bufs();
public:
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len, int flags);
	int write(unsigned char *buf, int len, int flags);
	int ioctl(unsigned long arg1,unsigned long arg2);

	wait_queue *send_waitq;
	unsigned char mac[7];
	static int test_k;
};

class virtio_p9_jdriver: public virtio_jdriver {
	int p9_attach_device(jdevice *dev);

public:
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len, int flags);
	int write(unsigned char *buf, int len, int flags);
	int ioctl(unsigned long arg1,unsigned long arg2);
	void *virtio_dev; /* TODO : need to remove later */
};


void register_jdriver(class jdriver *driver);
#endif
