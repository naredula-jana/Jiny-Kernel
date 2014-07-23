

typedef unsigned char uint8_t;
extern "C" {
#include "uip.h"
#include "uip_arp.h"
#include "uip_arch.h"
#include "uip-fw.h"
#include "jiny_uip.h"

void uip_arp_out(void);
void ut_printf(const char *format, ...);
void ut_log(const char *format, ...);
void ut_memcpy(uint8_t *dest, uint8_t *src, long len);
void ut_memset(uint8_t *dest, uint8_t val, long len);

extern int net_send_eth_frame(unsigned char *buf, int len, int write_flags);
extern unsigned char *jalloc_page(int flags);
extern int jfree_page(unsigned char *p);
}

#include "network_stack.hh"
#define JSUCCESS 1  /* TODO: need removed , redifined */
#define JFAIL 0
#define DEBUG

int network_stack::open(network_connection *conn, int flags) {
	struct uip_udp_conn_struct *uip_conn;
	uip_conn = uip_udp_new(0, 0);
	conn->proto_connection = uip_conn;
	conn->src_port = uip_conn->lport;

	return JSUCCESS;
}
int network_stack::close(network_connection *conn) {
	void *uip_conn = conn->proto_connection;
	if (uip_conn != 0) {
		DEBUG(" closing the uip conn sock: %x  uip:%x \n",conn,uip_conn);
		uip_close();
		conn->proto_connection = 0;
	}
	return JSUCCESS;
}

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])
#define UDPBUF ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])
int network_stack::read(network_connection *conn, uint8_t *raw_data, int raw_len, uint8_t *app_data, int app_maxlen) {
	int ret = JFAIL;
	int pkt_len;
	unsigned char *jbuf=0;

	netstack_lock();
	DEBUG("new UIP raw received length :%d  conn:%x   ..... :uip_conn:%x \n", raw_len,conn,uip_conn);
	uip_len = raw_len - 10;
	pkt_len = uip_len;
	if (uip_buf!=0){
		while(1);
	}
	jbuf = (unsigned char *) jalloc_page(0);
	if (jbuf ==0) {
		goto last;
	}else{
		uip_buf = jbuf+10;;
		ut_memset(jbuf,0,10);
	}

	ut_memcpy(uip_buf, raw_data + 10, uip_len);

	if (conn != 0 && conn->protocol == IPPROTO_UDP) {
		uip_appdata = 0;
		uip_input();
		if (uip_appdata) {
			ret = pkt_len - (UIP_LLH_LEN + UIP_IPUDPH_LEN);
			ut_memcpy(app_data, uip_appdata, ret);
			conn->dest_port = UDPBUF->srcport;
			ut_memcpy((unsigned char *)&(conn->src_ip), (unsigned char *)&(UDPBUF->destipaddr[0]),4);
			ut_memcpy((unsigned char *)&(conn->dest_ip), (unsigned char *)&(UDPBUF->srcipaddr[0]),4);
			jfree_page(jbuf);
			jbuf =0 ;
		} else {
			ret = JFAIL;
		}
		uip_len = 0;
		goto last;
	}else if (conn != 0 && conn->protocol == IPPROTO_TCP){
// TODO
	}else if (conn == 0 ){

	}

	/* packete not related to any connection like arp etc */
	if (BUF->type == HTONS(UIP_ETHTYPE_IP)) {
		uip_arp_ipin();
		uip_input();
		if (uip_len > 0) {
			uip_arp_out();
			DEBUG("FROM READ .... buf :%x  replied with the packet len : %d \n",uip_buf,uip_len);
			ret = net_send_eth_frame(uip_buf, uip_len,WRITE_BUF_CREATED);
			uip_len = 0;
			goto last;
		}
	} else if (BUF->type == HTONS(UIP_ETHTYPE_ARP)) {
		uip_arp_arpin();
		if (uip_len > 0) {
			DEBUG(" replied with the ARP   packet len : %d \n",uip_len);
			ret = net_send_eth_frame(uip_buf, uip_len,WRITE_BUF_CREATED);
			uip_len = 0;
		}
	}

last:
	uip_buf=0;
	if (ret == JFAIL && jbuf){
		jfree_page(jbuf);
	}
	netstack_unlock();
	return ret;
}

