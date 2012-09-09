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
int pci_read(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf);
int pci_write(pci_addr_t *d, uint16_t pos, uint8_t len, void *buf);


