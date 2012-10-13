/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/pci.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#define DEBUG_ENABLE 1
#include "pci.h"
#include "common.h"
#include "mm.h"
#include "task.h"
void init_pci();


int pci_write(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf){
	uint16_t port;
	uint32_t addr;

	if(d->bus > 255 || d->device > 31 || d->function > 5 || pos > 4096) {
		return -1;
	}

	//  if(alloc_ioport_range( 0xCF8, 8 ) != 0) return -1;
	port = PCI_CONFIG_DATA + (pos & 3);
	addr = PCI_CONF1_MAKE_ADDRESS(d->bus, d->device, d->function, pos);

	outl(PCI_CONFIG_ADDRESS, addr);


	switch (len) {
		case 1:
			outb(port, ((uint8_t *)buf)[0]);
			break;
		case 2:
			outw(port, ((uint16_t *)buf)[0]);
			break;
		case 4:
			outl(port, ((uint32_t *)buf)[0]);
			break;
		default:
			//    free_ioport_range(0xCF8,8);
			return -1;
	}
	// free_ioport_range(0xCF8,8);
	return 0;
}
int pci_read(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf){

	uint16_t port;
	uint32_t addr;

	if(d->bus > 255 || d->device > 31 || d->function > 5 || pos > 4096) {
		return -1;
	}
	//  if(alloc_ioport_range( 0xCF8, 8 ) != 0) return -1;
	port = PCI_CONFIG_DATA + (pos & 3);
	addr = PCI_CONF1_MAKE_ADDRESS(d->bus, d->device, d->function, pos);

	outl(PCI_CONFIG_ADDRESS, addr);

	switch(len) {
		case 1:
			((uint8_t *)buf)[0] = inb( port );
			break;
		case 2:
			((uint16_t *)buf)[0] = inw( port );
			break;
		case 4:
			((uint32_t *)buf)[0] = inl( port );
			break;
		default:
			//free_ioport_range(0xCF8,8);
			return -1;
	}
	// free_ioport_range(0xCF8,8);
	return 0;
}
int pci_generic_read(pci_addr_t *d, uint16_t pos, uint16_t len, void *buf)
{ 
	int c;
	int step;

	while(len > 0) {
		if((pos & 1) && len >= 1) {
			step = 1;
		} else if((pos & 3) && len >= 2) {
			step = 2;
		} else if(len < 2) {
			step = 1;
		} else if(len < 4) {
			step = 2;
		} else {
			step = 4;
		}

		c = pci_read(d, pos, step, buf);

		if( c != 0) {
			return c;
		}
		buf += step;
		pos += step;
		len -= step;
	}

	return 0;
}
static int get_bar(pci_addr_t *addr, int barno, uint32_t *start, uint32_t *len)
{
	int res;
	uint16_t offset;
	uint32_t mask = ~0U;

	// We write all ones to the register and read it.

	offset = PCI_BAR_0 + 4 * barno;

	res = pci_read(addr, offset, 4, start);

	if(res != 0) {
		return -1;
	}

	res = pci_write(addr, offset, 4, &mask);

	if(res != 0) {
		return -1;
	}

	res = pci_read(addr, offset, 4, len);

	if(res != 0) {
		return -1;
	}

	res = pci_write(addr, offset, 4, start);

	if(res != 0) {
		return -1;
	}
	if (*start == 0 ) return 0;
	DEBUG("   barno:%d start :%i len:%i \n",barno,*start,*len);
	return 0;
}
#define XEN_PLATFORM_VENDOR_ID 0x5853
#define XEN_PLATFORM_DEVICE_ID 0x0001
#define MAX_PCI_BARS 100
static pci_bar_t pci_bars[MAX_PCI_BARS];
static int bar_count=0;
int Jcmd_pci_stat(char *arg1 , char *arg2){
	int i;
	for (i=0; i<bar_count; i++){
		if (pci_bars[i].addr==0 ) return;
		ut_printf("name:%s  addr:%x len:%d\n",pci_bars[i].name,pci_bars[i].addr,pci_bars[i].len);
	}
	return 1;
}

#define MSI 1
static int read_dev_conf(uint8_t bus , uint8_t dev,uint8_t func)
{
	pci_dev_header_t header;
	pci_addr_t addr;
	int ret;
	int i,count_start;
	int msi_vector =0;

	addr.bus=bus;
	addr.device=dev;
	addr.function=func;
	header.vendor_id=0;
	header.device_id=0;
	pci_bars[bar_count].addr=0;
	count_start=bar_count;
	ret = pci_generic_read(&addr, 0, sizeof(header), &header);
	//BRK;
	if(ret != 0) {
		return -1;
	}

	if (header.vendor_id != 0xffff)
	{
		DEBUG(" PCI bus:%d devic:%d func:%d  vendor:%x devices:%x int:%x:%x baser:%i \n",bus,dev,func,header.vendor_id,header.device_id,header.interrupt_line,header.interrupt_pin,header.base_address_registers[0]);
		DEBUG("   base addr :%i :%i :%i :%i \n",header.base_address_registers[0],header.base_address_registers[1],header.base_address_registers[2],header.base_address_registers[3]);
		for(i=0; i<5;i++)
		{
			get_bar(&addr,i,&pci_bars[bar_count].addr,&pci_bars[bar_count].len);
			if (pci_bars[bar_count].addr != 0)
			{
				pci_bars[bar_count].len= (~pci_bars[bar_count].len)+1;
				pci_bars[bar_count].name="";
				bar_count++;
			}else
			{
				//break;
			}
		}

#ifdef MSI
		if (header.capabilities_pointer != 0 && header.device_id != 0x1009  ) { // do not msix for p9
			msi_vector=read_msi(&addr, header.capabilities_pointer, &pci_bars[count_start],i);
		}
#endif

		if (header.vendor_id == 0x1af4 && header.device_id==0x1110){
			init_host_shm(&header,&pci_bars[count_start],i,&msi_vector);
		}
#ifdef XEN
		else if (header.vendor_id == XEN_PLATFORM_VENDOR_ID  && header.device_id == XEN_PLATFORM_DEVICE_ID){
			init_xen_pci(&header,&pci_bars[count_start],i);
		}
#endif
#ifdef VIRTIO
#define VIRTIO_PCI_VENDOR_ID 0x1af4
		else if (header.vendor_id == VIRTIO_PCI_VENDOR_ID  && (header.device_id >= 0x1000 && header.device_id <= 0x103f) ){
			int msi_enabled = msi_vector;

			init_virtio_pci(&header,&pci_bars[count_start],i,&msi_vector);
			if (msi_enabled && msi_vector == 0) {
				//disable_msix(&addr,i); NOT working
			}
		}
#endif
	}
	return 1;
}
static int pci_initialised=0;
void init_pci()
{
	int i,j;
	if (pci_initialised == 1) return;
	pci_initialised=1;
	DEBUG(" Scanning PCI devices info started \n");
	for (i = 0; i < 32 ; i++) {
		for (j = 0; j < 32; j++)
			read_dev_conf(i, j, 0);
	}
	DEBUG(" Scanning PCI devices info Ended \n");

	return ;
}
