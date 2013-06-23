/* 
 * lwip-net.c
 *
 * interface between lwIP's ethernet and Mini-os's netfront.
 * For now, support only one network interface, as mini-os does.
 *
 * Tim Deegan <Tim.Deegan@eu.citrix.net>, July 2007
 * based on lwIP's ethernetif.c skeleton file, copyrights as below.
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#define __types_h
#define DEBUG_ENABLE 1
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/tcp.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>

#include "netif/etharp.h"

#include <lwip/netif.h>

#include "common.h"
#include "device.h"

#define down(x) sys_arch_sem_wait(x,100)
/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#define IF_IPADDR	0x00000000
#define IF_NETMASK	0x00000000

/* Only have one network interface at a time. */
static struct netif *the_interface = NULL;

/* Forward declarations. */
static err_t netfront_output(struct netif *netif, struct pbuf *p,
		struct ip_addr *ipaddr);

/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
	DEBUG("LWIP Lowlevel output \n");
#ifdef ETH_PAD_SIZE
	pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

	/* Send the data from the pbuf to the interface, one pbuf at a
	 time. The size of the data in each pbuf is kept in the ->len
	 variable. */
	if (!p->next) {
		/* Only one fragment, can send it directly */
		netif_tx(p->payload, p->len);
	} else {
		unsigned char data[p->tot_len], *cur;
		struct pbuf *q;

		for (q = p, cur = data; q != NULL; cur += q->len, q = q->next)
			memcpy(cur, q->payload, q->len);
		netif_tx(data, p->tot_len);
	}

#if ETH_PAD_SIZE
	pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

	LINK_STATS_INC(link.xmit);

	return ERR_OK;
}

/*
 * netfront_output():
 *
 * This function is called by the TCP/IP stack when an IP packet
 * should be sent. It calls the function called low_level_output() to
 * do the actual transmission of the packet.
 *
 */

static err_t netfront_output(struct netif *netif, struct pbuf *p,
		struct ip_addr *ipaddr) {

	/* resolve hardware address, then send (or queue) packet */
	return etharp_output(netif, p, ipaddr);

}

/*
 * netfront_input():
 *
 * This function should be called when a packet is ready to be read
 * from the interface. 
 *
 */

static void netfront_input(struct netif *netif, unsigned char* data, int len) {
	struct eth_hdr *ethhdr;
	struct pbuf *p, *q;

#if ETH_PAD_SIZE
	len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

	/* move received packet into a new pbuf */
	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	if (p == NULL) {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		return;
	}

#if ETH_PAD_SIZE
	pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

	/* We iterate over the pbuf chain until we have read the entire
	 * packet into the pbuf. */
	for (q = p; q != NULL && len > 0; q = q->next) {
		/* Read enough bytes to fill this pbuf in the chain. The
		 * available data in the pbuf is given by the q->len
		 * variable. */
		ut_memcpy(q->payload, data, len < q->len ? len : q->len);
		data += q->len;
		len -= q->len;
	}

#if ETH_PAD_SIZE
	pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

	LINK_STATS_INC(link.recv);

	/* points to packet payload, which starts with an Ethernet header */
	ethhdr = p->payload;

	ethhdr = p->payload;
	ethernet_input(p, netif);
#if 0
	switch (htons(ethhdr->type)) {
		/* IP packet? */
		case ETHTYPE_IP:
		/* skip Ethernet header */
		DEBUG("RECVED THE IP packet send to LWIP for processing\n");
		pbuf_header(p, -(int16_t)sizeof(struct eth_hdr));
		/* pass to network layer */
		if (tcpip_input(p, netif) == ERR_MEM)
		/* Could not store it, drop */
		pbuf_free(p);
		break;

		case ETHTYPE_ARP:
		/* pass p to ARP module  */
		DEBUG("RECVED THE ARP packet send to LWIP for processing\n");
		etharp_arp_input(netif, (struct eth_addr *) netif->hwaddr, p);
		break;

		default:
		DEBUG("RECVED THE UNKNOWN packet send to LWIP for processing\n");
		pbuf_free(p);
		p = NULL;
		break;
	}
#endif
}

static int lwip_netif_rx(unsigned char* data, unsigned int len,
		void *private_data) {
	static int stat_recv = 0;
	stat_recv++;
	if (the_interface != NULL) {
		DEBUG("Sending packet to tcpip layer :%d\n", len);
		netfront_input(the_interface, data + 10, len);
	}
	DEBUG("%d bytes incoming at %x  stat_recv:%d\n", len, data, stat_recv);
	return 1;
}

