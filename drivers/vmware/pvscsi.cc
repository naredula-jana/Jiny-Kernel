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
//#define DEBUG_SCSI 1

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
	unsigned long mmio_bar;
    struct PVSCSIRingsState *ring_state;
    struct PVSCSIRingReqDesc *ring_reqs;
    struct PVSCSIRingCmpDesc *ring_cmps;
    int req_numpages;
    int cmp_numpages;
    u64 context_id;
    u8 dev_target; /* target id */
    unsigned char *debug_scsi_type;
#define MAX_PVSCSI_DESCS 500
    unsigned char *req_mbufs[MAX_PVSCSI_DESCS];

	int disk_attach_device(jdevice *dev);
	int setup_rings();
	void write_cmd_desc(u32 cmd, const void *desc, size_t len);
	void fill_req(struct PVSCSIRingReqDesc *req,void *cdbcmd, unsigned long addr,int data_len);
	int get_response();
	int send_req(int type, unsigned long addr, int len,u32 sector, unsigned long mbuf_buf);
	void kick_io();

	unsigned long stat_success_sends,stat_err_sends,stat_err_outoforder,stat_kicks,stat_error_hoststatus,stat_error_scsistatus;

public:
	pvscsi_jdriver(class jdevice *jdev);
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
	if (device ==0){
		ut_printf(" Driver not attached to device \n");
		return;
	}
	ut_printf(" sends success:%d  reqProdId:%d reqConsId:%d kicks:%d disk_copys:%d\n",stat_success_sends,ring_state->reqProdIdx,ring_state->reqConsIdx,stat_kicks,stat_diskcopy_reqs);
	ut_printf(" Error: outoforder:%d send:%d hoststatus:%d scsistatus:%d\n",stat_err_outoforder,stat_err_sends,stat_error_hoststatus,stat_error_scsistatus);
}

int pvscsi_jdriver::MaxBufsSpace() {
	int ret;
	u32 req_entries = MASK(ring_state->reqNumEntriesLog2);

	ret = req_entries - (ring_state->reqProdIdx - ring_state->reqConsIdx);
	if (ret > 0){
		return ret-1;
	}
	return 0;
}

