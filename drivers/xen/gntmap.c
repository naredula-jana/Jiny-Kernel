#include "xen.h"

#include <xen/grant_table.h>


struct gntmap {
    int nentries;
    struct gntmap_entry *entries;
};


#define DEFAULT_MAX_GRANTS 128

struct gntmap_entry {
    unsigned long host_addr;
    grant_handle_t handle;
};

static inline int
gntmap_entry_used(struct gntmap_entry *entry)
{
    return entry->host_addr != 0;
}

static struct gntmap_entry*
gntmap_find_free_entry(struct gntmap *map)
{
    int i;

    for (i = 0; i < map->nentries; i++) {
        if (!gntmap_entry_used(&map->entries[i]))
            return &map->entries[i];
    }


    DEBUG("gntmap_find_free_entry(map=%p): all %d entries full\n",
           map, map->nentries);

    return NULL;
}

static struct gntmap_entry*
gntmap_find_entry(struct gntmap *map, unsigned long addr)
{
    int i;

    for (i = 0; i < map->nentries; i++) {
        if (map->entries[i].host_addr == addr)
            return &map->entries[i];
    }
    return NULL;
}

static int
_gntmap_map_grant_ref(struct gntmap_entry *entry, 
                      unsigned long host_addr,
                      uint32_t domid,
                      uint32_t ref,
                      int writable)
{
    struct gnttab_map_grant_ref op;
    int rc;

    op.ref = (grant_ref_t) ref;
    op.dom = (domid_t) domid;
    op.host_addr = (uint64_t) host_addr;
    op.flags = GNTMAP_host_map;
    if (!writable)
        op.flags |= GNTMAP_readonly;

    rc = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);
    if (rc != 0 || op.status != GNTST_okay) {
        printk("GNTTABOP_map_grant_ref failed: "
               "returned %d, status %d \n",
               rc, op.status);
        return rc != 0 ? rc : op.status;
    }

    entry->host_addr = host_addr;
    entry->handle = op.handle;
    return 0;
}

static int
_gntmap_unmap_grant_ref(struct gntmap_entry *entry)
{
    struct gnttab_unmap_grant_ref op;
    int rc;

    op.host_addr    = (uint64_t) entry->host_addr;
    op.dev_bus_addr = 0;
    op.handle       = entry->handle;

    rc = HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1);
    if (rc != 0 || op.status != GNTST_okay) {
        printk("GNTTABOP_unmap_grant_ref failed: "
               "returned %d, status %d\n",
               rc, op.status);
        return rc != 0 ? rc : op.status;
    }

    entry->host_addr = 0;
    return 0;
}

int
gntmap_munmap(struct gntmap *map, unsigned long start_address, int count)
{
    int i, rc;
    struct gntmap_entry *ent;


    DEBUG("gntmap_munmap(map=%p, start_address=%lx, count=%d)\n",
           map, start_address, count);


    for (i = 0; i < count; i++) {
        ent = gntmap_find_entry(map, start_address + PAGE_SIZE * i);
        if (ent == NULL) {
            printk("gntmap: tried to munmap unknown page\n");
            return -1;
        }

        rc = _gntmap_unmap_grant_ref(ent);
        if (rc != 0)
            return rc;
    }

    return 0;
}

void*
gntmap_map_grant_refs(struct gntmap *map, 
                      uint32_t count,
                      uint32_t *domids,
                      int domids_stride,
                      uint32_t *refs,
                      int writable)
{
    unsigned long addr;
    struct gntmap_entry *ent;
    int i;
return 0;
#if 0

    DEBUG("gntmap_map_grant_refs(map=%p, count=%d "
           "domids=%p [%d...], domids_stride=%d, "
           "refs=%p [%d, writable=%d)\n",
           map, count,
           domids, domids == NULL ? 0 : domids[0], domids_stride,
           refs, refs == NULL ? 0 : refs[0], writable);


    (void) gntmap_set_max_grants(map, DEFAULT_MAX_GRANTS);

    addr = allocate_ondemand((unsigned long) count, 1);
    if (addr == 0)
        return NULL;

    for (i = 0; i < count; i++) {
        ent = gntmap_find_free_entry(map);
        if (ent == NULL ||
            _gntmap_map_grant_ref(ent,
                                  addr + PAGE_SIZE * i,
                                  domids[i * domids_stride],
                                  refs[i],
                                  writable) != 0) {

            (void) gntmap_munmap(map, addr, i);
            return NULL;
        }
    }

    return (void*) addr;
#endif
}

void
gntmap_init(struct gntmap *map)
{
#ifdef GNTMAP_DEBUG
    printk("gntmap_init(map=%p)\n", map);
#endif
    map->nentries = 0;
    map->entries = NULL;
}
#if 0
void
gntmap_fini(struct gntmap *map)
{
    struct gntmap_entry *ent;
    int i;

#ifdef GNTMAP_DEBUG
    printk("gntmap_fini(map=%p)\n", map);
#endif

    for (i = 0; i < map->nentries; i++) {
        ent = &map->entries[i];
        if (gntmap_entry_used(ent))
            (void) _gntmap_unmap_grant_ref(ent);
    }

    ut_free(map->entries);
    map->entries = NULL;
    map->nentries = 0;
}
#endif
