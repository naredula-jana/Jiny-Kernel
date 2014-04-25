include Rules.inc
LDFLAGS += -M

LWIP_USER_OBJ=modules/lwip_net/lwip-net.o modules/lwip_net/lwip-arch.o modules/lwip_net/web_server.o
OBJ_LWIP=$(LWIP_USER_OBJ) /opt_src/lwip/src/api/sockets.o /opt_src/lwip/src/api/api_msg.o /opt_src/lwip/src/api/tcpip.o /opt_src/lwip/src/api/netifapi.o /opt_src/lwip/src/api/netbuf.o /opt_src/lwip/src/api/netdb.o /opt_src/lwip/src/api/api_lib.o /opt_src/lwip/src/api/err.o /opt_src/lwip/src/core/tcp.o /opt_src/lwip/src/core/dhcp.o /opt_src/lwip/src/core/pbuf.o /opt_src/lwip/src/core/memp.o /opt_src/lwip/src/core/snmp/msg_out.o \
 /opt_src/lwip/src/core/snmp/mib2.o /opt_src/lwip/src/core/snmp/asn1_enc.o /opt_src/lwip/src/core/snmp/asn1_dec.o /opt_src/lwip/src/core/snmp/mib_structs.o /opt_src/lwip/src/core/snmp/msg_in.o /opt_src/lwip/src/core/ipv4/igmp.o /opt_src/lwip/src/core/ipv4/ip.o /opt_src/lwip/src/core/ipv4/ip_addr.o /opt_src/lwip/src/core/ipv4/inet_chksum.o /opt_src/lwip/src/core/ipv4/icmp.o /opt_src/lwip/src/core/ipv4/autoip.o \
 /opt_src/lwip/src/core/ipv4/inet.o  /opt_src/lwip/src/core/timers.o /opt_src/lwip/src/core/ipv4/ip_frag.o /opt_src/lwip/src/core/udp.o /opt_src/lwip/src/core/sys.o /opt_src/lwip/src/core/init.o /opt_src/lwip/src/core/netif.o /opt_src/lwip/src/core/def.o /opt_src/lwip/src/core/tcp_out.o /opt_src/lwip/src/core/stats.o /opt_src/lwip/src/core/raw.o /opt_src/lwip/src/core/tcp_in.o /opt_src/lwip/src/core/dns.o /opt_src/lwip/src/core/mem.o /opt_src/lwip/src/netif/ppp/md5.o \
 /opt_src/lwip/src/netif/ppp/ppp.o /opt_src/lwip/src/netif/ppp/fsm.o /opt_src/lwip/src/netif/ppp/pap.o /opt_src/lwip/src/netif/ppp/vj.o /opt_src/lwip/src/netif/ppp/auth.o /opt_src/lwip/src/netif/ppp/magic.o /opt_src/lwip/src/netif/ppp/lcp.o /opt_src/lwip/src/netif/ppp/randm.o /opt_src/lwip/src/netif/ppp/ipcp.o /opt_src/lwip/src/netif/ppp/ppp_oe.o /opt_src/lwip/src/netif/ppp/chap.o /opt_src/lwip/src/netif/ppp/chpms.o \
 /opt_src/lwip/src/netif/slipif.o  /opt_src/lwip/src/netif/etharp.o /opt_src/lwip/src/netif/ethernetif.o
 
OBJ_XEN=drivers/xen/xen_init.o drivers/xen/xenbus.o drivers/xen/evntchn.o drivers/xen/gntmap.o drivers/xen/gnttab.o drivers/xen/net_front.o 

OBJ_VIRTIO= drivers/virtio/virtio_ring.o \
	drivers/virtio/virtio_memballoon.o \
	drivers/virtio/net/test_udpserver.o  \
	drivers/virtio/9p/9p.o \
	drivers/virtio/9p/p9_fs.o drivers/virtio/driver_virtio_pci.o

OBJ_MEMLEAK=mm/memleak/memleak.o mm/memleak/os_dep.o mm/memleak/prio_tree.o  mm/memleak/memleak_hook.o

OBJ_32CODE= arch/$(ARCH_DIR)/boot.o arch/$(ARCH_DIR)/smp/head.o arch/$(ARCH_DIR)/smp/trampoline_64.o 
OBJ_ARCH=  arch/$(ARCH_DIR)/vsyscall_emu_64.o \
	arch/$(ARCH_DIR)/init.o \
	arch/$(ARCH_DIR)/syscall.o \
	arch/$(ARCH_DIR)/clock.o \
	arch/$(ARCH_DIR)/isr.o \
	arch/$(ARCH_DIR)/descriptor_tables.o \
	arch/$(ARCH_DIR)/pci.o \
	arch/$(ARCH_DIR)/msix.o \
	arch/$(ARCH_DIR)/paging.o \
	arch/$(ARCH_DIR)/interrupt.o  
	
OBJ_DRIVER = drivers/display.o drivers/driver_keyboard.o drivers/driver_serial.o drivers/hostshm/host_fs.o \
	drivers/hostshm/shm_device.o drivers/hostshm/shm_queue.o

OBJ_KERNEL = kernel/debug.o kernel/jdevices.o kernel/init.o  kernel/acpi.o kernel/ipc.o  kernel/network_sched.o kernel/kshell.o  kernel/symbol_table.o  kernel/syscall.o  kernel/sched_task.o  kernel/util.o kernel/module_app.o
	
