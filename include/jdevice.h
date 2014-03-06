#ifndef __JDEVICE_H__
#define __JDEVICE_H__
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
}
#include "../fs/file.h"
class jdriver;

class jdevice: public vinode {

public:
	unsigned char *name;
	jdriver* driver;

	jdevice();
	int read(unsigned long offset, unsigned char *data, int len);
	int write(unsigned long offset, unsigned char *data, int len);
	int close();
	int ioctl(unsigned long arg1,unsigned long arg2);

	/* pci details */
	pci_device_t pci_device;
	int init_pci(uint8_t bus, uint8_t device, uint8_t function);
	int init(unsigned char *name);

	void print_stats();
};

class jdriver {
public:
	unsigned char *name;
	jdevice *device;

	int stat_sends,stat_recvs,stat_recv_interrupts;
	/* TODO : Do not change the order of virtual functions, c++ linking is implemented in handcoded */
	virtual int probe_device(jdevice *dev)=0;
	virtual jdriver *attach_device(jdevice *dev)=0; /* attach the driver by creating a new driver if it is sucessfull*/
	virtual int dettach_device(jdevice *dev)=0;
	virtual int read(unsigned char *buf, int len)=0;
	virtual int write(unsigned char *buf, int len)=0;
	virtual int print_stats()=0;
	virtual int ioctl(unsigned long arg1,unsigned long arg2)=0;
};

class virtio_jdriver: public jdriver {
public:
	int stat_send_kicks;
	int stat_recv_kicks;

	int virtio_create_queue(uint16_t index, int qType);
	int print_stats();

	struct virtqueue *vq[5];
};

#define COPY_OBJ(CLASS_NAME,OBJECT_NAME, NEW_OBJ, jdev) jdriver *NEW_OBJ; NEW_OBJ=(jdriver *)ut_calloc(sizeof(CLASS_NAME)); \
	ut_memcpy((unsigned char *)NEW_OBJ,(unsigned char *) (OBJECT_NAME), sizeof(CLASS_NAME)); \
	NEW_OBJ->device = jdev;

class virtio_net_jdriver: public virtio_jdriver {
	int net_attach_device(jdevice *dev);
public:
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);
	int ioctl(unsigned long arg1,unsigned long arg2);

	unsigned char mac[7];
};

class virtio_p9_jdriver: public virtio_jdriver {
	int p9_attach_device(jdevice *dev);

public:
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);
	int ioctl(unsigned long arg1,unsigned long arg2);
	void *virtio_dev; /* TODO : need to remove later */


};


void register_jdriver(class jdriver *driver);
#endif
