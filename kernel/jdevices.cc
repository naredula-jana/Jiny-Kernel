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

extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"
}

#include "jdevice.h"

#define MAX_DEVICES 200
class jdevice *jdevice_list[MAX_DEVICES];
static int device_count = 0;
#define MAX_DRIVERS 100
class jdriver *jdriver_list[MAX_DRIVERS];
static int driver_count = 0;

int jdevice::init_pci(uint8_t bus, uint8_t device, uint8_t function) {
	pci_device.pci_addr.bus = bus;
	pci_device.pci_addr.device = device;
	pci_device.pci_addr.function = function;
	pci_dev_header_t *pci_hdr;
	pci_bar_t *bars;
	int ret,len;
	driver = 0;

	ret = pci_generic_read(
			&this->pci_device.pci_addr, 0,
			sizeof(pci_dev_header_t),
			&this->pci_device.pci_header);
	if (ret != 0  || this->pci_device.pci_header.vendor_id
					== 0xffff)
		return JFAIL;
	ut_log("	scan devices %d:%d:%d  %x:%x\n", bus, device, function,
			this->pci_device.pci_header.vendor_id,
			this->pci_device.pci_header.device_id);

	ut_log(" reading pci info : bus:dev:fuc : %x:%x:%x \n",pci_device.pci_addr.bus,pci_device.pci_addr.device,pci_device.pci_addr.function);
	if (read_pci_info_new(&pci_device) != JSUCCESS){
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
		ut_printf("pci bus:dev:func: %2x:%2x:%2x : ", pci_device.pci_addr.bus,
				pci_device.pci_addr.device, pci_device.pci_addr.function);
	}
	if (driver != 0) {
		driver->print_stats();
	}else{
		ut_printf("\n");
	}
}

void register_jdriver(class jdriver *driver) {

	jdriver_list[driver_count] = driver;
	driver_count++;
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
				jdevice_list[device_count] =
						(class jdevice *) ut_malloc(sizeof(class jdevice));
				ut_memset((unsigned char *) jdevice_list[device_count], 0,
						sizeof(class jdevice));
				if (jdevice_list[device_count]->init_pci(i, j, k)==JFAIL){
					ut_free(jdevice_list[device_count]);
					continue;
				}

				jdevice_list[device_count]->name = (unsigned char *)"pci";
				/* attach the device to the know driver */
				for (d = 0; d < driver_count; d++) {
					if (jdriver_list[d]->probe_device(
							jdevice_list[device_count]) == JSUCCESS) {
						jdevice_list[device_count]->driver = jdriver_list[d];
						jdriver_list[d]->attach_device(
								jdevice_list[device_count]);
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

void init_jdevices(unsigned long unused_arg1) {
	device_count = 0;

	init_p9_jdriver();
	init_net_jdriver();
	init_keyboard_jdriver();
	init_serial_jdriver();
	jdevice_list[0] = (class jdevice *) ut_malloc(sizeof(class jdevice));
	jdevice_list[1] = (class jdevice *) ut_malloc(sizeof(class jdevice));
	jdevice_list[0]->name = (unsigned char *)"/dev/keyboard";
	jdevice_list[1]->name = (unsigned char *)"/dev/serial";
	device_count=2;

	scan_pci_devices();
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
