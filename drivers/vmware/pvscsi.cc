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

struct cdb_rwdata_10{
    u8 command;
    u8 flags;
    u32 lba;
    u8 resreved_06;
    u16 count;
    u8 reserved_09;
    u8 pad[6];
} __attribute__((packed));

class pvscsi_jdriver: public jdiskdriver {
	int disk_attach_device(jdevice *dev);
	int setup_rings();
	void write_cmd_desc(u32 cmd, const void *desc, size_t len);
	void fill_req(struct PVSCSIRingReqDesc *req,void *cdbcmd, unsigned long addr,int data_len);


    struct PVSCSIRingsState *ring_state;
    struct PVSCSIRingReqDesc *ring_reqs;
    struct PVSCSIRingCmpDesc *ring_cmps;
    int req_numpages;
    int cmp_numpages;
    u64 context_id;
    u8 dev_target; /* target id */
    unsigned char *debug_scsi_type;

public:
	unsigned long mmio_bar;
	void send_req(int type, unsigned long addr, int len,u32 sector);

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
#define CDB_CMD_TEST_UNIT_READY  0x00
#define CDB_CMD_READ_CAPACITY 0x25
#define CDB_CMD_READ_10 0x28

#define SIMPLE_QUEUE_TAG 0x20
#define MASK(n) ((1 << (n)) - 1)
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
	INIT_LOG("Ring setup  req entries: %d cmp entries:%d \n",MASK(ring_state->reqNumEntriesLog2),MASK(ring_state->cmpNumEntriesLog2));
	return 1;
}
void pvscsi_jdriver::print_stats(unsigned char *arg1,
		unsigned char *arg2) {
}

int pvscsi_jdriver::MaxBufsSpace() {
	return 1;
}


static __inline uint32_t bswap32(uint32_t __x) {
	return (__x >> 24) | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000)
			| (__x << 24);
}
void pvscsi_jdriver::fill_req(struct PVSCSIRingReqDesc *req,void *cdbcmd, unsigned long addr, int data_len)
{
	ut_log(" SCSI Req: id:%d  type:%s  addr:%x len:%d\n",context_id,debug_scsi_type,addr,data_len);
	debug_scsi_type = " NA ";
	req->context = context_id;
	context_id++;
    req->bus = 0;
    req->target = dev_target;
    ut_memset(req->lun, 0, sizeof(req->lun));
    req->lun[1] = 0;
    req->senseLen = 0;
    req->senseAddr = 0;
    req->cdbLen = 16;
    req->vcpuHint = 0;
    ut_memcpy((unsigned char *)req->cdb, (unsigned char *)cdbcmd, 16);
    req->tag = SIMPLE_QUEUE_TAG;
    req->flags = PVSCSI_FLAG_CMD_DIR_TOHOST ;
    if (addr != 0){
    	req->dataLen = data_len;
    	req->dataAddr = __pa(addr);
    }else{
    	req->dataLen = 0;
    	req->dataAddr = 0;
    }
}

