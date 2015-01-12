#ifndef __PCI_H__
#define __PCI_H__

#include "common.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_CONF1_MAKE_ADDRESS(b, d, f, p)           \
  0x80000000            |                            \
  (((p) & 0xF00) << 16) |                            \
  ((f) << 8)            |                            \
  ((d) << 11)           |                            \
  ((b) << 16)           |                            \
  ((p) & 0xFC)


/* Capability lists */

#define PCI_CAP_LIST_ID     0   /* Capability ID */
#define  PCI_CAP_ID_PM      0x01    /* Power Management */
#define  PCI_CAP_ID_AGP     0x02    /* Accelerated Graphics Port */
#define  PCI_CAP_ID_VPD     0x03    /* Vital Product Data */
#define  PCI_CAP_ID_SLOTID  0x04    /* Slot Identification */
#define  PCI_CAP_ID_MSI     0x05    /* Message Signalled Interrupts */
#define  PCI_CAP_ID_CHSWP   0x06    /* CompactPCI HotSwap */
#define  PCI_CAP_ID_PCIX    0x07    /* PCI-X */
#define  PCI_CAP_ID_HT      0x08    /* HyperTransport */
#define  PCI_CAP_ID_VNDR    0x09    /* Vendor specific */
#define  PCI_CAP_ID_DBG     0x0A    /* Debug port */
#define  PCI_CAP_ID_CCRC    0x0B    /* CompactPCI Central Resource Control */
#define  PCI_CAP_ID_SHPC    0x0C    /* PCI Standard Hot-Plug Controller */
#define  PCI_CAP_ID_SSVID   0x0D    /* Bridge subsystem vendor/device ID */
#define  PCI_CAP_ID_AGP3    0x0E    /* AGP Target PCI-PCI bridge */
#define  PCI_CAP_ID_EXP     0x10    /* PCI Express */
#define  PCI_CAP_ID_MSIX    0x11    /* MSI-X */
#define PCI_CAP_LIST_NEXT   1   /* Next capability in the list */
#define PCI_CAP_FLAGS       2   /* Capability defined flags (16 bits) */
#define PCI_CAP_SIZEOF      4


#define PCI_BAR_0       0x10
typedef struct __pci_dev_header_t {

  uint16_t vendor_id;
  uint16_t device_id;

  uint16_t command;
  uint16_t status;

  uint8_t revision_id;

  uint8_t prog_if_id;
  uint8_t subclass_id;
  uint8_t class_id;

  uint8_t cacheline_size;
  uint8_t latency_timer;
  uint8_t header_type;
  uint8_t bist;

  uint32_t base_address_registers[6];

  uint32_t cardbus_cis_pointer;

  uint16_t subsys_vendor_id;
  uint16_t subsys_id;

  uint32_t expansion_rom_base_address;

  uint8_t capabilities_pointer;
  uint8_t reserved[7];

  uint8_t interrupt_line;
  uint8_t interrupt_pin;
  uint8_t min_gnt;
  uint8_t max_lat;

  uint32_t device_specific_data[48];


} pci_dev_header_t;

typedef struct __pci_addr_t {
  uint8_t bus;
  uint8_t device;
  uint8_t function;
} pci_addr_t;

typedef struct __pci_bar_t {
	int type;
	uint32_t addr;
	uint32_t len;
	char *name;
}pci_bar_t;
#define MAX_PCI_BARS 20


int pci_generic_read(pci_addr_t *d, uint16_t pos, uint16_t len, void *buf);

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
typedef struct device device_t;
int read_pci_info(device_t *dev);
int pci_read(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf);
int pci_write(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf);
int read_msi(device_t *dev);
int enable_msix(device_t *dev);

#define VIRTIO_PCI_VENDOR_ID 0x1af4
#define VIRTIO_PCI_NET_DEVICE_ID 0x1000
#define VIRTIO_PCI_BLOCK_DEVICE_ID 0x1001
#define VIRTIO_PCI_BALLOON_DEVICE_ID 0x1002
#define VIRTIO_PCI_CONSOLE_DEVICE_ID 0x1003
#define VIRTIO_PCI_SCSI_DEVICE_ID 0x1004
#define VIRTIO_PCI_RING_DEVICE_ID 0x1004

#define VIRTIO_PCI_9P_DEVICE_ID 0x1009
#define IDE_PCI_DEVICE_ID 0x7010  /* ide bus */
#define IDE_CONTROLLER__PCI_DEVICE 0x7011 /* Intel(R) 82371AB/EB PCI Bus Master IDE Controller */

#define XEN_PLATFORM_VENDOR_ID 0x5853
#define XEN_PLATFORM_DEVICE_ID 0x0001

void init_pci_device(uint16_t vendor_id, uint16_t device_id, int msi_enabled , int *(init)(pci_dev_header_t pci,pci_bar_t bars[], uint32_t len,uint32_t *msi_vector));
int init_virtio_pci(pci_dev_header_t *pci_hdr, pci_bar_t bars[], uint32_t len,uint32_t *msi_vector);


typedef struct pci_device {
	pci_addr_t pci_addr;
	pci_dev_header_t pci_header;
	pci_bar_t pci_bars[MAX_PCI_BARS];
	int pci_bar_count;
	unsigned long pci_ioaddr, pci_iolen;
	unsigned long pci_mmio;
	unsigned long pci_mmiolen;
	struct pcicfg_msix msix_cfg;
	int msi_enabled;
	unsigned char *description;
}pci_device_t;
int read_pci_info_new(pci_device_t *dev);
int pci_enable_msix(pci_addr_t *addr,struct pcicfg_msix *msix,uint8_t capabilities_pointer);
int pci_read_msi(pci_addr_t *addr,pci_dev_header_t *pci_hdr, pci_bar_t *bars, uint32_t bars_count,  struct pcicfg_msix *msix);


#endif
