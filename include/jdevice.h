#ifndef __JDEVICE_H__
#define __JDEVICE_H__
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
}
class jdriver;

class jdevice {
public:
	unsigned char *name;
	jdriver* driver;

	/* pci details */
	pci_device_t pci_device;
	int init_pci(uint8_t bus, uint8_t device, uint8_t function);

	void print_stats();
};

class jdriver {
public:
	unsigned char *name;
	jdevice *device;

	int stat_sends,stat_recvs,stat_recv_interrupts;
	/* TODO : Do not change the order of virtual functions, c++ linking is implemented in handcoded */
	virtual int probe_device(jdevice *dev)=0;
	virtual int attach_device(jdevice *dev)=0;
	virtual int dettach_device(jdevice *dev)=0;
	virtual int read(unsigned char *buf, int len)=0;
	virtual int write(unsigned char *buf, int len)=0;
	virtual int print_stats()=0;
};

class virtio_jdriver: public jdriver {
public:
	int stat_send_kicks;
	int stat_recv_kicks;

	int virtio_create_queue(uint16_t index, int qType);
	int print_stats();
	struct virtqueue *vq[5];
};


class virtio_net_jdriver: public virtio_jdriver {
public:
	int probe_device(jdevice *dev);
	int attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);

	unsigned char mac[7];
};

class virtio_p9_jdriver: public virtio_jdriver {
public:
	int probe_device(jdevice *dev);
	int attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);
	void *virtio_dev; /* TODO : need to remove later */
};


void register_jdriver(class jdriver *driver);
#endif
