#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "interface.h"
#include "../virtio.h"
#include "../virtio_ring.h"
#include "../virtio_pci.h"
#include "virtio_net.h"

typedef unsigned int u32;

u16 ip_sum_calc(u16 len_ip_header, unsigned char buff[]) {
	u16 word16;
	u32 sum = 0;
	u16 i;

	// make 16 bit words out of every two adjacent 8 bit words in the packet
	// and add them up
	for (i = 0; i < len_ip_header; i = i + 2) {
		word16 = ((buff[i] << 8) & 0xFF00) + (buff[i + 1] & 0xFF);
		sum = sum + (u32) word16;
	}

	// take only 16 bits out of the 32 bit sum and add up the carries
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	// one's complement the result
	sum = ~sum;

	return ((u16) sum);
}
static unsigned char mac[10];
static void *net_dev = 0;
static int process_pkt(unsigned char *c, unsigned long len) {
	unsigned char ip[5], port[3], tc;
	u16 *pcsum;

	//DEBUG("type of packet :%x \n",h->flags);

	/* ip,udp packet and destination mac matches, then resend back the packet my interchanging ip,udp port and ip checksum recalculate */
	if (c[22] == 0x8 && c[23] == 0 && c[33] == 0x11 && c[10] == mac[0]
			&& c[11] == mac[1] && c[12] == mac[2] && c[13] == mac[3]
			&& c[14] == mac[4] && c[15] == mac[5]) {
		c[10] = c[16];
		c[11] = c[17];
		c[12] = c[18];
		c[13] = c[19];
		c[14] = c[20];
		c[15] = c[21];

		c[16] = mac[0];
		c[17] = mac[1];
		c[18] = mac[2];
		c[19] = mac[3];
		c[20] = mac[4];
		c[21] = mac[5];

		ip[0] = c[36];/* exchange src and dst ip */
		ip[1] = c[37];
		ip[2] = c[38];
		ip[3] = c[39];

		c[36] = c[40];
		c[37] = c[41];
		c[38] = c[42];
		c[39] = c[43];

		c[40] = ip[0];
		c[41] = ip[1];
		c[42] = ip[2];
		c[43] = ip[3];

		c[34] = 0x0; /* ip checksum is made zero */
		c[35] = 0x0;
		pcsum = &c[34];
		*pcsum = ip_sum_calc(20, &c[24]);
		tc = c[34];
		c[34] = c[35];
		c[35] = tc;

		port[0] = c[44];/* exchange src and dst port */
		port[1] = c[45];
		c[44] = c[46];
		c[45] = c[47];
		c[46] = port[0];
		c[47] = port[1];
		return 1;
	} else {
		return 0;
	}
}

static int net_rx(unsigned char *c, unsigned long len) {

	if (process_pkt(c, len) == 1) {
		netfront_xmit(net_dev, c, len);
		mm_putFreePages(c, 0);
	} else {
		mm_putFreePages(c, 0);
	}

	return 1;
}
void init_TestUdpStack() {
	net_dev = init_netfront(net_rx, mac, 0);
}
