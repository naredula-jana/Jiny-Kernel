/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   x86_64/msix.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
#define DEBUG_ENABLE 1
#include "pci.h"
#include "common.h"
#include "mm.h"
#include "task.h"



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
/***************** MSIX ********************************/
struct msix_table {
	uint32_t lower_addr;
	uint32_t upper_add;
	uint32_t data;
	uint32_t control;
};
struct pcicfg_msix {
	uint16_t msix_ctrl; /* Message Control id */
	uint16_t msix_msgnum; /* total Number of messages */
	uint8_t msix_location; /* Offset of MSI-X capability registers. */
	uint8_t msix_table_bar; /* BAR containing vector table. */
	uint8_t msix_pba_bar; /* BAR containing PBA. */
	uint32_t msix_table_offset;
	uint32_t msix_pba_offset;
	int msix_alloc; /* Number of allocated vectors. */
	int msix_table_len; /* Length of virtual table. */
	unsigned long msix_table_res; /* mmio address */
	struct msix_table *msix_table;
	int isr_vector;
};
/*
 * The address register = 0xfee0100c goes like this:
Bits 31 to 20 = 0xFEE = address is in the local APIC area
Bits 19 to 12 = Destination APIC ID for the IRQ = 1
Bit 3 = RH = Redirection Hint = 1
Bit 2 = DM = Destination Mode (same meaning as for IPIs) = 1
Bits 0 to 1 = Don't care
 */
#define LAPIC_BASE    0xfee00000
static void pci_msix_update_interrupts(struct pcicfg_msix *msix, int mask) {
	uint32_t offset;
	unsigned long address;
	uint32_t data;

	int i;
	if (mask == 1) {
		for (i = 0; i < msix->msix_msgnum; i++) {
			if (!(msix->msix_table[i].control & 0x1))
				msix->msix_table[i].control = msix->msix_table[i].control | 0x1;
		}
		return;
	}

	for (i = 0; i < msix->msix_msgnum; i++) {
		address = LAPIC_BASE;
	//	address = address | 0x1000; /* apic-id=1 as destination, to send to the second cpu */
		msix->msix_table[i].upper_add = address >> 32;
		msix->msix_table[i].lower_addr = address & 0xffffffff;
		msix->msix_table[i].data = msix->isr_vector + i;
		msix->msix_table[i].control = 0;
	}
	return;

}
#define MSI_VECTORS_START 101
static int msi_start_vector = MSI_VECTORS_START;

int read_msi(pci_addr_t *addr, uint8_t pos, pci_bar_t bars[],
		uint32_t bars_total) {
	uint32_t ret;
	uint16_t buf;
	uint32_t val;
	uint32_t bar_offset;
	int i;
	struct pcicfg_msix msix;


	ret = pci_read(addr, pos, 2, &buf);
	ret = pci_read(addr, pos + PCIR_MSIX_CTRL, 2, &msix.msix_ctrl);

	msix.msix_msgnum = (msix.msix_ctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1;
	if (msix.msix_msgnum > 0) {
		msix.isr_vector = msi_start_vector;
		msi_start_vector = msi_start_vector + msix.msix_msgnum;
	} else {
		msix.isr_vector = 0;
	}
	ret = pci_read(addr, pos + PCIR_MSIX_TABLE, 4, &bar_offset);
	msix.msix_table_bar = PCIR_BAR(bar_offset & PCIM_MSIX_BIR_MASK);
	if (bar_offset >= bars_total) return 0;
	msix.msix_table_res = bars[bar_offset].addr;
	msix.msix_table = __va(msix.msix_table_res);

	if ((ret = vm_mmap(0, __va(msix.msix_table_res), 0x1000, PROT_WRITE,
			MAP_FIXED, msix.msix_table_res)) == 0) /* this is for SMP */
	{
		ut_printf("ERROR : PCI mmap fails for \n");
		return 0;
	}

	ret = pci_read(addr, pos + PCIR_MSIX_PBA, 4, &val);
	msix.msix_pba_bar = PCIR_BAR(val & PCIM_MSIX_BIR_MASK);

	pci_msix_update_interrupts(&msix, 1); /* first mask interrupt */

	msix.msix_ctrl = msix.msix_ctrl | 0x8000; // enable msix
	pci_write(addr, pos + PCIR_MSIX_CTRL, 2, &msix.msix_ctrl);
	pci_msix_update_interrupts(&msix, 0);
	msix.msix_ctrl = msix.msix_ctrl | 0x8000; // enable msix
	pci_write(addr, pos + PCIR_MSIX_CTRL, 2, &msix.msix_ctrl);

	DEBUG("MSIX Configured ISR vector:%d  numvector:%d ctrl:%x\n", msix.isr_vector,msix.msix_msgnum,msix.msix_ctrl);
	return msix.isr_vector;
}

int disable_msix(pci_addr_t *addr, uint8_t pos){
	uint16_t msix_ctrl;
	int ret;
    ut_printf(" MSIX Disabling again \n");
	ret = pci_read(addr, pos + PCIR_MSIX_CTRL, 2, &msix_ctrl);
	msix_ctrl = msix_ctrl & 0x7fff; // disable msix
	pci_write(addr, pos + PCIR_MSIX_CTRL, 2, &msix_ctrl);
	return 1;
}

