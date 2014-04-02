SOURCE_ROOT=/opt_src/Jiny-Kernel/
#LWIP_NONMODULE=1
JINY_UDPSTACK=1
NETWORKING =1 
JINY_SLAB = 1
#JSLAB_DEBUG = 1
SMP=1
#MEMLEAK_TOOL=1
INCLUDE=. -DNETWORKING -I $(SOURCE_ROOT)/include -I $(SOURCE_ROOT)/mm -I $(SOURCE_ROOT)/modules/lwip_net/ -I $(SOURCE_ROOT)/arch/x86_64/


NEW_GCC=/data/opt/gcc-4.8.2/bin/

CC=$(NEW_GCC)gcc -g -gdwarf-2 -Wimplicit -w  
#CPP=$(NEW_GCC)g++ -g -std=gnu++0x -fno-rtti -lsupc++ -fno-exceptions -gdwarf-2 -fpermissive 
CPP=$(NEW_GCC)g++ -g -std=c++11 -fno-rtti -lsupc++ -fno-exceptions -gdwarf-2 -fpermissive  -w
LCPP=$(NEW_GCC)g++


AT=
ARCH=64

C_SOURCES=$(wildcard *.c)
CPP_SOURCES=$(wildcard *.cc)
ASM_SOURCES=$(wildcard *.s)
GNUASM_SOURCES=$(wildcard *.S)
SOURCES=$(C_SOURCES) $(ASM_SOURCES)
XEN_INCLUDE=/opt_src/xen/xen-4.1.1/dist/install/usr/include -I/opt_src/lwip/src/include -I/opt_src/lwip/src/include/ipv4 -I$(SOURCE_ROOT)/drivers/xen/ -I$(SOURCE_ROOT)/modules/lwip_net/ -I$(SOURCE_ROOT)/drivers/xen/arch/

CPP_OBJECTS=$(patsubst %.cc,%.o,$(CPP_SOURCES))
C_OBJECTS=$(patsubst %.c,%.o,$(C_SOURCES))
GNUASM_OBJECTS=$(patsubst  %.S,%.o,$(GNUASM_SOURCES))
ASM_OBJECTS=$(patsubst %.s,%.o,$(ASM_SOURCES))
OBJECTS=$(C_OBJECTS) $(ASM_OBJECTS) $(GNUASM_OBJECTS) $(CPP_OBJECTS)

ARCH_DIR=x86_64
CFLAGS= -mcmodel=kernel  -m64 -mno-red-zone -nostdlib -D _KERNEL  -nodefaultlibs  -Wall   -I$(INCLUDE) -I$(XEN_INCLUDE)  -DARCH_X86_64
CFLAGS_ASM= -mcmodel=kernel -nostdlib -nostdinc -nodefaultlibs -D__ASM__   -I$(INCLUDE) -DARCH_X86_64
LINK_FLAG= 
INCLUDE=. -I $(SOURCE_ROOT)/include -I $(SOURCE_ROOT)/mm -I $(SOURCE_ROOT)/include/$(ARCH_DIR)/ -I $(SOURCE_ROOT)/arch/$(ARCH_DIR) -I $(XEN_INCLUDE)

ifdef NETWORKING
INCLUDE += -DNETWORKING 
endif
ifdef JINY_SLAB
INCLUDE += -DJINY_SLAB
endif
ifdef JSLAB_DEBUG
INCLUDE += -DJSLAB_DEBUG
endif
ifdef SMP
INCLUDE += -DSMP
endif
ifdef MEMLEAK_TOOL
INCLUDE += -DMEMLEAK_TOOL
endif
ifdef JINY_UDPSTACK
INCLUDE += -DJINY_UDPSTACK
endif


%.o: %.cc
	@echo "$<"
	$(AT)$(CPP) -c $(CFLAGS) $< -o $@
	objcopy --localize-symbol=dwarf_entry $@ $@
%.o: %.c
	@echo "$<"
	$(AT)$(CC) -c $(CFLAGS) $< -o $@
%.o: %.k
	@echo "$<"
	$(AT)$(CC) -m32 -c $(CFLAGS_ASM) $< -o $@
%.o: %.S
	@echo "$<"
	$(AT)$(CC) -c $(CFLAGS_ASM) $< -o $@
%o:%s
	@echo "$<"
	$(AT)nasm -f elf  $< -o $@

dummy: all
	
process_sub_dirs:
	$(AT)for file in $(SUB_DIRS); do make SOURCE_ROOT=$(SOURCE_ROOT) -C $$file; done

clean_sub_dirs:
	$(AT)for file in $(SUB_DIRS); do make SOURCE_ROOT=$(SOURCE_ROOT) -C $$file clean; done

library: $(OBJECTS)
	$(AT)ar -rc $(SOURCE_ROOT)/`basename $$PWD`.a $(OBJECTS)

LIBRARIES=$(patsubst %,%.a,$(SUB_DIRS))

LD_CFLAGS += -m64 -mcmodel=kernel -mno-red-zone -fno-unwind-tables
LD_CFLAGS += -mno-sse -mno-mmx -mno-sse2 -mno-3dnow

%.ld: %.ld.S
	$(AT)$(CC) $(LD_CFLAGS) -I$(INCLUDE) -D__ASM__ -E -x c $< |  grep -v "^\#" > $@
