include Rules.inc
LDFLAGS += -M
LWIP_OBJ=/data/lwip/src/api/sockets.o /data/lwip/src/api/api_msg.o /data/lwip/src/api/tcpip.o /data/lwip/src/api/netifapi.o /data/lwip/src/api/netbuf.o /data/lwip/src/api/netdb.o /data/lwip/src/api/api_lib.o /data/lwip/src/api/err.o /data/lwip/src/core/tcp.o /data/lwip/src/core/dhcp.o /data/lwip/src/core/pbuf.o /data/lwip/src/core/memp.o /data/lwip/src/core/snmp/msg_out.o /data/lwip/src/core/snmp/mib2.o /data/lwip/src/core/snmp/asn1_enc.o /data/lwip/src/core/snmp/asn1_dec.o /data/lwip/src/core/snmp/mib_structs.o /data/lwip/src/core/snmp/msg_in.o /data/lwip/src/core/ipv4/igmp.o /data/lwip/src/core/ipv4/ip.o /data/lwip/src/core/ipv4/ip_addr.o /data/lwip/src/core/ipv4/inet_chksum.o /data/lwip/src/core/ipv4/icmp.o /data/lwip/src/core/ipv4/autoip.o /data/lwip/src/core/ipv4/inet.o /data/lwip/src/core/ipv4/ip_frag.o /data/lwip/src/core/udp.o /data/lwip/src/core/sys.o /data/lwip/src/core/init.o /data/lwip/src/core/netif.o /data/lwip/src/core/tcp_out.o /data/lwip/src/core/stats.o /data/lwip/src/core/raw.o /data/lwip/src/core/tcp_in.o /data/lwip/src/core/dns.o /data/lwip/src/core/mem.o /data/lwip/src/netif/ppp/md5.o /data/lwip/src/netif/ppp/ppp.o /data/lwip/src/netif/ppp/fsm.o /data/lwip/src/netif/ppp/pap.o /data/lwip/src/netif/ppp/vj.o /data/lwip/src/netif/ppp/auth.o /data/lwip/src/netif/ppp/magic.o /data/lwip/src/netif/ppp/lcp.o /data/lwip/src/netif/ppp/randm.o /data/lwip/src/netif/ppp/ipcp.o /data/lwip/src/netif/ppp/ppp_oe.o /data/lwip/src/netif/ppp/chap.o /data/lwip/src/netif/ppp/chpms.o /data/lwip/src/netif/slipif.o /data/lwip/src/netif/loopif.o /data/lwip/src/netif/etharp.o
XEN_OBJ=$(LWIP_OBJ) drivers/xen/xen_init.o drivers/xen/xenbus.o drivers/xen/evntchn.o drivers/xen/gntmap.o drivers/xen/gnttab.o drivers/xen/net_front.o drivers/xen/lwip-net.o drivers/xen/lwip-arch.o
OBJECTS=arch/$(ARCH_DIR)/boot.o arch/$(ARCH_DIR)/init.o arch/$(ARCH_DIR)/syscall.o arch/$(ARCH_DIR)/isr.o arch/$(ARCH_DIR)/descriptor_tables.o arch/$(ARCH_DIR)/pci.o arch/$(ARCH_DIR)/paging.o arch/$(ARCH_DIR)/interrupt.o drivers/display.o drivers/keyboard.o drivers/serial.o drivers/host_shm.o mm/memory.o mm/slab.o mm/mmap.o mm/pagecache.o fs/binfmt_elf.o fs/vfs.o fs/host_fs.o $(XEN_OBJ) kernel.a

user: 
	make SOURCE_ROOT=$$PWD -C userland clean
	make SOURCE_ROOT=$$PWD -C userland
all: lwip.a
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
	\rm $(LWIP_OBJ)

LWC     := $(shell find /data/lwip/ -type f -name '*.c')
LWC     := $(filter-out %6.c %ip6_addr.c %ethernetif.c, $(LWC))
LWO     := $(patsubst %.c,%.o,$(LWC))
# LWO     += $(addprefix $(OBJ_DIR)/,lwip-arch.o lwip-net.o)

lwip.a: $(LWO)
	ar cqs $@ $^


