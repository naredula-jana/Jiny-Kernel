/* 
 * lwip-net.c
 *
 * Interface to LWIP stack from Jiny
 *
 */

#define __types_h
//#define DEBUG_ENABLE 1
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

static int error_mem=0;
static int error_send_mem=0;
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
	int ret;

#if ETH_PAD_SIZE
	len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

	/* move received packet into a new pbuf */
	mutexLock(g_netBH_lock);
	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	mutexUnLock(g_netBH_lock);
	if (p == NULL) {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		error_mem++;
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

	mutexLock(g_netBH_lock);
	if (ethernet_input(p, netif) != ERR_OK){
		BUG();
		pbuf_free(p);
	}
	mutexUnLock(g_netBH_lock);
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
	//netif->link_speed =;
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

void *lwip_sock_open(int type) {
	void *ret;

	mutexLock(g_netBH_lock);
	if (type == SOCK_DGRAM) {
		ret = netconn_new(NETCONN_UDP);
	} else if (type == SOCK_STREAM ) {
		ret = netconn_new(NETCONN_TCP);
	} else {
		SYSCALL_DEBUG("Error socket Unknown type : %d \n",type);
		ret = 0;
	}
	mutexUnLock(g_netBH_lock);
	return ret;
}

static void lwip_network_status(unsigned char *arg1,unsigned char *arg2){
	struct netif *netif;

	netif = the_interface;
	if (netif ==0) return;

	ut_printf(" ip:%x gw:%x mask:%x \n",netif->ip_addr, netif->gw, netif->netmask);
	ut_printf("mac_addr : %x:%x:%x:%x:%x:%x \n",netif->hwaddr[0],netif->hwaddr[1],netif->hwaddr[2],netif->hwaddr[3],netif->hwaddr[4],netif->hwaddr[5]);
	ut_printf(" mem Errors: %d  send mem errors:%d\n",error_mem,error_send_mem);
	#if LWIP_SNMP
	ut_printf("in_bytes:%d out_bytes:%d inpkts:%d outpkts:%d  \n",netif->ifinoctets,netif->ifoutoctets,netif->ifinucastpkts,netif->ifoutucastpkts);
	#endif

	ut_printf(" link: Tx:%d Rx:%d drop:%d\n IP: Tx:%d Rx:%d drop:%d\nUDP: Tx:%d Rx:%d drop:%d\n",lwip_stats.link.xmit,lwip_stats.link.recv,lwip_stats.link.drop,lwip_stats.ip.xmit,lwip_stats.ip.recv,lwip_stats.ip.drop,lwip_stats.udp.xmit,lwip_stats.udp.recv,lwip_stats.udp.drop);
}
int lwip_sock_read_from(void *conn, unsigned char *buff, unsigned long len,struct sockaddr *sockaddr, int addr_len) {
	struct netbuf *new_buf=0;
	unsigned char *data;
	int data_len=0;
	int ret=0;

	SYSCALL_DEBUG(" SOCK recvfrom :%x len:%d \n",buff,len);

	mutexLock(g_netBH_lock);
	ret=netconn_recv(conn,  &new_buf);
	mutexUnLock(g_netBH_lock);
if (ret == ERR_TIMEOUT){
	if (g_current_task->killed == 1){
		return 0;
	}
}

	if (ret!=ERR_OK){
		SYSCALL_DEBUG(" Fail to recvfrom data: %x newret:%x(%d) \n",ret,-ret,-ret);
		return 0;
	}
	SYSCALL_DEBUG(" SUCESS to recv data:%d \n",ret);
	netbuf_data(new_buf,&data,&data_len);
	if (data_len >  0){
		if (sockaddr != 0){
			sockaddr->addr = new_buf->addr.addr;
			sockaddr->sin_port = new_buf->port;
		}
		ut_memcpy(buff,data,ut_min(data_len,len));
		ret = ut_min(data_len,len);
	}else{
		ret =0;
	}

	mutexLock(g_netBH_lock);
	netbuf_delete(new_buf);
	mutexUnLock(g_netBH_lock);

	return ret;
}
int lwip_sock_check(void *conn_p){
	int  recv_bytes=0;
	//lwip_ioctl(conn,FIONREAD,&recv_bytes);
	struct netconn *conn=conn_p;
	if (conn==0) return 0;
	return ((conn->acceptmbox.read_sem.count) || (conn->recvmbox.read_sem.count));
}
int lwip_sock_read(void *conn, unsigned char *buff, unsigned long len) {
	struct netbuf *new_buf=0;
	unsigned char *data;
	int data_len=0;
	int ret,newret;

	SYSCALL_DEBUG(" SOCK read :%x len:%d \n",buff,len);
	mutexLock(g_netBH_lock);
	ret=netconn_recv(conn, &new_buf);
	mutexUnLock(g_netBH_lock);

	if (ret!=ERR_OK){
		SYSCALL_DEBUG(" Fail to recv data: %x newret:%x(%d) \n",ret,-ret,-ret);
		return 0;
	}

	netbuf_data(new_buf,&data,&data_len);
	SYSCALL_DEBUG(" SUCESS to recv data:%d  ret:%d\n",data_len,ret);
	if (data_len >  0){
		ut_memcpy(buff,data,ut_min(data_len,len));
		ret = ut_min(data_len,len);
	}else{
		ret = 0;
	}

	mutexLock(g_netBH_lock);
	netbuf_delete(new_buf);
	mutexUnLock(g_netBH_lock);

	return ret;
}
int lwip_sock_connect(void *conn, unsigned long *addr, uint16_t port) {
	int ret;

	mutexLock(g_netBH_lock);
	ret = netconn_connect(conn, addr, lwip_ntohs(port));
	mutexUnLock(g_netBH_lock);
	return ret;
}
int lwip_sock_write(void *conn, unsigned char *buff, unsigned long len, int type, uint32_t daddr, uint16_t dport) {
	int ret;

	mutexLock(g_netBH_lock);
	if (type == SOCK_DGRAM) {
		ip_addr_t addr;
		struct netbuf *buf;
		char * data;
		SYSCALL_DEBUG("SOCKDGRAM  WRITING len : %d : daddr:%x dport:%x \n",len,daddr,dport);

		buf = netbuf_new();
		if (buf ==0){
			error_send_mem++;
			ret = 0;
			goto last;
		}
		data = netbuf_alloc(buf, len+1);
		ut_memcpy(data, buff, len);
		addr.addr = daddr;
		netconn_sendto(conn, buf,&addr,dport);
		netbuf_delete(buf);
		ret = len;
	} else if (type == SOCK_STREAM) {
		SYSCALL_DEBUG("SOCKSTRAEM  WRITING len : %d : \n",len);
		ret = netconn_write(conn, buff, len, NETCONN_COPY);
		if (ret == ERR_OK){
			ret = len;
		}
	}
last:
	mutexUnLock(g_netBH_lock);
	return ret;
}
int lwip_sock_close(void *session,int sock_type) {
	mutexLock(g_netBH_lock);
//	if (sock_type == SOCK_STREAM){
		netconn_disconnect(session);
//	}
	netconn_delete(session);
	mutexUnLock(g_netBH_lock);

	return SYSCALL_SUCCESS;
}
int lwip_get_addr(void *session, struct sockaddr *s){

	if (netconn_getaddr(session,&s->addr,&s->sin_port, 1) == ERR_OK)
		return SYSCALL_SUCCESS;
	else
		return SYSCALL_FAIL;
}
int lwip_sock_bind(void *session, struct sockaddr *s, int sock_type) {
	struct ip_addr listenaddr = { s->addr }; /* TODO */

	mutexLock(g_netBH_lock);
	int rc = netconn_bind(session, &listenaddr, ntohs(s->sin_port));
	mutexUnLock(g_netBH_lock);

	if (rc != ERR_OK) {
		DEBUG("Failed to bind connection: %i\n", rc);
		return SYSCALL_FAIL;
	}
	if (sock_type == SOCK_DGRAM){
		return SYSCALL_SUCCESS;
	}

	mutexLock(g_netBH_lock);
	rc = netconn_listen(session);
	mutexUnLock(g_netBH_lock);
	if (rc != ERR_OK) {
		DEBUG("Failed to listen on connection: %i\n", rc);
		return SYSCALL_FAIL;
	}
	return SYSCALL_SUCCESS;
}
void *lwip_sock_accept(void *listener, struct sockaddr *s) {
	void *ret;

	mutexLock(g_netBH_lock);
	netconn_accept(listener, &ret);
	mutexUnLock(g_netBH_lock);

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
extern int g_network_ip;
static int load_LwipTcpIpStack() {
	//mac = 00:30:48:DB:5E:06
	unsigned char mac[7] = { 0x00, 0x30, 0x48, 0xDB, 0x5E, 0x06, 0x0 };
	struct netif *netif;
	struct ip_addr ipaddr = { htonl(IF_IPADDR) };
	struct ip_addr netmask = { htonl(IF_NETMASK) };
	struct ip_addr gw = { 0 };
	char *ip = NULL;
	static int network_started = 0;
	ut_printf("Before starting the LWIP load network_started: %x  address:%x \n",network_started,&network_started);
	if (network_started != 0)
		return 0;
	network_started = 1;
	ut_printf(" starting the LWIP load\n");
	ut_log("LWIP Module: Waiting for network.\n");
	tcpip_is_up.name = "sem_tcpip";
	ipc_sem_new(&tcpip_is_up, 0 ); /* TODO : need to free the sem */

	ut_log("	LWIP: IP %x netmask %x gateway %x.\n", ntohl(ipaddr.addr), ntohl(netmask.addr), ntohl(gw.addr));
	ut_log("	LWIP: TCP/IP bringup begins.\n");

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
		struct ip_addr ipaddr = { htonl(g_network_ip) };
		struct ip_addr netmask = { htonl(0xffffff00) };
		struct ip_addr gw = { htonl(0x0ad18001) };
		networking_set_addr(&ipaddr, &netmask, &gw);
	}
	registerNetworkHandler(NETWORK_PROTOCOLSTACK, lwip_netif_rx, NULL);
	ut_log("	LWIP: Latest Network is ready with IP.\n");
	//start_webserver();

	lwip_socket_api.read = lwip_sock_read;
	lwip_socket_api.read_from = lwip_sock_read_from;
	lwip_socket_api.write = lwip_sock_write;
	lwip_socket_api.close = lwip_sock_close;
	lwip_socket_api.bind = lwip_sock_bind;
	lwip_socket_api.accept = lwip_sock_accept;
	lwip_socket_api.open = lwip_sock_open;
	lwip_socket_api.connect = lwip_sock_connect;
	lwip_socket_api.network_status = lwip_network_status;
	lwip_socket_api.get_addr = lwip_get_addr;
	lwip_socket_api.check_data = lwip_sock_check;
	register_to_socketLayer(&lwip_socket_api);
	ut_printf("Lwip Initialization completed \n");
	return 1;
}
static void init_module(){
	load_LwipTcpIpStack();
}
static void clean_module(){
	unregisterNetworkHandler(NETWORK_PROTOCOLSTACK, lwip_netif_rx, NULL);
	unregister_to_socketLayer();
	ut_printf("inside the clean module func\n");
}


//DEFINE_MODULE(LwipTcpIpStack, root, load_LwipTcpIpStack, unload_LwipTcpIpStack, stat_LwipTcpIpStack);