void pvscsi_jdriver::send_req(int type, unsigned long addr, int len, u32 sector) {
	struct cdb_rwdata_10 cmd;
	struct PVSCSIRingReqDesc *req;
	unsigned char *mmio_addr = mmio_bar;
	int data_len=blk_size;
    u32 req_entries = ring_state->reqNumEntriesLog2;


	ut_log("SCSCI req addr:%x len:%d  sector:%d \n", addr, len,sector);
	req = ring_reqs + (ring_state->reqProdIdx & MASK(req_entries));
	ut_memset((unsigned char *) &cmd, 0, sizeof(cmd));
	cmd.command = type;
	if (type == CDB_CMD_READ_10){
		cmd.lba =bswap32( sector);
		cmd.count = len/blk_size;
		if (cmd.count ==0){
			cmd.count =1;
		}
		data_len = cmd.count*blk_size;
		debug_scsi_type = "cmd_Read";
	}else if (type==CDB_CMD_TEST_UNIT_READY){
		cmd.lba = 0;
		cmd.count = 0;
		debug_scsi_type = "cmd_unit_ready";
	}else if (type==CDB_CMD_READ_CAPACITY){
		debug_scsi_type = "cmd_capacity";
		data_len = len;
	}
	fill_req(req, &cmd, addr,data_len);

	ring_state->reqProdIdx++;
	mmio_addr = mmio_addr + PVSCSI_REG_OFFSET_KICK_RW_IO;
	*mmio_addr = 0;
}
void pvscsi_jdriver::burst_send(struct struct_mbuf *mbuf, int list_len) {
	int i;

	for (i = 0; i < list_len; i++) {
		struct virtio_blk_req *req;
		unsigned long addr = mbuf[i].buf;
		int len = mbuf[i].len;

		req = addr;
		send_req(CDB_CMD_READ_10, (unsigned long) &req->data[0], req->len,req->sector);
	}
	return;
}
int pvscsi_jdriver::burst_recv(struct struct_mbuf *mbuf_list, int list_len) {
	int i,id;
	u32 req_entries = ring_state->reqNumEntriesLog2;
    u32 cmp_entries = ring_state->cmpNumEntriesLog2;

	if (ring_state->cmpProdIdx == ring_state->cmpConsIdx){
		return 0;
	}
	i=0;
	while ((ring_state->cmpProdIdx != ring_state->cmpConsIdx) && (i<list_len)){
		struct PVSCSIRingReqDesc *req;
		struct PVSCSIRingCmpDesc  *res;

		req = ring_reqs+((ring_state->cmpConsIdx) & MASK(req_entries));
		res = ring_cmps+((ring_state->cmpConsIdx) & MASK(cmp_entries));
		if (req->dataAddr != 0 && req->dataLen >= blk_size){
			mbuf_list[i].buf = __va(req->dataAddr) - 32;
			ut_log(" SCSI removed from queue addr:%x \n",mbuf_list[i].buf);
			req->dataAddr = 0;
			i++;
		}
		ring_state->cmpConsIdx++;
	}
	return i;
}
pvscsi_jdriver *debug_scsi=0;
extern "C"{
unsigned char test_data[(2*PAGE_SIZE) +2];
void Jcmd_scsi(unsigned char *arg1,unsigned char *arg2){
	ut_memset(&test_data[0],2*PAGE_SIZE,0);
	debug_scsi->send_req(CDB_CMD_READ_10, (unsigned long)&test_data[0],PAGE_SIZE/4,1);
}
}
int pvscsi_jdriver::probe_device(class jdevice *jdev) {
	if ((jdev->pci_device.pci_header.vendor_id == PCI_VENDOR_ID_VMWARE)
			&& ((jdev->pci_device.pci_header.device_id == PCI_DEVICE_ID_VMWARE_PVSCSI))) {
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

struct cdbres_read_capacity {
    u32 sectors;
    u32 blksize;
}  __attribute__((packed));
struct cdbres_read_capacity scsi_devs[5];
int pvscsi_jdriver::init_device(jdevice *jdev) {
	int i;

	INIT_LOG(" pvscsi: Bar0:%x  Bar1:%x \n",device->pci_device.pci_bars[0].addr,device->pci_device.pci_bars[1].addr);
	mmio_bar = vm_create_kmap("pvscsi_bar0",12*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_FIXED,device->pci_device.pci_bars[0].addr);
	write_cmd_desc(PVSCSI_CMD_ADAPTER_RESET, NULL, 0);
	setup_rings();
	blk_size = 512;
	disk_size = 102400*1024;
	context_id = 1;
    for (i = 0; i < 1; i++){
    	dev_target = i;
    	send_req(CDB_CMD_TEST_UNIT_READY, 0, 0,0);
    	ut_memset((unsigned char *)&scsi_devs[i],0,sizeof(struct cdbres_read_capacity));
    	send_req(CDB_CMD_READ_CAPACITY, (unsigned long)&scsi_devs[i], sizeof(struct cdbres_read_capacity),0);
    }
    dev_target = 0;


	return 0;
}

jdriver *pvscsi_jdriver::attach_device(class jdevice *jdev) {
	int i;

	COPY_OBJ(pvscsi_jdriver, this, new_obj, jdev);
	((pvscsi_jdriver *) new_obj)->init_device(jdev);

	((pvscsi_jdriver *) new_obj)->waitq = jnew_obj(wait_queue,
			"waitq_pvscsi_disk", 0);
	//spin_lock_init(&((virtio_disk_jdriver *)new_obj)->io_lock);
	init_tarfs((jdriver *) new_obj);
	debug_scsi = new_obj;
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
