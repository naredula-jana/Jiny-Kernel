
#include "xen.h"

shared_info_t *g_sharedInfoArea;
pci_dev_header_t xen_pci_hdr;

static unsigned long pci_ioaddr, pci_iolen;
static unsigned long platform_mmio;
static unsigned long platform_mmio_alloc = 0;
static unsigned long platform_mmiolen;
static unsigned long shared_info_frame;
#define XEN_IOPORT_BASE 0x10

#define XEN_IOPORT_PLATFLAGS    (XEN_IOPORT_BASE + 0) /* 1 byte access (R/W) */
#define XEN_IOPORT_MAGIC        (XEN_IOPORT_BASE + 0) /* 2 byte access (R) */
#define XEN_IOPORT_UNPLUG       (XEN_IOPORT_BASE + 0) /* 2 byte access (W) */
#define XEN_IOPORT_DRVVER       (XEN_IOPORT_BASE + 0) /* 4 byte access (W) */

#define XEN_IOPORT_SYSLOG       (XEN_IOPORT_BASE + 2) /* 1 byte access (W) */
#define XEN_IOPORT_PROTOVER     (XEN_IOPORT_BASE + 2) /* 1 byte access (R) */
#define XEN_IOPORT_PRODNUM      (XEN_IOPORT_BASE + 2) /* 2 byte access (W) */

#define XEN_IOPORT_MAGIC_VAL 0x49d2
#define XEN_IOPORT_LINUX_PRODNUM 0xffff /* NB: register a proper one */
#define XEN_IOPORT_LINUX_DRVVER  ((0x1 << 8) + 0x0)

#define UNPLUG_ALL_IDE_DISKS 1
#define UNPLUG_ALL_NICS 2
#define UNPLUG_AUX_IDE_DISKS 4
#define UNPLUG_ALL 7

static int check_platform_magic(long ioaddr, long iolen) {
	short magic, unplug = 0;
	char protocol, *p, *q, *err;

#if 0
	for (p = dev_unplug; p; p = q) {
		q = strchr(dev_unplug, ',');
		if (q)
		*q++ = '\0';
		if (!strcmp(p, "all"))
		unplug |= UNPLUG_ALL;
		else if (!strcmp(p, "ide-disks"))
		unplug |= UNPLUG_ALL_IDE_DISKS;
		else if (!strcmp(p, "aux-ide-disks"))
		unplug |= UNPLUG_AUX_IDE_DISKS;
		else if (!strcmp(p, "nics"))
		unplug |= UNPLUG_ALL_NICS;
		else
		dev_warn(dev, "unrecognised option '%s' "
				"in module parameter 'dev_unplug'\n", p);
	}
#endif
	if (iolen < 0x16) {
		err = "backend too old";
		goto no_dev;
	}

	magic = inw(XEN_IOPORT_MAGIC);

	if (magic != XEN_IOPORT_MAGIC_VAL) {
		err = "unrecognised magic value";
		goto no_dev;
	}

	protocol = inb(XEN_IOPORT_PROTOVER);

	DEBUG("Xen I/O protocol version %d\n", protocol);

	switch (protocol) {
	case 1:
		outw(XEN_IOPORT_LINUX_PRODNUM, XEN_IOPORT_PRODNUM);
		outl(XEN_IOPORT_LINUX_DRVVER, XEN_IOPORT_DRVVER);
		if (inw(XEN_IOPORT_MAGIC) != XEN_IOPORT_MAGIC_VAL) {
			DEBUG("Xen Error : blacklisted by host\n");
			return -1;
		}
		/* Fall through */
	case 0:
		outw(unplug, XEN_IOPORT_UNPLUG);
		break;
	default:
		err = "unknown I/O protocol version";
		goto no_dev;
	}

	return 0;

	no_dev: DEBUG("Xen Error failed backend handshake: %s\n", err);
	if (!unplug)
		return 0;
	DEBUG("Xen Error failed to execute specified dev_unplug options!\n");
	return -1;
}

static inline void native_cpuid(unsigned int *eax, unsigned int *ebx,
		unsigned int *ecx, unsigned int *edx) {
	/* ecx is often an input as well as an output. */
	asm volatile("cpuid"
			: "=a" (*eax),
			"=b" (*ebx),
			"=c" (*ecx),
			"=d" (*edx)
			: "0" (*eax), "2" (*ecx));
}

/*
 * Generic CPUID function
 * clear %ecx since some cpus (Cyrix MII) do not set or clear %ecx
 * resulting in stale register contents being returned.
 */
static inline void cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx,
		unsigned int *ecx, unsigned int *edx) {
	*eax = op;
	*ecx = 0;
	native_cpuid(eax, ebx, ecx, edx);
}

static int set_callback_via(uint64_t via) {
	struct xen_hvm_param a;

	a.domid = DOMID_SELF;
	a.index = HVM_PARAM_CALLBACK_IRQ;
	a.value = via;
	return HYPERVISOR_hvm_op(HVMOP_set_param, &a);
}

unsigned long alloc_xen_mmio(unsigned long len) {
	unsigned long addr;

	addr = platform_mmio + platform_mmio_alloc;
	platform_mmio_alloc += len;
	// BUG_ON(platform_mmio_alloc > platform_mmiolen);

	return addr;
}

