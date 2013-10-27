#include "xen.h"

#define NR_RESERVED_ENTRIES 8
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* NR_GRANT_FRAMES must be less than or equal to that configured in Xen */

#define NR_GRANT_FRAMES 1

#define NR_GRANT_ENTRIES (NR_GRANT_FRAMES * PAGE_SIZE / sizeof(grant_entry_t))

 grant_entry_t *gnttab_table;
static grant_ref_t gnttab_list[NR_GRANT_ENTRIES];
#ifdef GNT_DEBUG
static char inuse[NR_GRANT_ENTRIES];
#endif
//static __DECLARE_SEMAPHORE_GENERIC(gnttab_sem, 0);

static void
put_free_entry(grant_ref_t ref)
{
    unsigned long flags;
    local_irq_save(flags);
#ifdef GNT_DEBUG
    BUG_ON(!inuse[ref]);
    inuse[ref] = 0;
#endif
    gnttab_list[ref] = gnttab_list[0];
    gnttab_list[0]  = ref;
    local_irq_restore(flags);
/*   up(&gnttab_sem);*/
}

static grant_ref_t
get_free_entry(void)
{
    unsigned int ref;
    unsigned long flags;
 /*   down(&gnttab_sem);*/
    local_irq_save(flags);
    ref = gnttab_list[0];
    BUG_ON(ref < NR_RESERVED_ENTRIES || ref >= NR_GRANT_ENTRIES);
    gnttab_list[0] = gnttab_list[ref];
#ifdef GNT_DEBUG
    BUG_ON(inuse[ref]);
    inuse[ref] = 1;
#endif
    local_irq_restore(flags);
    return ref;
}

grant_ref_t
gnttab_grant_access(domid_t domid, unsigned long frame, int readonly)
{
    grant_ref_t ref;

    ref = get_free_entry();
    gnttab_table[ref].frame = frame;
    gnttab_table[ref].domid = domid;
    wmb();
    readonly *= GTF_readonly;
    gnttab_table[ref].flags = GTF_permit_access | readonly;

    return ref;
}

grant_ref_t
gnttab_grant_transfer(domid_t domid, unsigned long pfn)
{
    grant_ref_t ref;

    ref = get_free_entry();
    gnttab_table[ref].frame = pfn;
    gnttab_table[ref].domid = domid;
    wmb();
    gnttab_table[ref].flags = GTF_accept_transfer;

    return ref;
}

int
gnttab_end_access(grant_ref_t ref)
{
    uint16_t flags, nflags;

    BUG_ON(ref >= NR_GRANT_ENTRIES || ref < NR_RESERVED_ENTRIES);

    nflags = gnttab_table[ref].flags;
    do {
        if ((flags = nflags) & (GTF_reading|GTF_writing)) {
            printk("WARNING: g.e. still in use! (%x)\n", flags);
            return 0;
        }
    } while ((nflags = synch_cmpxchg(&gnttab_table[ref].flags, flags, 0)) !=
            flags);

    put_free_entry(ref);
    return 1;
}

unsigned long
gnttab_end_transfer(grant_ref_t ref)
{
    unsigned long frame;
    uint16_t flags;

    BUG_ON(ref >= NR_GRANT_ENTRIES || ref < NR_RESERVED_ENTRIES);

    while (!((flags = gnttab_table[ref].flags) & GTF_transfer_committed)) {
        if (synch_cmpxchg(&gnttab_table[ref].flags, flags, 0) == flags) {
            printk("Release unused transfer grant.\n");
            put_free_entry(ref);
            return 0;
        }
    }

    /* If a transfer is in progress then wait until it is completed. */
    while (!(flags & GTF_transfer_completed)) {
        flags = gnttab_table[ref].flags;
    }

    /* Read the frame number /after/ reading completion status. */
    rmb();
    frame = gnttab_table[ref].frame;

    put_free_entry(ref);

    return frame;
}

