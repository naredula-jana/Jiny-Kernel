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

#define WITH_BOTTOM_HALF 1
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
static virtio_dev_t *net_dev = 0;
queue_t nbh_waitq;
static int process_pkt(unsigned char *c, unsigned long len) {
	unsigned char ip[5], port[3], tc;
	u16 *pcsum;
	struct virtio_net_hdr *hdr=c;

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
int netbh_started=0;
int max_cont_recv=0;

void net_BH() {
	unsigned long addr, *len;
	int ret;
	int recv = 0;
	int from_sleep = 1;
	netbh_started = 1;

	while (1) {
		//sti();
		len = 0;
		addr = virtio_removeFromQueue(net_dev->vq[0], &len);
		if (addr == 0) {
			if (recv > 0) {
				//virtqueue_kick(net_dev->vq[0]);
				recv = 0;
			}
			recv--;
			virtqueue_enable_cb(net_dev->vq[0]); // enable interrupts as the queue is empty, so interrupt will be recieved for the first packet

#if 1
			if (recv < -3) {
				//if (from_sleep==1)
				//   sc_wait(&nbh_waitq, 100);
				//else
				sc_wait(&nbh_waitq, 10000);
			}
#endif
			from_sleep = 1;
			continue;
		} else {
			if (recv <= 0)
				recv = 0;
		}
		recv++;
		if (recv > max_cont_recv)
			max_cont_recv = recv;
		if (from_sleep == 1) {
			//	virtqueue_disable_cb(net_dev->vq[0]);
			from_sleep = 0;
		}
		//c = addr;
		//DEBUG("%d: new NEW ISR:%d :%x addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n", i, index, isr, addr, len, c[0], c[1], c[2], c[3], c[10], c[11], c[12], c[13]);

		if (process_pkt(addr, len) == 1) {
			netfront_xmit(net_dev, addr, len);
		} else {
			mm_putFreePages(addr, 0);
		}
		addBufToQueue(net_dev->vq[0], 0, 4096);
		//	if ((recv % 10) ==0)
		virtqueue_kick(net_dev->vq[0]);
	}
}

void init_TestUdpStack() {

	net_dev = init_netfront(0, mac, 0);
	if (net_dev ==0){
		ut_printf(" Fail to initialize the UDP stack \n");
		return;
	}
#ifdef WITH_BOTTOM_HALF
	int i,ret;

	sc_register_waitqueue(&nbh_waitq);
	ret=sc_createKernelThread(net_BH,0,"net_rx");
#endif


}