/*
 * Set the IP, mask and gateway of the IF
 */
void networking_set_addr(struct ip_addr *ipaddr, struct ip_addr *netmask,
		struct ip_addr *gw) {
	netif_set_ipaddr(the_interface, ipaddr);
	netif_set_netmask(the_interface, netmask);
	netif_set_gw(the_interface, gw);
}

static void arp_timer(void *arg) {
	etharp_tmr();
	sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
}

/*
 * netif_netfront_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */

err_t netif_netfront_init(struct netif *netif) {
	unsigned char *mac = netif->state;
	ut_printf("LWIP: netfront init called \n ");
#if LWIP_SNMP
	/* ifType ethernetCsmacd(6) @see RFC1213 */
	netif->link_type = 6;
	/* your link speed here */
	netif->link_speed =;
	netif->ts = 0;
	netif->ifinoctets = 0;
	netif->ifinucastpkts = 0;
	netif->ifinnucastpkts = 0;
	netif->ifindiscards = 0;
	netif->ifoutoctets = 0;
	netif->ifoutucastpkts = 0;
	netif->ifoutnucastpkts = 0;
	netif->ifoutdiscards = 0;
#endif

	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	netif->output = netfront_output;
	netif->linkoutput = low_level_output;

	the_interface = netif;

	/* set MAC hardware address */
	netif->hwaddr_len = 6;
	netif->hwaddr[0] = mac[0];
	netif->hwaddr[1] = mac[1];
	netif->hwaddr[2] = mac[2];
	netif->hwaddr[3] = mac[3];
	netif->hwaddr[4] = mac[4];
	netif->hwaddr[5] = mac[5];

	/* No interesting per-interface state */
	netif->state = NULL;

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* broadcast capability */
	netif->flags = NETIF_FLAG_BROADCAST;
	etharp_init();
	sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
	return ERR_OK;
}
enum sock_type {
          SOCK_STREAM     = 1,
          SOCK_DGRAM      = 2
};
void *lwip_sock_open(int type) {
	if (type == SOCK_DGRAM) {
		return netconn_new(NETCONN_UDP);
	} else if (type == SOCK_STREAM) {
		return netconn_new(NETCONN_TCP);
	} else {
		return 0;
	}
}

int lwip_sock_read_from(void *conn, unsigned char *buff, unsigned long len,uint32_t *addr, uint16_t port) {
	struct netbuf *new_buf=0;
	unsigned char *data;
	int data_len=0;
	int ret;

	ut_log(" SOCK read :%x len:%d \n",buff,len);

repeat:
	ret=netconn_recv(conn,  &new_buf);
	if (ret!=ERR_OK){
		sc_sleep(50);
		goto repeat;
		//return 0;
	}
	netbuf_data(new_buf,&data,&data_len);
	if (data_len >  0){
		ut_memcpy(buff,data,ut_min(data_len,len)); //TODO: need to free the memory
		return ut_min(data_len,len);
	}
	return 0;
}
int lwip_sock_read(void *conn, unsigned char *buff, unsigned long len) {
	struct netbuf *new_buf=0;
	unsigned char *data;
	int data_len=0;

	int ret;

	ut_log(" SOCK read :%x len:%d \n",buff,len);
	ret=netconn_recv(conn,  &new_buf);
	if (ret!=ERR_OK){
		return 0;
	}
	netbuf_data(new_buf,&data,&data_len);
	if (data_len >  0){
		ut_memcpy(buff,data,ut_min(data_len,len)); //TODO: need to fre the memory
		return ut_min(data_len,len);
	}
	return 0;
}
int lwip_sock_connect(void *conn, unsigned long *addr, uint16_t port) {
	return netconn_connect(conn, addr, lwip_ntohs(port));
}
int lwip_sock_write(void *conn, unsigned char *buff, unsigned long len, int type) {
	if (type == SOCK_DGRAM) {
		struct netbuf *buf;
		char * data;

		buf = netbuf_new();
		data = netbuf_alloc(buf, len+1);
		ut_memcpy(data, buff, len);
		netconn_send(conn, buf);
		netbuf_delete(buf);
		return len;
	} else if (type == SOCK_STREAM) {
		return netconn_write(conn, buff, len, NETCONN_COPY);
	}
	return 0;
}
int lwip_sock_close(void *session) {
	netconn_disconnect(session);
	netconn_delete(session);
	return 1;
}
int lwip_sock_bind(void *session, struct sockaddr *s, int sock_type) {
	struct ip_addr listenaddr = { s->addr }; /* TODO */
	int rc = netconn_bind(session, &listenaddr, (s->sin_port));
	if (rc != ERR_OK) {
		DEBUG("Failed to bind connection: %i\n", rc);
		return SYSCALL_FAIL;
	}
	if (sock_type == SOCK_DGRAM){
		return SYSCALL_SUCCESS;
	}
	rc = netconn_listen(session);
	if (rc != ERR_OK) {
		DEBUG("Failed to listen on connection: %i\n", rc);
		return SYSCALL_FAIL;
	}
	return SYSCALL_SUCCESS;
}
void *lwip_sock_accept(void *listener, struct sockaddr *s) {
	void *ret;
	netconn_accept(listener, &ret);
	return ret;
}
/*
 * Thread run by netfront: bring up the IP address and fire lwIP timers.
 */
