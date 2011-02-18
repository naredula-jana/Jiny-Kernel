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
	uint32_t addr;
	uint32_t len;
}pci_bar_t;

