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
//#define DEBUG_ENABLE 1
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

int read_pci_info_new(pci_device_t *dev)
{
	pci_bar_t *pci_bars=&dev->pci_bars[0];
	pci_addr_t *addr = &dev->pci_addr;
	int bar_count=0;
	uint8_t bus , dev_no, func;
	pci_dev_header_t *header=&dev->pci_header;
	int ret;
	int i,count_start;

	bus = addr->bus;
	dev_no= addr->device;
	func = addr->function;

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
		ut_log(" 	PCI bus:%d devic:%d func:%d  vendor:%x devices:%x int:%d:%x baser:%i \n",bus,dev,func,header->vendor_id,header->device_id,(32+header->interrupt_line),header->interrupt_pin,header->base_address_registers[0]);
		INIT_LOG("		base addr :%i :%i :%i :%i \n",header->base_address_registers[0],header->base_address_registers[1],header->base_address_registers[2],header->base_address_registers[3]);
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

#if 0

#define ATA0_MSTR 0x01f0
#define ATA0_SLVE 0x03f0

#define ATA1_MSTR 0x0170
#define ATA1_SLVE 0x0370


enum ide_register {
IDE_DATA = 0x00,
IDE_ERROR = 0x01,
IDE_FEATURES = 0x01,
IDE_SECCOUNT0 = 0x02,
IDE_LBA0 = 0x03,
IDE_LBA1 = 0x04,
IDE_LBA2 = 0x05,
IDE_HDDEVSEL = 0x06,
IDE_COMMAND = 0x07,
IDE_STATUS = 0x07,
IDE_SECCOUNT1 = 0x08,
IDE_LBA3 = 0x09,
IDE_LBA4 = 0x0a,
IDE_LBA5 = 0x0b,
IDE_CONTROL = 0x0c,
IDE_ALTSTATUS = 0x0c,
IDE_DEFVADDRESS = 0x0d
};


enum ide_status {
STATUS_BSY = 0x80,
STATUS_DRDY = 0x40,
STATUS_DRQ = 0x08,
STATUS_DF = 0x20,
STATUS_ERR = 0x01
};

enum ide_control {
CONTROL_RESET = 0x04,
CONTROL_NIEN = 0x02
};
enum ide_command {
CMD_READ_PIO = 0x20,
CMD_READ_PIO_EXT = 0x24,
CMD_WRITE_PIO = 0x30,
CMD_WRITE_PIO_EXT = 0x34,
CMD_FLUSH = 0xe7,
CMD_FLUSH_EXT = 0xea,
CMD_IDENTIFY = 0xec
};
int
ide_poll()
{
	unsigned char mask, match;
	int status;
	mask=(STATUS_BSY|STATUS_DRDY);
	match=STATUS_DRDY;
	//while (((status = inb(ATA0_MSTR+IDE_STATUS)) & mask) != match){
	while (((status = inb(ATA0_MSTR+IDE_STATUS)) ) != match){
		sc_sleep(5000);
		ut_printf("new  status: %x match:%x compar:%x \n",status,match,status&mask);
	}
	inb(ATA0_MSTR+IDE_ALTSTATUS);
	return status;
}
unsigned char PRIMARY_MASTER = 0xe0 | (0<<4);
void Jcmd_read(){
	ut_printf(" RESETTING  ide  primary master: %x\n",PRIMARY_MASTER);
	 outb(CONTROL_RESET, ATA0_MSTR + IDE_CONTROL); // reset the device
	 sc_sleep(5000);
	 outb(0, ATA0_MSTR + IDE_CONTROL);
	 sc_sleep(500);

	 /* Select primary master */
	 outb(PRIMARY_MASTER,  ATA0_MSTR + IDE_HDDEVSEL);
	auto sel = inb(ATA0_MSTR +IDE_HDDEVSEL);
	if (sel != PRIMARY_MASTER) /* No drive */{
		ut_printf(" Fail to find the IDE drive :  %x \n",sel);
		return ;
	}
	ut_printf(" SUCESS found the IDE drive \n");

	 /* Disable intterupts on   master/slave */
	 outb(CONTROL_NIEN, ATA0_MSTR+IDE_CONTROL);
	 outb(CONTROL_NIEN, ATA1_MSTR+IDE_CONTROL);

	 /* Send CMD_IDENTIFY */
	 outb(CMD_IDENTIFY, ATA0_MSTR+IDE_COMMAND);

	ut_printf(" starting ide read\n");
	 ide_poll();
   outb(0x40, ATA0_MSTR + 0x0006); // Select Master
   outb(0x00, ATA0_MSTR + 0x0002); // sectorcount high byte
   outb(0x00, ATA0_MSTR + 0x0003); // LBA4
   outb(0x00, ATA0_MSTR + 0x0004); // LBA5
   outb(0x00, ATA0_MSTR + 0x0005); // LBA6
      outb(0x01, ATA0_MSTR + 0x0002); // sectorcount low byte
   outb(0x00, ATA0_MSTR + 0x0003); // LBA1
   outb(0x00, ATA0_MSTR + 0x0004); // LBA2
   outb(0x00, ATA0_MSTR + 0x0005); // LBA3
   outb(0x24, ATA0_MSTR + 0x0007); // read pio cmd
  // outb(0x20, ATA0_MSTR + 0x0007); // read  cmd
   int i;
   uint8_t data;
   ut_printf(" Before poll ......\n");
   ide_poll();
   ut_printf(" Before read ......\n");
   for(i = 0; i < 512; i++){
      data = inb(ATA0_MSTR); // read byte
      ut_printf("%d: data read after wait : %x\n",i,data);
     // vga_write(COLOR_BLACK, COLOR_CYAN, data, 800 + i);
   }
}
#endif