grant_ref_t
gnttab_alloc_and_grant(void **map)
{
    unsigned long mfn;
    grant_ref_t gref;

    *map = (void *)alloc_page(0);
    mfn = __pa(*map)>>PAGE_SHIFT;
    gref = gnttab_grant_access(0, mfn, 0);
    return gref;
}

static const char * const gnttabop_error_msgs[] = GNTTABOP_error_msgs;

const char *
gnttabop_error(int16_t status)
{
    status = -status;
    if (status < 0 || status >= ARRAY_SIZE(gnttabop_error_msgs))
	return "bad status";
    else
        return gnttabop_error_msgs[status];
}
#if 0
void
init_gnttab(void)
{
    struct gnttab_setup_table setup;
    unsigned long frames[NR_GRANT_FRAMES];
    int i;

#ifdef GNT_DEBUG
    memset(inuse, 1, sizeof(inuse));
#endif
    for (i = NR_RESERVED_ENTRIES; i < NR_GRANT_ENTRIES; i++)
        put_free_entry(i);

    setup.dom = DOMID_SELF;
    setup.nr_frames = NR_GRANT_FRAMES;

   set_xen_guest_handle(setup.frame_list, frames);
    //setup.frame_list=frames;
    setup.status=0x456;
    //frames[0]=0x123;
    printk("NEW BEFORE gnttab_table mapped at %x .  status:%x\ size:%d subsize:%d \n", frames[0],setup.status,sizeof(struct gnttab_setup_table),sizeof(setup.frame_list));
    HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);

   // gnttab_table = map_frames(frames, NR_GRANT_FRAMES);
    printk("After gnttab_table mapped at %x.  innew status:%x\n", frames[0],setup.status);
}
#endif

static unsigned long frames[NR_GRANT_FRAMES];
struct gnttab_setup_table setup;

#if 0
void
init_gnttab(void)
{


    int ret,i;


  //  for (i = NR_RESERVED_ENTRIES; i < NR_GRANT_ENTRIES; i++)
    //    put_free_entry(i);

    printk("BEFORE .. gnttab_table mapped at %p. frames:%x :%x \n", gnttab_table,gnttab_table,frames);
    setup.dom = DOMID_SELF;

    setup.nr_frames = NR_GRANT_FRAMES;
    setup.nr_frames = 1;
// setup.frame_list=(__guest_handle_ulong)frames;
    setup.status=0;
   set_xen_guest_handle(setup.frame_list, &frames[0]);
    //setup.frame_list=(__guest_handle_ulong)&frames[0];
    frames[0]=0x0;

    printk("NEW before hypercall frames[0] :%x size:%d subsize:%d status:%x(%d)\n", frames[0],sizeof(struct gnttab_setup_table),sizeof(setup.frame_list),setup.status,setup.status);
    ret=HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
    //gnttab_table = map_frames(frames, NR_GRANT_FRAMES);
    printk("AFTER.. grant table [0,1] :%x :%x ret:%x status:%x(%d)\n", frames[0],frames[1],-ret,setup.status,setup.status);
}
#endif


void init_gnttab(void)
{
    int ret, i;

	for (i = NR_RESERVED_ENTRIES; i < NR_GRANT_ENTRIES; i++)
		put_free_entry(i);

	struct xen_add_to_physmap xatp;

	gnttab_table = mm_getFreePages(0, 1);
	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_grant_table;
	xatp.gpfn = (__pa(gnttab_table))>>PAGE_SHIFT;
	ret=HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp);

	if (ret != 0)
	{
		DEBUG(" ERRRO after 3rd mmapping return value zero with test_data:%x  \n",-ret);
		return ;
	}
	//	BUG();


	DEBUG("grant table SUCCES info area sec  : %x  \n", gnttab_table);
}

void
fini_gnttab(void)
{
    struct gnttab_setup_table setup;

    setup.dom = DOMID_SELF;
    setup.nr_frames = 0;

    HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
}
