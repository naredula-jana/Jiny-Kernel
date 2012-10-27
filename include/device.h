#ifndef __DEVICE_H__
#define __DEVICE_H__
#include "common.h"
#include "pci.h"

#define MAX_PCI_BARS 20

typedef struct device_class device_class_t;
//typedef struct device device_t;

struct device_class{
    char *name;

	/* List of Methods*/
	int (*probe)(device_t*);
	int (*attach)(device_t *dev);
	int (*dettach)(device_t *dev);

	device_class_t *parent;
	device_class_t *children;
	device_class_t *sibling;

	device_t *devices;
};


struct device{
	device_class_t *devClass;
    void *private_data;

	pci_dev_header_t pci_hdr;
	pci_addr_t pci_addr;
	pci_bar_t pci_bars[MAX_PCI_BARS];
	int pci_bar_count;
	struct pcicfg_msix msix_cfg;

	device_t *next;
};
extern device_class_t deviceClass_root;
#define DEFINE_DRIVER(name,parent,probe,attach,dettach) \
   extern device_class_t deviceClass##_##parent; \
   device_class_t deviceClass##_##name = {\
       #name, probe, attach, dettach, &deviceClass##_##parent, \
	    NULL, NULL, NULL};

extern int init_devClasses();
extern int add_deviceClass(void *addr);
extern int add_module(void *addr);
/****************************************************************************
 * Modules
 ***************************************************************************/
typedef struct module module_t;
struct module{
    char *name;

	/* List of Methods*/
	int (*load)();
	int (*unload)();
	int (*stat)();

	int init_level;
	module_t *parent;
	module_t *children;
	module_t *sibling;
};
extern module_t MODULE_root;
#define DEFINE_MODULE(name,parent,load,unload,stat) \
   extern module_t MODULE##_##parent; \
   module_t MODULE##_##name = {\
       #name, load, unload, stat, 1,  &MODULE##_##parent, \
	    NULL, NULL};

#endif