OBJ_MEM= mm/memory.o mm/jslab.o mm/mmap.o mm/pagecache.o 
OBJ_FS=	fs/binfmt_elf.o fs/pipe.o fs/file.o fs/fs_api.o fs/socket.o fs/vfs.o fs/proc_fs.o
OBJ_SMP = arch/$(ARCH_DIR)/smp/smp-imps.o  arch/$(ARCH_DIR)/smp/apic.o


OBJECTS =  $(OBJ_ARCH) $(OBJ_SMP) $(OBJ_KERNEL) $(OBJ_MEM) $(OBJ_FS) $(OBJ_MEMLEAK) $(OBJ_VIRTIO) $(OBJ_DRIVER)



ifdef LWIP_NONMODULE
//OBJECTS += $(OBJ_LWIP) 
endif
ifdef JINY_UDPSTACK
OBJECTS += modules/udp_stack/udp_stack.o
endif


 
user: 
	make SOURCE_ROOT=$$PWD -C userland clean
	make SOURCE_ROOT=$$PWD -C userland

kshell:
	make SOURCE_ROOT=$$PWD -C modules/kshell
	cp modules/kshell/kshell.o test/root/
	
udpip_stack:
	make SOURCE_ROOT=$$PWD -C modules/udp_stack

test_net:
	make SOURCE_ROOT=$$PWD -C modules/test_net
	cp modules/test_net/test_net.o test/root/

test_file:
	make SOURCE_ROOT=$$PWD -C modules/test_file
	cp modules/test_file/test_file.o test/root/
	gcc modules/test_file/test_file.c -static -o test/root/test_file
	
all: 
	make SOURCE_ROOT=$$PWD -C kernel
	make SOURCE_ROOT=$$PWD -C drivers
#	make SOURCE_ROOT=$$PWD -C drivers/xen
ifdef LWIP_ENABLE
ifdef LWIP_NONMODULE
	make SOURCE_ROOT=$$PWD -C modules/lwip_net
else
	make SOURCE_ROOT=$$PWD -C modules/lwip_net
	ld -r $(LWIP_OBJ) -o lwip-module.o
endif
endif
	make SOURCE_ROOT=$$PWD -C modules/udp_stack
	make SOURCE_ROOT=$$PWD -C drivers/hostshm
	make SOURCE_ROOT=$$PWD -C drivers/virtio
	make SOURCE_ROOT=$$PWD -C drivers/virtio/9p
	make SOURCE_ROOT=$$PWD -C drivers/virtio/net
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR)
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR)/smp
	make SOURCE_ROOT=$$PWD -C mm
	make SOURCE_ROOT=$$PWD -C mm/memleak
	make SOURCE_ROOT=$$PWD -C fs
	$(LCPP)  -nostdlib -g -I.  -feliminate-dwarf2-dups $(LINK_FLAG)  $(OBJ_32CODE)  $(OBJECTS) -Wl,-N -T kernel/kernel.ldp -o bin/jiny_kernel.elf

#	$(LCPP) -nostdlib -g -I. -feliminate-dwarf2-dups $(LINK_FLAG)  $(OBJ_32CODE)  $(OBJECTS) -Wl,-N -Wl,-Ttext -Wl,40100000 -Tdata=40200000 -o bin/kernel_bin

	objdump -D -l bin/jiny_kernel.elf > bin/obj_file
	objcopy -O binary bin/jiny_kernel.elf bin/jiny_kernel.bin
#	util/gen_symboltbl util/in bin/mod_file > util/out
#	util/dwarf_reader bin/kernel_bin > util/dwarf_temp_output
#	chmod 777 ./dwarf_datatypes
#	mv ./dwarf_datatypes test/root/
	
clean:
	make SOURCE_ROOT=$$PWD -C kernel clean
	make SOURCE_ROOT=$$PWD -C drivers clean
	make SOURCE_ROOT=$$PWD -C drivers/xen clean
	make SOURCE_ROOT=$$PWD -C modules/lwip_net clean
	make SOURCE_ROOT=$$PWD -C drivers/hostshm clean
	make SOURCE_ROOT=$$PWD -C drivers/virtio clean
	make SOURCE_ROOT=$$PWD -C drivers/virtio/9p clean
	make SOURCE_ROOT=$$PWD -C drivers/virtio/net clean
	make SOURCE_ROOT=$$PWD -C fs clean
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR) clean
	make SOURCE_ROOT=$$PWD -C arch/$(ARCH_DIR)/smp clean
	make SOURCE_ROOT=$$PWD -C mm clean
	make SOURCE_ROOT=$$PWD -C mm/memleak clean
	make SOURCE_ROOT=$$PWD -C modules/udp_stack clean
	\rm bin/jiny_bin
ifdef LWIP_ENABLE
	\rm $(LWIP_OBJ)
endif

LWC     := $(shell find /opt_src/lwip/src/ -type f -name '*.c')
LWC     := $(filter-out %6.c %ip6_addr.c, $(LWC))
LWO     := $(patsubst %.c,%.o,$(LWC))
# LWO     += $(addprefix $(OBJ_DIR)/,lwip-arch.o lwip-net.o)

lwip.a: $(LWO)
	ar cqs $@ $^


