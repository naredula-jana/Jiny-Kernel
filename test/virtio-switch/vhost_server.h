/*
 * vhost_server.h
 */

#ifndef VHOST_SERVER_H_
#define VHOST_SERVER_H_

#include "server.h"
#include "vring.h"
#include "stat.h"

typedef struct VhostServerMemoryRegion {
    uint64_t guest_phys_addr;
    uint64_t memory_size;
    uint64_t userspace_addr;
    uint64_t mmap_addr;
} VhostServerMemoryRegion;

typedef struct VhostServerMemory {
    uint32_t nregions;
    VhostServerMemoryRegion regions[VHOST_MEMORY_MAX_NREGIONS];
} VhostServerMemory;

typedef struct VhostServer {
    Server* server;
    VhostServerMemory memory;
    VringTable vring_table;

    int is_polling;
    uint8_t buffer[BUFFER_SIZE];
    uint32_t buffer_size;
    Stat stat;
} VhostServer;

VhostServer* new_vhost_server(const char* path, int is_listen);
int end_vhost_server(VhostServer* vhost_server);
int run_vhost_server(VhostServer* vhost_server,VhostServer* other_vhost_server);

#endif /* VHOST_SERVER_H_ */
