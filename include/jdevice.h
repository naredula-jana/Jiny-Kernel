#ifndef __JDEVICE_H__
#define __JDEVICE_H__

class jdriver;

class jdevice {
public:
	char *name;
	jdriver* driver;

	/* pci details */
	pci_device_t pci_device;

	int read_pci();
	void init(uint8_t bus, uint8_t device, uint8_t function);
	void print_stats();
};

class jdriver {
	/* The below function are added because c++ linking problem, unable to generate virtual function table while linking*/
	int (*func_probe)(jdevice *dev);
	int (*func_attach)(jdevice *dev);
	int (*func_stat)(jdriver *dev);
public:
	char *name;
	jdevice *device;
	int init_func(int (*func_probe)(jdevice *dev), int (*func_attach)(jdevice *dev), int (*func_stat)(jdriver *dev));
	void print_stats();

	int probe_device(jdevice *dev);
	int attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
};

class virtio_jdriver: public jdriver {
public:
	int stat_send_kicks,stats_sends;
	int stat_recvs,stat_recv_interrupts,stat_recv_kicks;

	int virtio_create_queue(uint16_t index, int qType);
	struct virtqueue *vq[5];
};


class virtio_net_jdriver: public virtio_jdriver {
public:
	unsigned char mac[7];
};

class virtio_p9_jdriver: public virtio_jdriver {
public:
	void *virtio_dev; /* TODO : need to remove later */
};


void register_jdriver(class jdriver *driver);
#endif
