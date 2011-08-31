include Rules.inc
LDFLAGS += -M
# XEN_OBJ=drivers/xen/lwip-net.o drivers/xen/lwip-arch.o
XEN_OBJ=drivers/xen/xen_init.o drivers/xen/xenbus.o drivers/xen/evntchn.o drivers/xen/gntmap.o drivers/xen/gnttab.o drivers/xen/net_front.o 
OBJECTS=arch/$(ARCH_DIR)/boot.o arch/$(ARCH_DIR)/init.o arch/$(ARCH_DIR)/syscall.o arch/$(ARCH_DIR)/isr.o arch/$(ARCH_DIR)/descriptor_tables.o arch/$(ARCH_DIR)/pci.o arch/$(ARCH_DIR)/paging.o arch/$(ARCH_DIR)/interrupt.o drivers/display.o drivers/keyboard.o drivers/serial.o drivers/host_shm.o mm/memory.o mm/slab.o mm/mmap.o mm/pagecache.o fs/binfmt_elf.o fs/vfs.o fs/host_fs.o $(XEN_OBJ) kernel.a

user: 
	make SOURCE_ROOT=$$PWD -C userland clean
	make SOURCE_ROOT=$$PWD -C userland
all: 
	make SOURCE_ROOT=$$PWD -C kernel
	make SOURCE_ROOT=$$PWD -C drivers
	make SOURCE_ROOT=$$PWD -C drivers/xen
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR)
	make SOURCE_ROOT=$$PWD -C mm
	make SOURCE_ROOT=$$PWD -C fs
	rm drivers.a  fs.a mm.a $(ARCH_DIR).a
#	ld -T kernel.ld $(LDFLAGS) -o bin/kernel_bin $(OBJECTS) -Map kernel.map
	gcc -g -I. $(LINK_FLAG)  $(OBJECTS) -nostdlib -Wl,-N -Wl,-Ttext -Wl,40100000 -o bin/kernel_bin
#	gcc -g -I. $(LINK_FLAG)  $(OBJECTS) -nostdlib -Wl,-N -Wl,-Ttext -Wl,100000 -o bin/kernel_bin
#	gcc -mcmodel=large -shared-intel -g -I. $(LINK_FLAG)  $(OBJECTS) -nostdlib -Wl,-N -Wl,-Ttext -Wl,c0000000 -o bin/kernel_bin
#	gcc -g -I. $(LINK_FLAG)  $(OBJECTS) -nostdlib -Wl,-N -Wl,-Ttext  -Wl,-Tkernel.ld  -o bin/kernel_bin
	objdump -D -l bin/kernel_bin > bin/obj_file
	nm bin/kernel_bin | sort > util/in
#	\rm bin/mod_file
	util/gen_symboltbl util/in bin/mod_file
clean:
	make SOURCE_ROOT=$$PWD -C kernel clean
	make SOURCE_ROOT=$$PWD -C drivers clean
	make SOURCE_ROOT=$$PWD -C drivers/xen clean
	make SOURCE_ROOT=$$PWD -C fs clean
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR) clean
	make SOURCE_ROOT=$$PWD -C mm clean

