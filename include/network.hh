#ifndef _JINYKERNEL_NETWORK_HH
#define _JINYKERNEL_NETWORK_HH

extern "C" {
#include "common.h"
#include "mm.h"
#include "interface.h"
extern unsigned char g_mac[];
}
#include "file.hh"
#include "jdevice.h"

#if 1
struct machdr{
	uint8_t dest[6];
	uint8_t src[6];
	uint8_t type[2];
}__attribute__((packed));

struct iphdr{
    uint8_t    ihl:4,version:4;
	uint8_t 	tos;
	uint16_t 	tot_len;
	uint16_t 	id;
	uint16_t 	frag_off;
	uint8_t 	ttl;
	uint8_t 	protocol;
	uint16_t 	check;
	uint32_t 	saddr;
	uint32_t 	daddr;
}__attribute__((packed));

struct udphdr{
	uint16_t source;
	uint16_t dest;
	uint16_t len;
	uint16_t checksum;
}__attribute__((packed));


#define IPPROTO_UDP 0x11
struct ether_pkt{
	struct machdr machdr;
	struct iphdr iphdr;
	struct udphdr udphdr;
	unsigned char data;
} __attribute__((packed));
#endif

#define htons(A) ((((uint16_t)(A) & 0xff00) >> 8) | \
(((uint16_t)(A) & 0x00ff) << 8))

#define ntohs(A) htons(A)  /* little indian , intel */

#define htonl(A) ((((uint32_t)(A) & 0xff000000) >> 24) | \
(((uint32_t)(A) & 0x00ff0000) >> 8) | \
(((uint32_t)(A) & 0x0000ff00) << 8) | \
(((uint32_t)(A) & 0x000000ff) << 24))


#include "network_stack.hh"

#define MAX_POLL_DEVICES 5
struct device_under_poll_struct {
	void *private_data;
	int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts);
	int active;
};

class network_scheduler {
	int network_enabled;
	wait_queue *waitq;
	struct device_under_poll_struct device_under_poll[MAX_POLL_DEVICES];
	int poll_underway;
	int netrx_cpuid; /* cpu id where netrx thread runs */
	void *g_netBH_lock; /* All BH code will serialised by this lock */
	int stat_netrx_bh_recvs;

public:
	jdevice *device;
	int init();
	int netRx_thread(void *arg, void *arg2);
	int netRx_BH();
	int netif_rx(unsigned char *data, unsigned int len);
	int netif_rx_enable_polling(void *private_data, int (*poll_func)(void *private_data, int enable_interrupt, int total_pkts));
};

extern class network_scheduler net_sched;
#endif