int network_stack::write(network_connection *conn, uint8_t *app_data, int app_len) {
	int ret = JFAIL;
	unsigned char *buf=0;
	uint16_t port;
	uint32_t ip;
	netstack_lock();
	if (uip_buf!=0){
		while(1);
	}
	if (conn == 0 || conn->proto_connection == 0) {
		goto last;
	}

	buf = (unsigned long) jalloc_page(0);
	if (buf ==0) {
		goto last;
	}else{
		uip_buf = buf+10;
		ut_memset(buf,0,10);
	}
	if (conn->protocol == IPPROTO_UDP) {
		uip_udp_conn = conn->proto_connection;
		ip = conn->dest_ip;
		port = conn->dest_port;
		uip_udp_conn->rport = port;

		uip_udp_conn->ripaddr[0] = ip & 0xffff;
		uip_udp_conn->ripaddr[1] = (ip >> 16) & 0xffff;

		uip_slen = app_len;
		uip_process(UIP_UDP_SEND_CONN);
		ut_memcpy(uip_appdata, app_data, app_len);

		if (uip_len > 0) {
			uip_arp_out();
			DEBUG("FROM WRITE ... - uip SENDTO pkt: buf: %x  %d applen :%d \n", uip_buf, uip_len, app_len);
			ret = net_send_eth_frame(uip_buf, uip_len,WRITE_BUF_CREATED);
		}
		uip_len = 0;
		uip_slen = 0;
		uip_conn = 0;
		uip_udp_conn = 0;
	}else if (conn->protocol == IPPROTO_TCP){

	}

last:
	uip_buf=0;
	if (ret == JFAIL && buf){
		jfree_page(buf);
	}
	netstack_unlock();
	return ret;
}
int network_stack::bind(network_connection *conn, uint16_t port) {
	uip_listen(port);
	ut_log(" uip_mod : binding on the port :%d  HTONS:%d \n", port, HTONS(port));
	return 1;
}
int network_stack::connect(network_connection *conn) { // only for TCP
	u16_t test[2];
	int ret = JFAIL;
	int i;
	unsigned data[20];

	if (conn->protocol != IPPROTO_TCP) {
		ret = JFAIL;
		goto last;
	}

	test[0] = conn->dest_ip & 0xffff;
	test[1] = (conn->dest_ip >> 16) & 0xffff;

	uip_conn = uip_connect(&test, conn->dest_port);
	uip_process(UIP_TIMER);
	if (uip_len > 0) {
		ut_log(" tcp connect  message : %d \n",uip_len);
		ret = net_send_eth_frame(uip_buf, uip_len,0);
		uip_len = 0;
	}
last:
	return ret;
}
network_stack uip_stack;

extern "C" {
void net_get_mac(unsigned char *mac);
void uip_mod_udpappcall(void) { /* NOT used */

}
void uip_mod_appcall(void) {
	if (uip_connected()) {
		ut_log(" socket connected :%d \n", uip_len);

	}
	if (uip_len > 0) {
		ut_log(" Data len :%d :%s:\n", uip_len, uip_appdata);
		return;
	}

	return;
}
unsigned char g_conf_ipaddr[40];
unsigned char g_conf_gw[40];
uint8_t *ut_strcpy(uint8_t *dest, const uint8_t *src);
unsigned int ut_atoi(uint8_t *p, int format);
int get_ip(unsigned char *str, int loc) {
	unsigned char ip[50];
	int i,ret;
	unsigned char *start;
	int dots = 0;

	ut_strcpy(ip, str);
	start = ip;
	for (i = 0; i < 50; i++) {
		if (ip[i] == '.' || ip[i]=='\0') {
			dots++;
			if (dots == loc) {
				ip[i] = '\0';
				ret = ut_atoi(start, 2);
				//ut_log(" ip addr %d :%d\n",loc,ret);
				return ret;
			}
			start = &ip[i+1];
			ip[i] = '\0';
		}
	}
}
void *init_netmod_uipstack() {
	struct uip_eth_addr mac_addr;
	uip_stack.name = "UIP stack";
	int i1,i2,i3,i4;

	ut_log(" ipaddr :%s:  gw:%s: \n",g_conf_ipaddr,g_conf_gw);
	uip_ipaddr_t ipaddr;
	uip_init();

	i1=get_ip(g_conf_ipaddr,1);
	i2=get_ip(g_conf_ipaddr,2);
	i3=get_ip(g_conf_ipaddr,3);
	i4=get_ip(g_conf_ipaddr,4);
	uip_ipaddr(ipaddr, i1,i2,i3,i4);
	uip_sethostaddr(ipaddr);

	i1=get_ip(g_conf_gw,1);
	i2=get_ip(g_conf_gw,2);
	i3=get_ip(g_conf_gw,3);
	i4=get_ip(g_conf_gw,4);
	uip_ipaddr(ipaddr, i1,i2,i3,i4);
	uip_setdraddr(ipaddr);

	uip_ipaddr(ipaddr, 255, 255, 255, 0);
	uip_setnetmask(ipaddr);
	net_get_mac(&(mac_addr.addr[0]));
	uip_setethaddr(mac_addr);

	uip_listen(HTONS(80));
	ut_log(" initilizing UIP stack \n");
	return &uip_stack;
}

}
