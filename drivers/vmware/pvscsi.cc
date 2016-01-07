/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/pvscsi.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "mach_dep.h"
}
#include "pvscsi.h"

class pvscsi_jdriver: public jdiskdriver {
	int disk_attach_device(jdevice *dev);
	int setup_rings();
	void write_cmd_desc(u32 cmd, const void *desc, size_t len);

    struct PVSCSIRingsState *ring_state;
    struct PVSCSIRingReqDesc *ring_reqs;
    struct PVSCSIRingCmpDesc *ring_cmps;
    int req_numpages;
    int cmp_numpages;

public:
	unsigned long mmio_bar;

	int MaxBufsSpace();
	void burst_send(struct struct_mbuf *mbuf, int len);
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
void pvscsi_jdriver::write_cmd_desc(u32 cmd, const void *desc, size_t len)
{
    const u32 *ptr = desc;
    u32 *base_ptr = mmio_bar;
    size_t i;

    len /= sizeof(*ptr);
    *base_ptr = cmd;
    //writel(iobase + PVSCSI_REG_OFFSET_COMMAND, cmd);
    base_ptr=base_ptr+1;
    for (i = 0; i < len; i++){
        *base_ptr = ptr[i];
        //writel(iobase + PVSCSI_REG_OFFSET_COMMAND_DATA, ptr[i]);
    }
}
int pvscsi_jdriver::setup_rings() {
	struct PVSCSICmdDescSetupRings cmd = { 0, };

	req_numpages = 4;
	cmp_numpages = 4;
	ring_reqs = ut_calloc(req_numpages * PAGE_SIZE);
	ring_cmps = ut_calloc(cmp_numpages * PAGE_SIZE);
	ring_state = ut_calloc(PAGE_SIZE);

	cmd.reqRingNumPages = 1;
	cmd.cmpRingNumPages = 1;
	cmd.ringsStatePPN = __pa(ring_state) >> PAGE_SHIFT;
	cmd.reqRingPPNs[0] = __pa(ring_reqs) >> PAGE_SHIFT;
	cmd.cmpRingPPNs[0] = __pa(ring_cmps) >> PAGE_SHIFT;

	write_cmd_desc(PVSCSI_CMD_SETUP_RINGS, &cmd, sizeof(cmd));
	return 1;
}
void pvscsi_jdriver::print_stats(unsigned char *arg1,
		unsigned char *arg2) {
}

int pvscsi_jdriver::MaxBufsSpace() {
	return 1;
}
int pvscsi_jdriver::burst_recv(struct struct_mbuf *mbuf_list, int len) {
	return 1;
}
void pvscsi_jdriver::burst_send(struct struct_mbuf *mbuf, int len) {
	int ret;

	return;
}

int pvscsi_jdriver::probe_device(class jdevice *jdev) {
	if ((jdev->pci_device.pci_header.vendor_id == PCI_VENDOR_ID_VMWARE)
			&& ((jdev->pci_device.pci_header.device_id
					== PCI_DEVICE_ID_VMWARE_PVSCSI))) {
		ut_log("		Matches the pvscsi disk Probe :%d\n",
				jdev->pci_device.pci_header.device_id);
		return JSUCCESS;
	}
	return JFAIL;
}
int pvscsi_jdriver::dettach_device(jdevice *jdev) {
	/*TODO:  Need to free the resources */
	return JFAIL;
}
int pvscsi_jdriver::init_device(jdevice *jdev) {
	int i;

	INIT_LOG(" pvscsi: Bar0:%x  Bar1:%x \n",device->pci_device.pci_bars[0].addr,device->pci_device.pci_bars[1].addr);
	mmio_bar = vm_create_kmap("pvscsi_bar0",12*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_FIXED,device->pci_device.pci_bars[0].addr);
	write_cmd_desc(PVSCSI_CMD_ADAPTER_RESET, NULL, 0);
	setup_rings();
	return 0;
}
pvscsi_jdriver *debug_scsi=0;
jdriver *pvscsi_jdriver::attach_device(class jdevice *jdev) {
	int i;

	COPY_OBJ(pvscsi_jdriver, this, new_obj, jdev);
	((pvscsi_jdriver *) new_obj)->init_device(jdev);
#if 0
	for (i = 0; i < 5; i++) {
		if (disk_drivers[i] == 0) {
			disk_drivers[i] = (jdriver *) new_obj;
			break;
		}
	}
#endif
	((pvscsi_jdriver *) new_obj)->waitq = jnew_obj(wait_queue,
			"waitq_pvscsi_disk", 0);
	//spin_lock_init(&((virtio_disk_jdriver *)new_obj)->io_lock);
	init_tarfs((jdriver *) new_obj);
	return (jdriver *) new_obj;
}
int pvscsi_jdriver::read(unsigned char *buf, int len, int offset,
		int read_ahead) {
	int ret;
//ut_log(" read len :  %d offset:%d \n",len, offset);
	ret = disk_io(DISK_READ, buf, len, offset, read_ahead, this);
	return ret;
}
int pvscsi_jdriver::write(unsigned char *buf, int len, int offset) {
	int ret;

	ret = disk_io(DISK_WRITE, buf, len, offset, 0, this);
	return ret;
}
int pvscsi_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	if (arg1 == IOCTL_DISK_SIZE) {
		return disk_size;
	}
	return JSUCCESS;
}
static pvscsi_jdriver *disk_jdriver;
extern "C" {
int init_pvscsi_driver(){
	/* init disk */
	disk_jdriver = jnew_obj(pvscsi_jdriver);
	disk_jdriver->name = (unsigned char *) "disk_pvscsi_driver";
	register_jdriver(disk_jdriver);
}

}