static unsigned char xen_data[1024];
int xen_time(char *arg1, char *arg2) {
	static int init=0;
	struct vcpu_time_info *src = &g_sharedInfoArea->vcpu_info[0].time;
	unsigned long ns;
	unsigned long e_pen, e_mask;
	ns = src->system_time / 1000000000;
	DEBUG(" new xen  system time:%x :%x %d \n", src->system_time,
			src->tsc_timestamp, ns);
if (1)
{
	start_networking();
	start_webserver();
}

	return g_sharedInfoArea->wc_sec;
}

static int init_xen_info(void) {
	struct xen_add_to_physmap xatp;
	unsigned long phy_addr;

	//  setup_xen_features();
	phy_addr = alloc_xen_mmio(PAGE_SIZE);
	shared_info_frame = phy_addr >> PAGE_SHIFT;
	xatp.domid = DOMID_SELF;
	xatp.idx =0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = shared_info_frame;
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
		BUG();

	g_sharedInfoArea = HOST_XEN_SH_ADDR;
	vm_mmap(0, g_sharedInfoArea, PAGE_SIZE, PROT_WRITE, MAP_FIXED, phy_addr,"xen");

	DEBUG("NEW shared info area sec  : %x phyaddr:%x \n", g_sharedInfoArea->wc_sec,phy_addr);
	init_events();
	if (1) {
		struct xen_hvm_param evntchn, a;

		evntchn.domid = DOMID_SELF;
		evntchn.index = HVM_PARAM_STORE_EVTCHN;
		HYPERVISOR_hvm_op(HVMOP_get_param, &evntchn);
		DEBUG("xen Event channel :%x:\n", evntchn.value);
		a.domid = DOMID_SELF;
		a.index = HVM_PARAM_STORE_PFN;
		HYPERVISOR_hvm_op(HVMOP_get_param, &a);
		init_xenbus(a.value << PAGE_SHIFT, evntchn.value);
		DEBUG("xen Event channel pfn :%x:\n", a.value);
	}
	return 0;
}
extern void do_hypervisor_callback(struct pt_regs *regs);
static void xen_pci_interrupt(registers_t regs)
{

	//*p = 0; /* reset the irq by resetting the status  */
	DEBUG("LATEST  XEN PCI-INTERRUPT:  \n");

	do_hypervisor_callback(0);
}

static uint32_t xen_cpuid_base(void) {
	uint32_t base, eax, ebx, ecx, edx;
	char signature[13];

	for (base = 0x40000000; base < 0x40010000; base += 0x100) {
		cpuid(base, &eax, &ebx, &ecx, &edx);
		*(uint32_t*) (signature + 0) = ebx;
		*(uint32_t*) (signature + 4) = ecx;
		*(uint32_t*) (signature + 8) = edx;
		signature[12] = 0;
		DEBUG("XEN signature :%s: \n ", signature);
		// if (!strcmp("XenVMMXenVMM", signature) && ((eax - base) >= 2))
		return base;
	}

	return 0;
}
static int init_hypercall_stubs(pci_dev_header_t *pci_hdr) {
	int ret;
	uint32_t eax, ebx, ecx, edx, npages, msr, i, base;
	unsigned long page;

	base = xen_cpuid_base();
	if (base == 0) {
		DEBUG("Detected Xen platform device but not Xen VMM?\n");
		return -1;
	}

	cpuid(base + 1, &eax, &ebx, &ecx, &edx);

	DEBUG("Xen version base:%x %d.%d.  \n", base, eax >> 16, eax & 0xffff);

	/*
	 * Find largest supported number of hypercall pages.
	 * We'll create as many as possible up to this number.
	 */
	cpuid(base + 2, &npages, &msr, &ecx, &edx);
	page = mm_getFreePages(0, 2);
	DEBUG("XEN pages:%d  vaddr :%x paddr:%x trap_instr:%s:\n", npages,
			page, __pa(page), TRAP_INSTR);
	wrmsrl(msr, __pa(page));
	ut_memcpy(hypercall_page, page, PAGE_SIZE - 1);
	//hypercall_page=page;
	ret = check_platform_magic(pci_ioaddr, pci_iolen);
	if ((ret = init_xen_info())) {
		ut_printf(" XEN : ERROR in init_xen_info \n");
		goto out;
	}

	if ((ret = set_callback_via(pci_hdr->interrupt_line)))
		goto out;

	out: return 0;
}
int init_xen_pci(pci_dev_header_t *pci_hdr, pci_bar_t bars[], uint32_t len) {
	uint32_t ret, i;
	xen_pci_hdr = *pci_hdr;

	DEBUG(" Initialising XEN PCI \n");

	if (bars[0].addr != 0 && bars[1].addr != 0) {
		pci_ioaddr = bars[0].addr;
		pci_iolen = bars[0].len;
		platform_mmio = bars[1].addr;
		platform_mmiolen = bars[1].len;
	} else {
		ut_printf(" ERROR in initializing xen PCI driver \n");
		return 0;
	}

	if (pci_hdr->interrupt_line > 0) {
		DEBUG(" Interrupt number : %i \n", pci_hdr->interrupt_line);
		ar_registerInterrupt(32 + pci_hdr->interrupt_line, xen_pci_interrupt,
				"xen_pci",NULL);
	}

	init_hypercall_stubs(pci_hdr);
	init_gnttab();

	return 1;
}
