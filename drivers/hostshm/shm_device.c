#include "common.h"
#include "task.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"

pci_dev_header_t host_shm_pci_hdr;
pci_bar_t host_shm_pci_bar[4];
unsigned long g_hostShmLen = 0;
unsigned long g_hostShmPhyAddr = 0;


queue_t g_hfs_waitqueue;
static void host_shm_interrupt(registers_t regs) {
	uint32_t  *p, ret;

	p = (unsigned char *) HOST_SHM_CTL_ADDR + 4;
	*p = 0; /* reset the irq by resetting the status  */
	ret = sc_wakeUp(&g_hfs_waitqueue); /* wake all the waiting processes */
	ut_printf(" GOT HOST SHM INTERRUPT  :%x:  wakedup :%d \n", p, ret);
}
extern init_HostFs();
int init_host_shm(pci_dev_header_t *pci_hdr, pci_bar_t bars[], uint32_t len, int *msi_vector) {
	uint32_t i, *p;
	long ret;
	host_shm_pci_hdr = *pci_hdr;

	ut_printf(" Initialising HOST SHM .. \n");
	for (i = 0; i < len && i < 4; i++) {
		host_shm_pci_bar[i] = bars[i];
		ut_printf("Host_shm bar addr :%x  len: %x \n", host_shm_pci_bar[i].addr,
				host_shm_pci_bar[i].len);
	}
	if (bars[0].addr != 0) {
		if ((ret = vm_mmap(0, HOST_SHM_CTL_ADDR, 0x1000, PROT_WRITE, MAP_FIXED,
				bars[0].addr)) == 0) {
			ut_printf(
					"ERROR : mmap fails for Host_ctl addr :%x len:%x ret:%x \n",
					bars[0].addr, bars[0].len, ret);
			return 0;
		} else {
			p = (uint32_t *) HOST_SHM_CTL_ADDR;
			*p = 0xffffffff; /* set the proper mask */
			// g_hostShmLen=bars[0].len;
		}
	}
	if (bars[2].addr == 0)
		i=1; /* No MSI */
	else
      i=2;		/* MSI present in bar-1 */
	if (bars[i].addr != 0) {
		if ((ret = vm_mmap(0, HOST_SHM_ADDR, bars[i].len, PROT_WRITE, MAP_FIXED,
				bars[i].addr)) == 0) {
			ut_printf(
					"ERROR : mmap fails for Host_shm addr :%x len:%x ret:%x \n",
					bars[i].addr, bars[i].len, ret);
			return 0;
		} else {
			g_hostShmPhyAddr = bars[i].addr;
			g_hostShmLen = bars[i].len;
		}
	}else{
		return 0;
	}
	sc_register_waitqueue(&g_hfs_waitqueue,"Hfs");
	if (pci_hdr->interrupt_line > 0) {
		int k;
#if 1
			p =(uint32_t *) HOST_SHM_CTL_ADDR;
			*p=0xffffffff;
			p=p+1;
			k=*p;
#endif
		ut_printf(" Interrupt NUMBER : %i k:%d \n", pci_hdr->interrupt_line,k);
		ar_registerInterrupt(32 + pci_hdr->interrupt_line, host_shm_interrupt,
				"host_shm");
	}
	init_HostFs();
	if (*msi_vector > 0) {
		ar_registerInterrupt(*msi_vector , host_shm_interrupt,"hostshm_msi");
	}

	return 1;
}

