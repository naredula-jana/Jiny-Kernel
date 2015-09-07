

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
#define MEM_NETBUF     0x0200000  /* TODO : need to remove later */
int g_conf_zerocopy=0;  /* TODO: temporary variable : need to remove later */
}

#include "network_stack.hh"
#define JSUCCESS 1  /* TODO: need removed , redefined */
#define JFAIL 0  /* TODO: need removed , redefined */
#define DEBUG

int network_stack::open(network_connection *conn, int flags) {
	conn->proto_connection = 0;
	if (conn->protocol == IPPROTO_UDP) {
		struct uip_udp_conn_struct *uip_conn;
		uip_conn = uip_udp_new(0, 0);
		if (uip_conn ==0 ){
			return JFAIL;
		}
		conn->proto_connection = uip_conn;
		conn->src_port = uip_conn->lport;
	} else if (conn->protocol == IPPROTO_TCP) {
		conn->src_port = 0;
	}

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
#define EXTRA_INITIAL_BYTES 10
/*
 *  raw_data  : raw packet recvied
 *  app_data  : app data
 */
int network_stack::read(network_connection *conn, uint8_t *raw_data, int raw_len, uint8_t *app_data, int app_maxlen) {
	int ret = JFAIL;
	int pkt_len;
	unsigned char *jbuf=0;

	netstack_lock();
	DEBUG("new UIP raw received length :%d  conn:%x   ..... :uip_conn:%x \n", raw_len,conn,uip_conn);
	uip_len = raw_len - EXTRA_INITIAL_BYTES;
	pkt_len = uip_len;
	if (uip_buf!=0){
		while(1);
	}

	uip_appdata = 0;
	jiny_uip_data = 0;
	jiny_uip_callback_flags = 0;
	if (conn != 0 && conn->protocol == IPPROTO_UDP) {
		uip_buf = raw_data+EXTRA_INITIAL_BYTES;
		uip_input();
		if (uip_appdata) {
			ret = pkt_len - (UIP_LLH_LEN + UIP_IPUDPH_LEN);
			if (g_conf_zerocopy == 0){
				ut_memcpy(app_data, uip_appdata, ret);
			}
			conn->dest_port = UDPBUF->srcport;
			ut_memcpy((unsigned char *)&(conn->src_ip), (unsigned char *)&(UDPBUF->destipaddr[0]),4);
			ut_memcpy((unsigned char *)&(conn->dest_ip), (unsigned char *)&(UDPBUF->srcipaddr[0]),4);
			if (jbuf != 0){
				jfree_page(jbuf);
				jbuf =0;
			}
		} else {
			ret = JFAIL;
		}
		uip_len = 0;
		goto last;
	}

	jbuf = (unsigned char *) jalloc_page(MEM_NETBUF);
	if (jbuf ==0) {
		goto last;
	}else{
		uip_buf = jbuf+EXTRA_INITIAL_BYTES;
		ut_memset(jbuf,0,EXTRA_INITIAL_BYTES);
	}
	ut_memcpy(jbuf, raw_data , uip_len+EXTRA_INITIAL_BYTES);

	if (conn != 0 && conn->protocol == IPPROTO_TCP){
		ret = JFAIL;
		uip_input();
		if (jiny_uip_data != 0 ) {
			ret = jiny_uip_dlen;
			if (app_data != 0){
				ut_memcpy(app_data, jiny_uip_data, ret);
			}
		}
		if (jiny_uip_callback_flags == UIP_CLOSE){
			ret = -1;
		}
		if (jiny_uip_callback_flags == UIP_CONNECTED){
			network_connection *new_conn = conn->child_connection;
			ut_printf(" New connection details : %x \n",new_conn);
			if (new_conn != 0){
				new_conn->dest_port = UDPBUF->srcport;
				ut_memcpy((unsigned char *)&(new_conn->dest_ip), (unsigned char *)&(UDPBUF->srcipaddr[0]),4);
				ut_printf(" New connection port : ip : %x:%x \n",new_conn->dest_port,new_conn->dest_ip);
			}
			ret = -2;
		}
		if (uip_len > 0) {
			uip_arp_out();
			ret = net_send_eth_frame(uip_buf, uip_len, 0);
			if (ret != JFAIL) {
				jbuf = 0;
			}
			uip_len = 0;
			goto last;
		}
		jfree_page(jbuf);
		jbuf =0 ;
		goto last;

	}else if (conn == 0 ){

	}

	/* packete not related to any connection like arp etc */
	if (BUF->type == HTONS(UIP_ETHTYPE_IP)) {
		uip_arp_ipin();
		uip_input();
		if (uip_len > 0) {
			uip_arp_out();
			DEBUG("FROM READ .... buf :%x  replied with the packet len : %d \n",uip_buf,uip_len);
			ret = net_send_eth_frame(uip_buf, uip_len,0);
			uip_len = 0;
			goto last;
		}
	} else if (BUF->type == HTONS(UIP_ETHTYPE_ARP)) {
		uip_arp_arpin();
		if (uip_len > 0) {
			DEBUG(" replied with the ARP   packet len : %d \n",uip_len);
			ret = net_send_eth_frame(uip_buf, uip_len,0);
			uip_len = 0;
			goto last;
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
extern "C"{
int g_conf_test_dummy_send=0;
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

	buf = (unsigned long) jalloc_page(MEM_NETBUF);
	if (buf ==0) {
		goto last;
	}else{
		uip_buf = buf+10;
		ut_memset(buf,0,10);
	}
	if (conn->protocol == IPPROTO_UDP) {
		unsigned char *send_buf;
		unsigned int send_len;

		uip_udp_conn = conn->proto_connection;
		ip = conn->dest_ip;
		port = conn->dest_port;
		uip_udp_conn->rport = port;
		if (uip_udp_conn->lport != conn->src_port ){ /* bind as over written the protcol port , so need to take socket port */
			uip_udp_conn->lport = conn->src_port;
		}

		uip_udp_conn->ripaddr[0] = ip & 0xffff;
		uip_udp_conn->ripaddr[1] = (ip >> 16) & 0xffff;

		uip_slen = app_len;
		uip_process(UIP_UDP_SEND_CONN);
		if (g_conf_zerocopy == 0){
			ut_memcpy(uip_appdata, app_data, app_len);
		}

		send_buf = uip_buf;
		send_len = 0;
		if (uip_len > 0) {
			uip_arp_out();
			DEBUG("FROM WRITE ... - uip SENDTO pkt: buf: %x  %d applen :%d \n", uip_buf, uip_len, app_len);
			send_len = uip_len;
			//ret = net_send_eth_frame(uip_buf, uip_len,WRITE_BUF_CREATED);
		}
		uip_len = 0;
		uip_slen = 0;
		uip_conn = 0;
		uip_udp_conn = 0;
		if (send_len > 0){
			uip_buf=0;
			netstack_unlock();
#if 1
			if (g_conf_test_dummy_send > 0) {
				int i;
				for (i=0; i<g_conf_test_dummy_send; i++) {
					unsigned char *test_dbuf = (unsigned long) jalloc_page(MEM_NETBUF);
					if (test_dbuf != 0) {
						ut_memset(test_dbuf,0,10);
						test_dbuf = test_dbuf +10;
						ut_memcpy(test_dbuf, send_buf, send_len+50);
						ret = net_send_eth_frame(test_dbuf, send_len, WRITE_SLEEP_TILL_SEND);
						if (ret == JFAIL) {
							jfree_page(test_dbuf);
						}
					}
				}
			}
#endif
			ret = net_send_eth_frame(send_buf, send_len, WRITE_SLEEP_TILL_SEND);
			if ((ret == JFAIL) && send_buf){
					jfree_page(send_buf);
					buf =0 ;
			}
			return ret;
		}
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
	unsigned char *buf=0;
	netstack_lock();
	if (conn->protocol != IPPROTO_TCP) {
		ret = JFAIL;
		goto last;
	}
	buf = (unsigned long) jalloc_page(0);
	if (buf ==0) {
		goto last;
	}else{
		uip_buf = buf+10;
		ut_memset(buf,0,10);
	}

	test[0] = conn->dest_ip & 0xffff;
	test[1] = (conn->dest_ip >> 16) & 0xffff;

	uip_conn = uip_connect(&test, conn->dest_port);
	if (uip_conn != 0){
		conn->proto_connection = uip_conn;
		conn->src_port = uip_conn->lport;
	}
	uip_process(UIP_TIMER);
	uip_process(UIP_TIMER);
	if (uip_len > 0) {
		uip_arp_out();
		ut_printf(" tcp connect  message : %d \n",uip_len);
		ret = net_send_eth_frame(uip_buf, uip_len,0);
		uip_len = 0;
	}
last:
	uip_buf = 0;
	if (ret == JFAIL && buf) {
		jfree_page (buf);
	}
	netstack_unlock();
	return ret;
}
network_stack uip_stack;

extern "C" {
void net_get_mac(unsigned char *mac);
void uip_mod_udpappcall(void) { /* NOT used */

}
void uip_mod_appcall(void) {
	if (uip_connected()) {
		ut_printf("CALLBACK socket connected :%d \n", uip_len);

	}
	jiny_uip_callback_flags = uip_flags;
	ut_printf("CALLBACK flag :%x\n",uip_flags);
	if (uip_len > 0) {
		jiny_uip_data = uip_appdata;
		jiny_uip_dlen = uip_len;
		ut_printf(" CALLBACK Data len :%d :%s:\n", uip_len, uip_appdata);
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
void Jcmd_ifconfig(unsigned char *arg1,unsigned char *arg2){
	int i1,i2,i3,i4;
	uip_ipaddr_t ipaddr;
	struct uip_eth_addr mac_addr;
	if (arg1 ==0 || arg2==0){
		ut_printf(" ipaddr :%s gw_addr: %s \n",g_conf_ipaddr,g_conf_gw);
		ut_printf(" ifconfig <ip_address> <gw_ipaddress>\n");
		return;
	}

	if (g_conf_ipaddr != arg1){
		ut_strcpy(g_conf_ipaddr,arg1);
	}

	if (g_conf_gw != arg2){
		ut_strcpy(g_conf_gw,arg2);
	}

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
}
void *init_netmod_uipstack() {
	uip_stack.name = "UIP stack";
	ut_log(" ipaddr :%s:  gw:%s: \n",g_conf_ipaddr,g_conf_gw);

	uip_init();
	Jcmd_ifconfig(g_conf_ipaddr,g_conf_gw);

	uip_listen(HTONS(80));
	ut_log(" initilizing UIP stack \n");
	return &uip_stack;
}

}