static __inline uint32_t bswap32(uint32_t __x) {
	return (__x >> 24) | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000)
			| (__x << 24);
}
static __inline uint16_t bswap16(uint16_t __x) {
	return (__x >> 8) | (__x << 8 & 0xff00) ;
}
void pvscsi_jdriver::fill_req(struct PVSCSIRingReqDesc *req,void *cdbcmd, unsigned long addr, int data_len){
#if DEBUG_SCSI
	ut_log(" SCSI Req: id:%d  type:%s  addr:%x len:%d\n",context_id,debug_scsi_type,addr,data_len);
#endif
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
void pvscsi_jdriver::kick_io(){
	unsigned char *mmio_addr = mmio_bar;

	mmio_addr = mmio_addr + PVSCSI_REG_OFFSET_KICK_RW_IO;
	*mmio_addr = 0;
	stat_kicks++;
}
int pvscsi_jdriver::send_req(int type, unsigned long addr, int len, u32 sector,unsigned long mbuf_addr) {
	struct cdb_rwdata_10 cmd;
	struct PVSCSIRingReqDesc *req;
	unsigned char *mmio_addr = mmio_bar;
	int data_len=blk_size;
    u32 req_entries = ring_state->reqNumEntriesLog2;

#if DEBUG_SCSI
	ut_log("SCSCI req addr:%x len:%d  sector:%d blk_size:%d\n", addr, len,sector,blk_size);
#endif
	req = ring_reqs + (ring_state->reqProdIdx & MASK(req_entries));
	if (req->dataAddr != 0){
		return JFAIL;
	}
	ut_memset((unsigned char *) &cmd, 0, sizeof(cmd));
	cmd.command = type;
	if (type == CDB_CMD_READ_10){
		cmd.lba =bswap32( sector);
		cmd.count = bswap16(len/blk_size);
		 if (cmd.count ==0){
			cmd.count =bswap16(1);
		 }

		data_len = len;
		debug_scsi_type = "cmd_Read";
		//ut_log(" sector:%x no of sector: %x \n",cmd.lba,cmd.count);
	}else if (type==CDB_CMD_TEST_UNIT_READY){
		cmd.lba = 0;
		cmd.count = 0;
		debug_scsi_type = "cmd_unit_ready";
	}else if (type==CDB_CMD_READ_CAPACITY){
		debug_scsi_type = "cmd_capacity";
		data_len = len;
	}
	req_mbufs[(ring_state->reqProdIdx & MASK(req_entries))] = mbuf_addr;
	fill_req(req, &cmd, addr,data_len);
	ring_state->reqProdIdx++;

	stat_success_sends++;
	return JSUCCESS;
}
/* TODO:  burst_send and burst_recv need share the lock */
int pvscsi_jdriver::burst_send(struct struct_mbuf *mbuf, int list_len) {
	int i;
	int ret = 0;

	for (i = 0; i < list_len; i++) {
		struct virtio_blk_req *req;
		unsigned long addr = mbuf[i].buf;
		int len = mbuf[i].len;
		unsigned char *data;

		req = addr;
		if (req->user_data != 0) {
			data = req->user_data;
		} else {
			data = &req->data[0];
		}
		if (send_req(CDB_CMD_READ_10, (unsigned long) data, req->len,
				req->sector, addr) == JFAIL) {
			stat_err_sends++;
			goto last;
		}
		mbuf[i].buf = 0;
		ret = i + 1;
	}
	last: if (ret > 0) {
		kick_io();
	}
	return ret;
}
/*
 *  scsi Status codes
 */
#if 0
#define GOOD                 0x00
#define CHECK_CONDITION      0x01
#define CONDITION_GOOD       0x02
#define BUSY                 0x04
#define INTERMEDIATE_GOOD    0x08
#define INTERMEDIATE_C_GOOD  0x0a
#define RESERVATION_CONFLICT 0x0c
#define COMMAND_TERMINATED   0x11
#define QUEUE_FULL           0x14

#define GOOD                 0x00
#define CHECK_CONDITION      0x02
#define CONDITION_GOOD       0x04
#define BUSY                 0x08
#define INTERMEDIATE_GOOD    0x10
#define INTERMEDIATE_C_GOOD  0x14
#define RESERVATION_CONFLICT 0x18
#define COMMAND_TERMINATED   0x22
#define TASK_SET_FULL        0x28
#define ACA_ACTIVE           0x30
#define TASK_ABORTED         0x40
#endif

int pvscsi_jdriver::burst_recv(struct struct_mbuf *mbuf_list, int list_len) {
	int i,id;
	u32 req_entries = ring_state->reqNumEntriesLog2;
    u32 cmp_entries = ring_state->cmpNumEntriesLog2;

	if (ring_state->cmpProdIdx == ring_state->cmpConsIdx){
		return 0;
	}
	i=0;
	while ((ring_state->cmpProdIdx != ring_state->cmpConsIdx) && (i<list_len)){
		struct PVSCSIRingReqDesc *req=0;
		struct PVSCSIRingCmpDesc  *res;
		int id,count;

		count=0;
		res = ring_cmps+((ring_state->cmpConsIdx) & MASK(cmp_entries));
		for (id=ring_state->cmpConsIdx  ; count< (MASK(req_entries)+4); id++,count++){
			req = ring_reqs+((id) & MASK(req_entries));
			if (req->context == res->context){
				break;
			}
			stat_err_outoforder++;
			req=0;
		}
		if (req==0){
			break;
		}
		mbuf_list[i].buf =0;
		mbuf_list[i].ret_code = 0;
		if (req->dataAddr != 0){
			mbuf_list[i].buf = req_mbufs[((id) & MASK(req_entries))];
#if DEBUG_SCSI
			ut_log(" SCSI removed from queue context:%d  addr:%x: return:%d :%d\n",req->context,mbuf_list[i].buf,res->hostStatus,res->scsiStatus);
#endif
			mbuf_list[i].ret_code = 0;
			if (res->hostStatus != 0){
				stat_error_hoststatus++;
			}
			if (res->scsiStatus != 0){
				stat_error_scsistatus++;
				ut_log(" SCSI error :%d(%x) \n",res->scsiStatus,res->scsiStatus);
				//mbuf_list[i].ret_code = res->scsiStatus;
			}
			i++;
		}
		req->dataAddr = 0;
		ring_state->cmpConsIdx++;
	}
	return i;
}
pvscsi_jdriver *debug_scsi=0;
extern "C"{
unsigned char test_data[(2*PAGE_SIZE) +2];
void Jcmd_scsi(unsigned char *arg1,unsigned char *arg2){
	//ut_memset(&test_data[0],2*PAGE_SIZE,0);
	//debug_scsi->send_req(CDB_CMD_READ_10, (unsigned long)&test_data[0],PAGE_SIZE/4,1);
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

int pvscsi_jdriver::get_response() {
	struct struct_mbuf mbuf[4];
	int ret;
	unsigned long start_tsc, curr_tsc;
	start_tsc = ar_read_tsc();
	while (1) {
		ret = burst_recv(&mbuf[0], 4);
		if (ret > 0) {
			return ret;
		}
		curr_tsc = ar_read_tsc();
		if ((curr_tsc-start_tsc) >  5000000){
			return 0;
		}
	}
	return 0;
}
static int pvscsi_disk_interrupt(void *private_data) {
	jdevice *dev;
	//pvscsi_jdriver *driver = (virtio_disk_jdriver *) private_data;
	return 1;
}
int pvscsi_jdriver::init_device(jdevice *jdev) {
	int i,msi_vector;
	struct cdbres_read_capacity cdrres_cap;

	INIT_LOG(" pvscsi: Bar0:%x  Bar1:%x \n",device->pci_device.pci_bars[0].addr,device->pci_device.pci_bars[1].addr);
	mmio_bar = vm_create_kmap("pvscsi_bar0",12*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_FIXED,device->pci_device.pci_bars[0].addr);
	write_cmd_desc(PVSCSI_CMD_ADAPTER_RESET, NULL, 0);
	setup_rings();
	blk_size = 512;
	disk_size = 0;
	context_id = 1;

#if 0
	//pci_enable_msix(device->pci_device.pci_bars[0].addr+(PVSCSI_MEM_SPACE_MSIX_TABLE_PAGE*PAGE_SIZE), &device->pci_device.msix_cfg,
	//		device->pci_device.pci_header.capabilities_pointer);
	msi_vector = pci_read_msi_withoutbars(&device->pci_device.pci_addr,
			&device->pci_device.pci_header, &device->pci_device.msix_cfg,device->pci_device.pci_bars[0].addr+(PVSCSI_MEM_SPACE_MSIX_TABLE_PAGE*PAGE_SIZE));
	if (msi_vector > 0) {
		pci_enable_msix(&device->pci_device.pci_addr,&device->pci_device.msix_cfg,
						device->pci_device.pci_header.capabilities_pointer);
	}
#else
	unsigned char *mmio_addr = mmio_bar;
	uint32_t *intr_addr,val_intr;

	mmio_addr = mmio_addr + PVSCSI_REG_OFFSET_INTR_STATUS;
	intr_addr = (uint32_t *)mmio_addr;
	val_intr = *intr_addr ;
	*intr_addr = 1;

	u32 intr_bits;
	intr_bits = PVSCSI_INTR_CMPL_MASK;
	mmio_addr = mmio_addr + PVSCSI_REG_OFFSET_INTR_MASK;
	intr_addr = (uint32_t *)mmio_addr;
	*intr_addr = intr_bits;

	ut_log(" New pcscsi inteerupt registration :%d \n",val_intr);
	ar_registerInterrupt(32 + jdev->pci_device.pci_header.interrupt_line,
			pvscsi_disk_interrupt, "pvscsi_irq", (void *) this);
#endif

    for (i = 0; i < 1; i++){
    	dev_target = i;
    	send_req(CDB_CMD_TEST_UNIT_READY, 0, 0,0,0);
    	kick_io();
    	get_response();
    	ut_memset((unsigned char *)&cdrres_cap,0,sizeof(struct cdbres_read_capacity));
    	send_req(CDB_CMD_READ_CAPACITY, (unsigned long)&cdrres_cap, sizeof(struct cdbres_read_capacity),0,0);
    	kick_io();
    	if (get_response()> 0){
    		disk_size = bswap32(cdrres_cap.sectors)*512;
    		blk_size = bswap32(cdrres_cap.blksize);
    		INIT_LOG(" pvscsi: disksize:%d blk_size:%d\n",bswap32(cdrres_cap.sectors)*512,bswap32(cdrres_cap.blksize));
    		break;
    	}else{

    	}
    }
    INIT_LOG("PVSCSI initialization completed disk_size:%d blksize:%d \n",disk_size,blk_size);
	return 0;
}
pvscsi_jdriver::pvscsi_jdriver(class jdevice *jdev) {
	name = "pvscsi";
	device = jdev;
	if (jdev != 0) {
		init_device(jdev);
		//req_blk_size = blk_size;
		req_blk_size = PAGE_SIZE;
		waitq = jnew_obj(wait_queue, "waitq_pvscsi_disk", 0);
		init_tarfs((jdriver *) this,1);
	}
}
jdriver *pvscsi_jdriver::attach_device(class jdevice *jdev) {
	pvscsi_jdriver *new_obj = jnew_obj(pvscsi_jdriver,jdev);
	debug_scsi = new_obj;
	return (jdriver *) new_obj;
}
static int debug_seq=0;
int pvscsi_jdriver::read(unsigned char *buf, int len, int offset,
		int read_ahead) {
	int ret;
	if (disk_size ==0){
		return 0;
	}
#if 0 /*  debugging purpose */
	if (len == PAGE_SIZE){
		ut_memset(buf,0,PAGE_SIZE);
	}
#endif
	ret = disk_io(DISK_READ, buf, len, offset, read_ahead, this);
#if 0
ut_log("%d: read len :  %d offset:%d read_ahead:%d err:%d\n",debug_seq,len, offset,read_ahead,stat_error_scsistatus);
debug_seq++;
#endif
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
	disk_jdriver = jnew_obj(pvscsi_jdriver,0);
	disk_jdriver->name = (unsigned char *) "disk_pvscsi_driver";
	register_jdriver(disk_jdriver);
}

}
