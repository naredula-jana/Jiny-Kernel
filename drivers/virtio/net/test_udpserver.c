#define DEBUG_ENABLE 1
#include "common.h"
#include "pci.h"
#include "mm.h"
#include "vfs.h"
#include "task.h"
#include "device.h"

typedef unsigned int u32;
extern int addBufToQueue(struct virtqueue *vq, unsigned char *buf,
		unsigned long len);

static uint16_t ip_sum_calc(uint16_t len_ip_header, unsigned char buff[]) {
	uint16_t word16;
	u32 sum = 0;
	uint16_t i;

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

	return ((uint16_t) sum);
}

static int dummy_pkt_scan = 1;

static int process_pkt(unsigned char *c, unsigned int len, void *private_data) {
	unsigned char ip[5], port[3], tc;
	uint16_t *pcsum;
	int ret = 0;
	device_t *dev = (device_t *)private_data;
	unsigned char *mac=&dev->mac[0];

	ut_printf("Processing the packet by UDP SERVER \n");
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
		pcsum = (uint16_t *) &c[34];
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

		if (dummy_pkt_scan == 0) {
			int cksum = 0;
			int i;
			for (i = 0; i < 1000 && i < len; i++)
				cksum = cksum + c[i];
		}
		ret = 1;
	} else {
		ret = 0;
	}
	netif_tx(c,len);
	//mm_putFreePages((unsigned long) c, 0);
	return 1;
}

int stat_udp_server() {
	return 0;
}


int load_udp_server() {
	static int init = 0;
	if (init == 1)
		return 0;

	registerNetworkHandler(NETWORK_PROTOCOLSTACK, process_pkt, NULL);
	ut_printf("UDPServer : Initilization completed\n");
	return 1;
}
static int unload_udp_server() {
	return 1;
}

//DEFINE_MODULE(test_udp_server, root, load_udp_server, unload_udp_server, stat_udp_server);

static int kkk=0;

