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


static int pci_write(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf)
{
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
static int pci_read(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf){

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
int print_pci(char *arg1 , char *arg2){
	int i;
	for (i=0; i<bar_count; i++){
		if (pci_bars[i].addr==0 ) return;
		ut_printf("name:%s  addr:%x len:%d\n",pci_bars[i].name,pci_bars[i].addr,pci_bars[i].len);
	}
	return 1;
}
/***********************
 * TODO : to implement MSI need to read the capabilities
 */
#if 1
struct msix_table {
	uint32_t lower_addr;
	uint32_t upper_add;
	uint32_t data;
	uint32_t control;
};
struct pcicfg_msix {
    uint16_t	msix_ctrl;	/* Message Control */
    uint16_t	msix_msgnum;	/* Number of messages */
    uint8_t	msix_location;	/* Offset of MSI-X capability registers. */
    uint8_t	msix_table_bar;	/* BAR containing vector table. */
    uint8_t	msix_pba_bar;	/* BAR containing PBA. */
    uint32_t	msix_table_offset;
    uint32_t	msix_pba_offset;
    int		msix_alloc;	/* Number of allocated vectors. */
    int		msix_table_len;	/* Length of virtual table. */
    unsigned long msix_table_res; /* mmio address */
    struct msix_table *msix_table;

#if 0
    struct msix_table_entry *msix_table; /* Virtual table. */
    struct msix_vector *msix_vectors;	/* Array of allocated vectors. */
    struct resource *msix_table_res;	/* Resource containing vector table. */
    struct resource *msix_pba_res;	/* Resource containing PBA. */
#endif
};
#define	PCIR_MSIX_CTRL		0x2
#define	PCIR_MSIX_TABLE		0x4
#define	PCIR_MSIX_PBA		0x8

#define	PCIM_MSIXCTRL_MSIX_ENABLE	0x8000
#define	PCIM_MSIXCTRL_FUNCTION_MASK	0x4000
#define	PCIM_MSIXCTRL_TABLE_SIZE	0x07FF
#define	PCIM_MSIX_BIR_MASK		0x7

#define	PCIR_BARS	0x10
#define	PCIR_BAR(x)		(PCIR_BARS + (x) * 4)


#define	FIRST_MSI_INT	256
#define	NUM_MSI_INTS	512

#define MSI_VECTORS_START 60
#define LAPIC_BASE    0xfee00000
void
pci_msix_update_interrupts(struct pcicfg_msix *msix, int mask)
{
	uint32_t offset;
	unsigned long address;
	uint32_t data;

	int i;
	if (mask == 1) {
		for (i = 0; i < msix->msix_msgnum; i++) {
			if (!(msix->msix_table[i].control & 0x1))
				msix->msix_table[i].control = msix->msix_table[i].control
						| 0x1;
		}
		return;
	}
#if 0
	for (i = 0; i < msix->msix_msgnum; i++) {
		address = LAPIC_BASE;
		msix->msix_table[i].upper_add = address >> 32;
		msix->msix_table[i].lower_addr = address & 0xffffffff;
		msix->msix_table[i].data = MSI_VECTORS_START + i;
		msix->msix_table[i].control = 0;
	}
	return;
#endif
	uint32_t *msix_table;
	msix_table =  __va(msix->msix_table_res);
    for (i=0; i<msix->msix_msgnum; i++){
    	//msix_table =msix_table + (i)*16;

    	address = LAPIC_BASE;
    	data=MSI_VECTORS_START+i;
    	*msix_table = address & 0xffffffff;
    	msix_table++;
    	*msix_table = address >> 32;
    	msix_table++;
    	data=MSI_VECTORS_START+i;
    //	*msix_table = 0x4000|data; //message Data
    	*msix_table = data; //message Data
     	msix_table++;
    	*msix_table = 0; // vector control
     	msix_table++;
    }
#if 0
	offset = msix->msix_table_offset + index * 16;
	bus_write_4(msix->msix_table_res, offset, address & 0xffffffff);
	bus_write_4(msix->msix_table_res, offset + 4, address >> 32);
	bus_write_4(msix->msix_table_res, offset + 8, data);

	/* Enable MSI -> HT mapping. */
	pci_ht_map_msi(dev, address);
#endif
}
struct pcicfg_msix msix;
static int read_msi(pci_addr_t *addr,uint8_t pos,int bar_start) {
	uint32_t  ret;
	uint16_t buf;
	uint32_t val;
	uint32_t bar_offset;
	int i;

	ret = pci_read(addr, pos, 2, &buf);
	ret = pci_read(addr, pos+PCIR_MSIX_CTRL, 2, &msix.msix_ctrl);

	msix.msix_msgnum = (msix.msix_ctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1;

	ret = pci_read(addr, pos+PCIR_MSIX_TABLE, 4, &bar_offset);
	msix.msix_table_bar = PCIR_BAR(bar_offset & PCIM_MSIX_BIR_MASK);
	msix.msix_table_res = pci_bars[bar_start+bar_offset].addr;
	msix.msix_table = __va(msix.msix_table_res);
	if ((ret=vm_mmap(0,__va(msix.msix_table_res) ,0x1000,PROT_WRITE,MAP_FIXED,msix.msix_table_res)) == 0) /* this is for SMP */
	{
		ut_printf("ERROR : PCI mmap fails for \n");
		return 0;
	}
	ret = pci_read(addr, pos+PCIR_MSIX_PBA, 4, &val);
	msix.msix_pba_bar = PCIR_BAR(val & PCIM_MSIX_BIR_MASK);

	pci_msix_update_interrupts(&msix,1); /* first mask interrupt */

	ut_printf("msixctrl:%x \n",msix.msix_ctrl);
	msix.msix_ctrl = msix.msix_ctrl | 0x8000; // enable msix
	pci_write(addr, pos+PCIR_MSIX_CTRL, 2, &msix.msix_ctrl);

	pci_msix_update_interrupts(&msix,0);

	msix.msix_ctrl = msix.msix_ctrl | 0x8000; // enable msix
	pci_write(addr, pos+PCIR_MSIX_CTRL, 2, &msix.msix_ctrl);

	ut_printf("MSIX Configured bytes  :%x  :%x val=%x \n",ret,buf,val);
}
#endif

static int read_dev_conf(uint8_t bus , uint8_t dev,uint8_t func)
{
	pci_dev_header_t header;
	pci_addr_t addr;
	int ret;
	int i,count_start;

	addr.bus=bus;
	addr.device=dev;
	addr.function=func;
	header.vendor_id=0;
	header.device_id=0;
	pci_bars[bar_count].addr=0;
	count_start=bar_count;
	ret = pci_generic_read(&addr, 0, sizeof(header), &header);

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
				break;
			}
		}
#ifdef MSI
		if (header.capabilities_pointer != 0) {
			read_msi(&addr, header.capabilities_pointer, count_start);
		}
#endif
		if (header.vendor_id == 0x1af4 && header.device_id==0x1110){
			init_host_shm(&header,&pci_bars[count_start],3);
		}
#ifdef XEN
		else if (header.vendor_id == XEN_PLATFORM_VENDOR_ID  && header.device_id == XEN_PLATFORM_DEVICE_ID){
			init_xen_pci(&header,&pci_bars[count_start],3);
		}
#endif
#ifdef VIRTIO
#define VIRTIO_PCI_VENDOR_ID 0x1af4
		else if (header.vendor_id == VIRTIO_PCI_VENDOR_ID  && (header.device_id >= 0x1000 && header.device_id <= 0x103f) ){
			int msi_vector =0;
			if (header.capabilities_pointer !=0){ // msi enabled
#ifdef MSI
				msi_vector=MSI_VECTORS_START;
#endif
			}
			init_virtio_pci(&header,&pci_bars[count_start],3,msi_vector);


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
