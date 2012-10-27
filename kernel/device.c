#define DEBUG_ENABLE 1
#include "device.h"
#include "interface.h"

device_class_t deviceClass_root = { "root",NULL, NULL, NULL,NULL,
	NULL, NULL, NULL};


int add_deviceClass(void *addr){
	device_class_t *devClass,*parent;

	devClass = addr;
    parent = devClass->parent;

    if (parent ==0 ) return 0;
    devClass->sibling = parent->children;
    parent->children = devClass;
    return 1;
}

#define MAX_DEVICES 100
struct {
	pci_addr_t addr;
	pci_dev_header_t header;
}device_list[MAX_DEVICES];
int device_count=0;

static void scan_devices(){
	int i,j,k;
	int ret;
#define MAX_BUS 32
#define MAX_PCI_DEV 32
#define MAX_PCI_FUNC 32

	device_count=0;
	for (i = 0; i < MAX_BUS ; i++) {
		for (j = 0; j < MAX_PCI_DEV; j++) {
	        for (k=0;  k< MAX_PCI_FUNC; k++){
	        	if (device_count >= (MAX_DEVICES-1)) return 0;

	        	device_list[device_count].addr.bus=i;
	        	device_list[device_count].addr.device=j;
	        	device_list[device_count].addr.function=k;
	        	device_list[device_count].header.vendor_id=0;
	        	device_list[device_count].header.device_id=0;

	        	ret = pci_generic_read(&device_list[device_count].addr, 0, sizeof(pci_dev_header_t), &device_list[device_count].header);
	        	if (ret != 0 || device_list[device_count].header.vendor_id==0xffff) continue;
	        	DEBUG("scan devices %d:%d:%d  %x:%x\n",i,j,k,device_list[device_count].header.vendor_id,device_list[device_count].header.device_id);
	        	device_count++;
	        }
		}
	}
}
int init_devClasses() {
	device_t *dev;
	device_class_t *devClass;
	int (*probe)(device_t *dev);
	int (*attach)(device_t *dev);
	int i,ret;

	/* get all devices */
	scan_devices();

	for (i = 0; i < device_count; i++) {
		dev = ut_malloc(sizeof(device_t));
		memset(dev, 0, sizeof(device_t));
		ut_memcpy(&dev->pci_addr, &device_list[i].addr, sizeof(pci_addr_t));
		ut_memcpy(&dev->pci_hdr, &device_list[i].header,sizeof(pci_dev_header_t));
		ret = 0;
		devClass = deviceClass_root.children;
		while (devClass != NULL && ret == 0) {
			probe = devClass->probe;
			attach = devClass->attach;
			if (probe != NULL && attach != NULL) {
				if (probe(dev) == 1) {
					ret=devClass->attach(dev);
					break;
				}
			}
			devClass = devClass->sibling;
		}
		if (ret == 0) {
			ut_printf("Unable to attach the device to any driver: bus:dev;fun:%x:%x:%x devdor:device:id:%x-%x \n",dev->pci_addr.bus, dev->pci_addr.device, dev->pci_addr.function,dev->pci_hdr.vendor_id, dev->pci_hdr.device_id);
			ut_free(dev);
		}
	}
	return 1;
}
static int list_devClasses(device_class_t *parent, int level){
	int count=0;
    if (parent ==0) return 0;
	while (parent != NULL) {
       ut_printf(" level:%d  %s\n",level,parent->name);
       count = count+list_devClasses(parent->children,level++);
       parent = parent->sibling;
       count = count+1;
	}
	return count;
}
int Jcmd_dev_stat(){
	list_devClasses(&deviceClass_root,0);
}
/***********************************************************************************************
 * Modules:
 ***********************************************************************************************/

module_t MODULE_root = { "root",NULL, NULL, NULL,0,
	NULL, NULL, NULL};


int add_module(void *addr){
	module_t *module,*parent;

	module = addr;
    parent = module->parent;

    if (parent == 0 ) return 0;
    module->sibling = parent->children;
    parent->children = module;
    return 1;
}

int init_modules() {

	return 1;
}
