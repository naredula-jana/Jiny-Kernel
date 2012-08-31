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
//#define SEND_BH 1
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
int dummy_pkt_scan = 0;
queue_t nbh_waitq;
static int process_pkt(unsigned char *c, unsigned long len) {
	unsigned char ip[5], port[3], tc;
	u16 *pcsum;
	struct virtio_net_hdr *hdr = c;

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

		if (dummy_pkt_scan == 1) {
			int cksum = 0;
			int i;
			for (i = 0; i < 2000 && i < len; i++)
				cksum = cksum + c[i];
		}
		return 1;
	} else {
		return 0;
	}
}
int netbh_started = 0;
static int max_cont_recv = 0;
static int stat_recv = 0;
static int went_sleep = 0;
static int stat_sleep = 0;
static int recv_count = 0;
int g_conf_netbh_poll = 0;
int g_conf_udp_respond = 0;
int netbh_state = 0;
extern int virtio_send_errors;
int netbh_flag = 0;

int stat_netcpus[10];
void print_udpserver() {
	ut_printf(
			"New recv:%d max_recv:%d wsleep:%d sleep:%d total recv:%d snd_err:%d cpus:%d:%d\n",
			stat_recv, max_cont_recv, went_sleep, stat_sleep, recv_count,
			virtio_send_errors, stat_netcpus[0], stat_netcpus[1]);
}
#define MAX_QUEUE_SIZE 200
static struct {
	unsigned long addr;
	int len;
} sendq[MAX_QUEUE_SIZE];

static int put_index = 0;
static int recv_index = 0;
void send_net_BH() {
	int i;

	for (i = 0; i < MAX_QUEUE_SIZE; i++) {
		sendq[i].addr = 0;
	}
	while (1) {
		if (sendq[recv_index].addr == 0)
			continue;
		netfront_xmit(net_dev, sendq[recv_index].addr, sendq[recv_index].len);
		sendq[recv_index].addr = 0;
		recv_index++;
		if (recv_index >= MAX_QUEUE_SIZE)
			recv_index = 0;
	}
}
void net_BH() {
	unsigned long addr, *len;
	int ret, i;
	int consumed_pkts = 0;

	int from_sleep = 1;
	netbh_started = 1;
	stat_netcpus[0] = stat_netcpus[1] = 0;
	while (1) {
		len = 0;
		netbh_state = 100;
		netbh_flag = 0;
		addr = virtio_removeFromQueue(net_dev->vq[0], &len);
		if (addr == 0) {
			netbh_state = 200;

			if (g_conf_netbh_poll == 0) {
				sti();
				netbh_state = 202;
				stat_sleep++;
				virtqueue_enable_cb(net_dev->vq[0]); // enable interrupts as the queue is empty, so interrupt will be recieved for the first packet
				if (netbh_flag != 0)
					continue;
				sc_wait(&nbh_waitq, 2);
				if (g_current_task->cpu < 9)
					stat_netcpus[g_current_task->cpu]++;
				virtqueue_disable_cb(net_dev->vq[0]);
				netbh_state = 203;
			} else {
				cli();
			}
			if (stat_recv > 0) {
				went_sleep++;
			}
			stat_recv = 0;

			from_sleep = 1;
			continue;
		} else {
			cli();
		}
		stat_recv++;

		if (stat_recv > max_cont_recv)
			max_cont_recv = stat_recv;
		if (from_sleep == 1) {
			from_sleep = 0;
		}
		//c = addr;
		//DEBUG("%d: new NEW ISR:%d :%x addd:%x len:%x c:%x:%x:%x:%x  c+10:%x:%x:%x:%x\n", i, index, isr, addr, len, c[0], c[1], c[2], c[3], c[10], c[11], c[12], c[13]);

		if (process_pkt(addr, len) == 1) {
			recv_count++;
			netbh_state = 301;
			if (g_conf_udp_respond == 1) {
#if SEND_BH
				if (sendq[put_index].addr == 0) {
					sendq[put_index].len = len;
					sendq[put_index].addr = addr;
					put_index++;
					if (put_index >= MAX_QUEUE_SIZE)
					put_index = 0;
				} else {
					mm_putFreePages(addr, 0);
				}
#else
				netfront_xmit(net_dev, addr, len);
#endif
			} else {
				mm_putFreePages(addr, 0);
			}
			netbh_state = 320;
		} else {
			mm_putFreePages(addr, 0);
		}

#if 1
		netbh_state = 403;
		addBufToQueue(net_dev->vq[0], 0, 4096);
		if ((recv_count % 10) == 0) {
			netbh_state = 404;
			virtqueue_kick(net_dev->vq[0]);
		}
#else
		consumed_pkts++;
		if (consumed_pkts > 150) {
			netbh_state = 402;
			for (i = 0; i < 50; i++) {
				addBufToQueue(net_dev->vq[0], 0, 4096);
				consumed_pkts--;
			}
			//	if ((recv % 10) ==0)
			virtqueue_kick(net_dev->vq[0]);
		}
#endif
	}
}

void init_TestUdpStack() {

	net_dev = init_netfront(0, mac, 0);
	if (net_dev == 0) {
		ut_printf(" Fail to initialize the UDP stack \n");
		return;
	}
#ifdef WITH_BOTTOM_HALF
	int i, ret;

	sc_register_waitqueue(&nbh_waitq);
	ret = sc_createKernelThread(net_BH, 0, "net_rx");
#ifdef SEND_BH
	ret=sc_createKernelThread(send_net_BH,0,"net_send_bh");
#endif
#endif
}
