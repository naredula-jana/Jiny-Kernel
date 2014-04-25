/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   jdevice.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#include "jdevice.h"

#define MAX_DEVICES 200
static class jdevice *jdevice_list[MAX_DEVICES];
static int device_count = 0;
#define MAX_DRIVERS 100
class jdriver *jdriver_list[MAX_DRIVERS];
static int driver_count = 0;
jdevice::jdevice(){

}
int jdevice::init_pci(uint8_t bus, uint8_t device, uint8_t function) {
	pci_device.pci_addr.bus = bus;
	pci_device.pci_addr.device = device;
	pci_device.pci_addr.function = function;
	pci_dev_header_t *pci_hdr;
	pci_bar_t *bars;
	int ret,len;
	driver = 0;

	ret = pci_generic_read(&this->pci_device.pci_addr, 0, sizeof(pci_dev_header_t), &this->pci_device.pci_header);
	if (ret != 0 || this->pci_device.pci_header.vendor_id == 0xffff)
		return JFAIL;
	ut_log("	scan devices %d:%d:%d  %x:%x\n", bus, device, function, this->pci_device.pci_header.vendor_id,
			this->pci_device.pci_header.device_id);

	ut_log(" reading pci info : bus:dev:fuc : %x:%x:%x \n", pci_device.pci_addr.bus, pci_device.pci_addr.device,
			pci_device.pci_addr.function);
	if (read_pci_info_new(&pci_device) != JSUCCESS) {
		ut_log("ERROR: reading pci device failed\n");
		return JFAIL;
	}

	pci_hdr = &pci_device.pci_header;
	bars = &pci_device.pci_bars[0];
	len = pci_device.pci_bar_count;
	ut_log(" succeded reading pci info : bus:dev:fuc : %x:%x:%x \n",pci_device.pci_addr.bus,pci_device.pci_addr.device,pci_device.pci_addr.function);

	if (bars[0].addr != 0) {
		pci_device.pci_ioaddr = bars[0].addr - 1;
		pci_device.pci_iolen = bars[0].len;
		pci_device.pci_mmio = bars[1].addr;
		pci_device.pci_mmiolen = bars[1].len;
	} else {
		ut_log(" ERROR in initializing PCI driver %x : %x \n", bars[0].addr,
				bars[1].addr);
		return JFAIL;
	}
	return JSUCCESS;
}
void jdevice::print_stats() {
	if (ut_strcmp(name, (unsigned char *) "pci") != 0) {
		ut_printf("%s: ", name);
	} else {
		ut_printf("pci bus:dev:func: %2x:%2x:%2x : ", pci_device.pci_addr.bus, pci_device.pci_addr.device,
				pci_device.pci_addr.function);
	}
	if (driver != 0) {
		driver->print_stats();
	} else {
		ut_printf(" - No driver");
	}
	ut_printf("\n");
}
int jdevice::read(unsigned long unused, unsigned char *buf, int len){
	if (driver != 0){
		return driver->read(buf,len);
	}
	return -1;
}
int jdevice::write(unsigned long unused, unsigned char *buf, int len){
	if (driver != 0){
		return driver->write(buf,len);
	}
	return -1;
}
int jdevice::ioctl(unsigned long arg1,unsigned long arg2){
	if (driver != 0){
		return driver->ioctl(arg1,arg2);
	}
	return -1;
}
int jdevice::close(){
	if (driver != 0){
		/* TODO : ioctl function need to implement in driver */
		return -1;
	}
	return -1;
}
static void *vptr_jdevice[7] = {
		(void *) &jdevice::read, (void *) &jdevice::write,
		(void *) &jdevice::close, (void *) &jdevice::ioctl, 0 };
int jdevice::init(unsigned char *dev_name) {
	int i;
	void **p = (void **) this;
	*p = &vptr_jdevice[0];
	ut_snprintf(name,MAX_DEVICE_NAME,"%s",dev_name);
	return JSUCCESS;
}
void register_jdriver(class jdriver *driver) {

	jdriver_list[driver_count] = driver;
	driver_count++;
}

