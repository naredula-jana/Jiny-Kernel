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
#include "mach_dep.h"

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
/* TODO : if d->function are greater then 5 then some how the pcu device like vitio_net does not recvies the interrupts */
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
#if 0
static int read_dev_conf(uint8_t bus , uint8_t dev_no,uint8_t func ,device_t *dev)
{
	pci_bar_t *pci_bars=&dev->pci_bars[0];
	int bar_count=0;
	pci_dev_header_t *header=&dev->pci_hdr;
	pci_addr_t *addr=&dev->pci_addr;
	int ret;
	int i,count_start;

	addr->bus=bus;
	addr->device=dev_no;
	addr->function=func;
	header->vendor_id=0;
	header->device_id=0;
	pci_bars[bar_count].addr=0;
	count_start=bar_count;
	ret = pci_generic_read(addr, 0, sizeof(pci_dev_header_t), header);

	if(ret != 0) {
		return 0;
	}

	if (header->vendor_id != 0xffff && header->vendor_id == dev->pci_hdr.vendor_id && header->device_id == dev->pci_hdr.device_id)
	{
		DEBUG(" PCI bus:%d devic:%d func:%d  vendor:%x devices:%x int:%x:%x baser:%i \n",bus,dev,func,header->vendor_id,header->device_id,header->interrupt_line,header->interrupt_pin,header->base_address_registers[0]);
		DEBUG("   base addr :%i :%i :%i :%i \n",header->base_address_registers[0],header->base_address_registers[1],header->base_address_registers[2],header->base_address_registers[3]);
		for(i=0; i<5;i++)
		{
			get_bar(addr,i,&pci_bars[bar_count].addr,&pci_bars[bar_count].len);
			if (pci_bars[bar_count].addr != 0)
			{
				pci_bars[bar_count].len= (~pci_bars[bar_count].len)+1;
				pci_bars[bar_count].name="";
				bar_count++;
			}
		}
		dev->pci_bar_count=bar_count;
		return 1;
	}
	return 0;
}
#endif
int read_pci_info_new(pci_device_t *dev)
{
	pci_bar_t *pci_bars=&dev->pci_bars[0];
	pci_addr_t *addr = &dev->pci_addr;
	int bar_count=0;
	uint8_t bus , dev_no, func;
	pci_dev_header_t *header=&dev->pci_header;
	//pci_addr_t *addr=&dev->pci_addr;
	int ret;
	int i,count_start;

	bus = addr->bus;
	dev_no= addr->device;
	func = addr->function;
#if 0
	addr->bus=bus;
	addr->device=dev_no;
	addr->function=func;
#endif

	header->vendor_id=0;
	header->device_id=0;
	pci_bars[bar_count].addr=0;
	count_start=bar_count;
	ret = pci_generic_read(addr, 0, sizeof(pci_dev_header_t), header);

	if(ret != 0) {
		return JFAIL;
	}

	if (header->vendor_id != 0xffff && header->vendor_id == dev->pci_header.vendor_id && header->device_id == dev->pci_header.device_id)
	{
		DEBUG(" PCI bus:%d devic:%d func:%d  vendor:%x devices:%x int:%x:%x baser:%i \n",bus,dev,func,header->vendor_id,header->device_id,header->interrupt_line,header->interrupt_pin,header->base_address_registers[0]);
		DEBUG("   base addr :%i :%i :%i :%i \n",header->base_address_registers[0],header->base_address_registers[1],header->base_address_registers[2],header->base_address_registers[3]);
		for(i=0; i<5;i++)
		{
			get_bar(addr,i,&pci_bars[bar_count].addr,&pci_bars[bar_count].len);
			if (pci_bars[bar_count].addr != 0)
			{
				pci_bars[bar_count].len= (~pci_bars[bar_count].len)+1;
				pci_bars[bar_count].name="";
				bar_count++;
			}
		}
		dev->pci_bar_count=bar_count;
		return JSUCCESS;
	}
	return JFAIL;
}


