#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"

pci_dev_header_t host_shm_pci_hdr;
pci_bar_t host_shm_pci_bar[4];
unsigned long g_hostShmLen=0;
extern struct wait_struct g_hfs_waitqueue;
static void host_shm_interrupt(registers_t regs)
{
	uint32_t  i,*p,ret;

	p=(unsigned char *)HOST_SHM_CTL_ADDR+4;
       	*p=0; /* reset the irq by resetting the status  */
	ret=sc_wakeUp(&g_hfs_waitqueue,NULL); /* wake all the waiting processes */	
	ut_printf(" GOT HOST SHM INTERRUPT  :%x:  wakedup :%d \n",p,ret);
}
int init_host_shm(pci_dev_header_t *pci_hdr,pci_bar_t bars[], uint32_t len)
{
	uint32_t  i,*p;
	host_shm_pci_hdr=*pci_hdr;

	ut_printf(" Initialising HOST SHM .. \n");
	for (i=0; i<len && i<4;i++)
	{
		host_shm_pci_bar[i]=bars[i];
		ut_printf("Host_shm bar addr :%x  len: %x \n",host_shm_pci_bar[i].addr,host_shm_pci_bar[i].len);
	}	
        if (bars[0].addr !=0)
        {
                if (vm_mmap(0,HOST_SHM_CTL_ADDR ,0x1000,PROT_WRITE,MAP_FIXED,bars[0].addr)==0)
                {
                        ut_printf("ERROR : mmap fails for Host_ctl addr :%x len:%x \n",bars[0].addr,bars[0].len);
                        return 0;
                }else
                {
			 p=(unsigned char *)HOST_SHM_CTL_ADDR;
        		 *p=0xffffffff; /* set the proper mask */
                       // g_hostShmLen=bars[0].len;
                }
        }
	if (bars[2].addr !=0)
	{
		if (vm_mmap(0,HOST_SHM_ADDR ,bars[2].len,PROT_WRITE,MAP_FIXED,bars[2].addr)==0)
		{
			ut_printf("ERROR : mmap fails for Host_shm addr :%x len:%x \n",bars[2].addr,bars[2].len);
			return 0;
		}else
		{
			g_hostShmLen=bars[2].len;
			pc_init(HOST_SHM_ADDR,bars[2].len);
		}
	}
	if (pci_hdr->interrupt_line > 0)
	{
		ut_printf(" Interrupt number : %i \n",pci_hdr->interrupt_line);
		ar_registerInterrupt(32+pci_hdr->interrupt_line, host_shm_interrupt);
	}
	return 1;
}