void *operator new(int sz) {
    void *obj = ut_calloc(sz);
    return obj;
}
/*********************************************************************************/
extern "C" {

static int scan_pci_devices() {
	int i, j, k, d;
	int ret;
#define MAX_BUS 32
#define MAX_PCI_DEV 32
#define MAX_PCI_FUNC 32

	for (i = 0; i < MAX_BUS && i < 2; i++) {
		for (j = 0; j < MAX_PCI_DEV; j++) {
			for (k = 0; k < MAX_PCI_FUNC; k++) {
				if (device_count >= (MAX_DEVICES - 1))
					return JSUCCESS;
				//jdevice_list[device_count] = (class jdevice *) ut_calloc(sizeof(class jdevice));
				jdevice_list[device_count] = new jdevice();
				if (jdevice_list[device_count]->init_pci(i, j, k) == JFAIL) {
					ut_free(jdevice_list[device_count]);
					continue;
				}

				jdevice_list[device_count]->init((unsigned char *) "pci");
				/* attach the device to the know driver */
				for (d = 0; d < driver_count; d++) {
					if (jdriver_list[d]->probe_device(jdevice_list[device_count]) == JSUCCESS) {
						jdevice_list[device_count]->driver = jdriver_list[d]->attach_device(jdevice_list[device_count]);
						break;
					}
				}
				device_count++;
			}
		}
	}
	return JSUCCESS;
}
extern void init_p9_jdriver();
extern void init_net_jdriver();
extern void init_keyboard_jdriver();
extern void init_serial_jdriver();
struct jdevice keyboard_device,serial_in_device;
struct jdevice vga_device,serial_out_device;
void init_jdevices(unsigned long unused_arg1) {
	device_count = 0;
	int d,k;

	init_p9_jdriver();
	init_net_jdriver();
	init_keyboard_jdriver();
	init_serial_jdriver();
	ut_memset((unsigned char *)&keyboard_device,0,sizeof(class jdevice));
	ut_memset((unsigned char *)&vga_device,0,sizeof(class jdevice));
	ut_memset((unsigned char *)&serial_in_device,0,sizeof(class jdevice));
	ut_memset((unsigned char *)&serial_out_device,0,sizeof(class jdevice));
	jdevice_list[0] = &keyboard_device;
	jdevice_list[1] = &vga_device;
	jdevice_list[2] = &serial_in_device;
	jdevice_list[3] = &serial_out_device;
	jdevice_list[0]->init((unsigned char *)"/dev/keyboard");
	jdevice_list[1]->init((unsigned char *)"/dev/vga");
	jdevice_list[2]->init((unsigned char *)"/dev/serial_in");
	jdevice_list[3]->init((unsigned char *)"/dev/serial_out");
	keyboard_device.file_type = IN_FILE;
	vga_device.file_type = OUT_FILE;
	serial_in_device.file_type = IN_FILE;
	serial_out_device.file_type = OUT_FILE;
	device_count=4;

	/* attach the device to the know driver */
	for (k = 0; k < device_count; k++) {
		for (d = 0; d < driver_count; d++) {
			if (jdriver_list[d]->probe_device(jdevice_list[k]) == JSUCCESS) {
				jdevice_list[k]->driver = jdriver_list[d]->attach_device(
						jdevice_list[k]);
				break;
			}
		}
	}

	scan_pci_devices();
}
void *get_keyboard_device(int device_type,int file_type){
	if (device_type == DEVICE_KEYBOARD){
		if (file_type == IN_FILE)
			return (void *)&keyboard_device;
		else
			return (void *)&vga_device;
	}else{
		if (file_type == IN_FILE)
			return (void *)&serial_in_device;
		else
			return (void *)&serial_out_device;
	}
}
void Jcmd_jdevices() {
	int i;

	for (i = 0; i < device_count; i++) {
		jdevice_list[i]->print_stats();
	}
}
void *test_addr[5];
void Jcmd_virttest() {

	jdriver_list[0]->print_stats();
}
extern "C" void __cxa_pure_virtual() { while (1); }  /* TODO : to avoid compilation error */
}