//static __DECLARE_SEMAPHORE_GENERIC(tcpip_is_up, 0); TODO : need to check correctness by initialising in start_networking
static struct semaphore tcpip_is_up;
static void tcpip_bringup_finished(void *p) {
	DEBUG("TCP/IP bringup ends.\n");
	sys_sem_signal(&tcpip_is_up);
}
struct Socket_API lwip_socket_api;
// TODO  commented tcp checksum otherwise it is failing src/include/lwip/opt.h:1397:// JANA #define CHECKSUM_CHECK_TCP

/* 
 * Utility function to bring the whole lot up.  Call this from app_main() 
 * or similar -- it starts netfront and have lwIP start its thread,
 * which calls back to tcpip_bringup_finished(), which 
 * lets us know it's OK to continue.int lwip_sock_read
 */
int load_LwipTcpIpStack() {
	//mac = 00:30:48:DB:5E:06
	unsigned char mac[7] = { 0x00, 0x30, 0x48, 0xDB, 0x5E, 0x06, 0x0 };
	struct netif *netif;
	struct ip_addr ipaddr = { htonl(IF_IPADDR) };
	struct ip_addr netmask = { htonl(IF_NETMASK) };
	struct ip_addr gw = { 0 };
	char *ip = NULL;
	static int network_started = 0;
	if (network_started != 0)
		return 0;
	network_started = 1;

	DEBUG("LWIP: Waiting for network.\n");
	tcpip_is_up.name = "sem_tcpip";
	ipc_sem_new(&tcpip_is_up, 0 ); /* TODO : need to free the sem */

	DEBUG(
			"LWIP: IP %x netmask %x gateway %x.\n", ntohl(ipaddr.addr), ntohl(netmask.addr), ntohl(gw.addr));

	DEBUG("LWIP: TCP/IP bringup begins.\n");

	netif = mm_malloc(sizeof(struct netif), 0);

	tcpip_init(tcpip_bringup_finished, netif);

	netif_add(netif, &ipaddr, &netmask, &gw, mac, netif_netfront_init,
			ip_input);
	netif_set_default(netif);
	netif_set_up(netif);
	netif->flags = netif->flags | NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP
			| NETIF_FLAG_LINK_UP;
	//down(&tcpip_is_up);

	if (1) {
		struct ip_addr ipaddr = { htonl(0x0ad181b0) }; /* 10.209.129.176  */
		struct ip_addr netmask = { htonl(0xffffff00) };
		struct ip_addr gw = { htonl(0x0ad18001) };
		networking_set_addr(&ipaddr, &netmask, &gw);
	}
	registerNetworkHandler(NETWORK_PROTOCOLSTACK, lwip_netif_rx, NULL);
	DEBUG("LWIP: Latest Network is ready with IP.\n");
	start_webserver();

	lwip_socket_api.read = lwip_sock_read;
	lwip_socket_api.read_from = lwip_sock_read_from;
	lwip_socket_api.write = lwip_sock_write;
	lwip_socket_api.close = lwip_sock_close;
	lwip_socket_api.bind = lwip_sock_bind;
	lwip_socket_api.accept = lwip_sock_accept;
	lwip_socket_api.open = lwip_sock_open;
	lwip_socket_api.connect = lwip_sock_connect;
	register_to_socketLayer(&lwip_socket_api);
	return 1;
}

/* Shut down the network */
int unload_LwipTcpIpStack() {
	return 1;
}
int stat_LwipTcpIpStack() {
	return 0;
}

DEFINE_MODULE(LwipTcpIpStack, root, load_LwipTcpIpStack, unload_LwipTcpIpStack, stat_LwipTcpIpStack);

