include Rules.inc
LDFLAGS += -M

ifeq "$(ARCH)" "32"
OBJECTS=arch/$(ARCH_DIR)/boot.o arch/$(ARCH_DIR)/gdt.o arch/$(ARCH_DIR)/descriptor_tables.o arch/$(ARCH_DIR)/isr.o arch/$(ARCH_DIR)/interrupt.o arch/$(ARCH_DIR)/pci.o arch/$(ARCH_DIR)/paging.o drivers/display.o drivers/keyboard.o mm/memory.o kernel.a
else
OBJECTS=arch/$(ARCH_DIR)/boot.o arch/$(ARCH_DIR)/isr.o arch/$(ARCH_DIR)/descriptor_tables.o arch/$(ARCH_DIR)/pci.o arch/$(ARCH_DIR)/paging.o arch/$(ARCH_DIR)/interrupt.o drivers/display.o drivers/keyboard.o drivers/serial.o drivers/host_shm.o mm/memory.o mm/slab.o mm/mmap.o fs/vfs.o fs/host_fs.o kernel.a
endif

#all: kernel.ld
all: clean
	make SOURCE_ROOT=$$PWD -C kernel
	make SOURCE_ROOT=$$PWD -C drivers
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR)
	make SOURCE_ROOT=$$PWD -C mm
	make SOURCE_ROOT=$$PWD -C fs
	rm drivers.a  fs.a mm.a $(ARCH_DIR).a
#	ld -T kernel.ld $(LDFLAGS) -q $(OBJECTS) -o $@ -Map kernel.map
#	ld -T kernel.ld $(LDFLAGS) -q $(OBJECTS) -o kernel_bin -Map kernel.map
	gcc -g -I. $(LINK_FLAG)  $(OBJECTS) -nostdlib -Wl,-N -Wl,-Ttext -Wl,100000 -o bin/kernel_bin
	objdump -D -l bin/kernel_bin > bin/obj_file
	nm bin/kernel_bin | sort > util/in
#	\rm bin/mod_file
	util/gen_symboltbl util/in bin/mod_file
clean:
	make SOURCE_ROOT=$$PWD -C kernel clean
	make SOURCE_ROOT=$$PWD -C drivers clean
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR) clean
	make SOURCE_ROOT=$$PWD -C mm clean

