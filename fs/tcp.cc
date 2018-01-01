/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/tcp.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
extern "C"{
#include "common.h"
#include "mm.h"
#include "interface.h"
extern int net_send_eth_frame(unsigned char *buf, int len, int write_flags);
extern uint32_t net_htonl(uint32_t n);
unsigned long g_stat_tcpInData=0;
unsigned long g_stat_tcpInDataDiscard=0;
unsigned long g_stat_tcpInCtlDiscard=0;
unsigned long g_stat_tcpOutData=0;
unsigned long g_stat_tcpOutRetrans=0;
unsigned long g_stat_tcpOutAck=0;
unsigned long g_stat_tcpNewConn=0;
unsigned long g_stat_tcpDupSYN=0;
unsigned long g_stat_tcpBufAllocs=0;
unsigned long g_stat_tcpBufFrees=0;
unsigned long g_conf_tcpDebug __attribute__ ((section ("confdata"))) =0;
unsigned long g_stat_tcpSendBlocked=0;

}
#include "file.hh"
#include "network.hh"
//#include "network_stack.hh"
#include "types.h"
struct tcpip_hdr {
	/* IPv4 header. */
	uint8_t vhl, tos, len[2], ipid[2], ipoffset[2], ttl, proto;
	uint16_t ipchksum;
	uint16_t srcipaddr[2], destipaddr[2];

	/* TCP header. */
	uint16_t srcport, destport;
	uint32_t seqno, ackno;
	uint8_t tcpoffset, flags, wnd[2];
	uint16_t tcpchksum;
	uint8_t urgp[2];
	uint8_t optdata[4];
};

/* Structures and definitions. */
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_CTL 0x3f
#define u16_t uint16_t
#define u8_t uint8_t
void static tcp_retransmit(network_connection *conn) ;
static u16_t chksum(u16_t sum, const u8_t *data, u16_t len) {
	u16_t t;
	const u8_t *dataptr;
	const u8_t *last_byte;

	dataptr = data;
	last_byte = data + len - 1;
	while (dataptr < last_byte) { /* At least two more bytes */
		t = (dataptr[0] << 8) + dataptr[1];
		sum += t;
		if (sum < t) {
			sum++; /* carry */
		}
		dataptr += 2;
	}
	if (dataptr == last_byte) {
		t = (dataptr[0] << 8) + 0;
		sum += t;
		if (sum < t) {
			sum++; /* carry */
		}
	}

	/* Return sum in host byte order. */
	return sum;
}
#define IPH_LEN    20
#define LLH_LEN    14
#define PROTO_TCP   6
static u16_t upper_layer_chksum(u8_t *data, u8_t proto) {
	struct tcpip_hdr *BUF = (struct tcpip_hdr *) (data + LLH_LEN);
	u16_t upper_layer_len;
	u16_t sum;

	upper_layer_len = (((u16_t) (BUF->len[0]) << 8) + BUF->len[1]) - IPH_LEN;
	/* First sum pseudoheader. */
	/* IP protocol and length fields. This addition cannot carry. */
	sum = upper_layer_len + proto;
	/* Sum IP source and destination addresses. */
	sum = chksum(sum, (u8_t *) &BUF->srcipaddr[0], 2 * 4);

	/* Sum TCP header and data. */
	sum = chksum(sum, &data[IPH_LEN + LLH_LEN], upper_layer_len);
	return (sum == 0) ? 0xffff : htons(sum);
}

static u16_t ipchksum(u8_t *data) {
	u16_t sum;
	sum = chksum(0, &data[LLH_LEN], IPH_LEN);
	return (sum == 0) ? 0xffff : htons(sum);
}
int tcp_connection::send_tcp_pkt(uint8_t flags, unsigned char *data, int data_len,uint32_t seq_no) {
	struct ether_pkt *send_pkt;
	int ret, send_len;
	struct tcpip_hdr *send_tcp_hdr;
	int ip_len;

	unsigned char *buf = (unsigned long) jalloc_page(MEM_NETBUF);
	if (buf == 0) {
		return 0;
	}
	//g_stat_tcpBufAllocs++;
	ut_memset(buf, 0, 10);
	send_pkt = (struct ether_pkt *) (buf + 10);

	send_tcp_hdr = (struct tcpip_hdr *) &(send_pkt->iphdr);
	ut_memcpy(send_pkt->machdr.src, mac_src, 6);
	ut_memcpy(send_pkt->machdr.dest, mac_dest, 6);
	send_pkt->machdr.type[0] = 0x8;
	send_pkt->machdr.type[1] = 0;

	send_pkt->iphdr.daddr = ip_daddr;
	send_pkt->iphdr.saddr = ip_saddr;
	send_pkt->iphdr.frag_off = 0x40;

	send_tcp_hdr->destport = destport;
	send_tcp_hdr->srcport = srcport;
	send_tcp_hdr->vhl = 0x45;
	send_tcp_hdr->tos = 0x10;
	send_tcp_hdr->ttl = 0x40;
	send_tcp_hdr->flags = flags;
	send_tcp_hdr->proto = 0x6;
	send_tcp_hdr->urgp[0] = 0x0;
	send_tcp_hdr->urgp[1] = 0x0;
	send_tcp_hdr->wnd[0] = 0x72;
	send_tcp_hdr->wnd[1] = 0x10;
	ip_len = 40 + data_len;
	send_tcp_hdr->len[0] = (ip_len >>8) & 0xff; /* msb */
	send_tcp_hdr->len[1] = (ip_len & 0xff) ; /* lsb: ip + tcp len */

	send_len = 14 + 20 + 20 + data_len; /* ethernet+ip+tcp*/
	send_tcp_hdr->tcpoffset = 0x50; /* length of tcp header, number of 4 bytes(0x50= 5*4=20), no option is present */

	send_pkt->iphdr.check = 0;
	if (send_tcp_hdr->flags & TCP_SYN) {
		send_tcp_hdr->seqno = net_htonl(seq_no - 1);
	} else {
		if (send_tcp_hdr->flags & TCP_FIN) {
			send_tcp_hdr->seqno = net_htonl(seq_no);
		}else{
			send_tcp_hdr->seqno = net_htonl(seq_no);
		}
	}
	send_tcp_hdr->ackno = net_htonl(recv_seq_no);

	if (data_len > 0) {
		send_tcp_hdr->flags = send_tcp_hdr->flags | TCP_PSH;
		ut_memcpy(buf + 10 + 14 + 40, data, data_len);
		send_tcp_hdr->seqno = net_htonl(seq_no);
	}else{
		g_stat_tcpOutAck++;
	}
	send_tcp_hdr->tcpchksum = 0;
	send_tcp_hdr->tcpchksum = ~(upper_layer_chksum((unsigned char *) send_pkt,
			PROTO_TCP));
	send_pkt->iphdr.check = ~(ipchksum((unsigned char *) send_pkt));

	ret = net_send_eth_frame((unsigned char *) send_pkt, send_len, 0);
	if (ret != JFAIL) {

	} else {
		unsigned char *buf = (unsigned char *) send_pkt;
		jfree_page(buf - 10);
		//g_stat_tcpBufFrees++;
	}
	return ret;
}


/**********************   TCP api functions ***********************************************/
int tcp_conn_new(network_connection *conn, struct ether_pkt *recv_pkt) {
	int i;
	struct tcpip_hdr *recv_tcp_hdr = (struct tcpip_hdr *) &(recv_pkt->iphdr);
	int empty_i = -1;
	for (i=0; i< MAX_TCP_LISTEN; i++){
		if (conn->new_tcp_conn[i] != 0 && (conn->new_tcp_conn[i]->destport == recv_tcp_hdr->srcport)){
			g_stat_tcpDupSYN++;
			return 0;
		}
		if (conn->new_tcp_conn[i] == 0 && empty_i==-1){
			empty_i =i;
		}
	}
	if (empty_i == -1){
		return 0;
	}

	tcp_connection *tcp_conn = jnew_obj(tcp_connection);
	tcp_conn->magic_no = 0xabcd123;
	ut_memcpy(tcp_conn->mac_src, recv_pkt->machdr.dest, 6);
	ut_memcpy(tcp_conn->mac_dest, recv_pkt->machdr.src, 6);
	tcp_conn->ip_daddr = recv_pkt->iphdr.saddr;
	tcp_conn->ip_saddr = recv_pkt->iphdr.daddr;
	tcp_conn->destport = recv_tcp_hdr->srcport;
	tcp_conn->srcport = recv_tcp_hdr->destport;
	tcp_conn->send_seq_no = net_htonl(0x200);
	tcp_conn->recv_seq_no = net_htonl(recv_tcp_hdr->seqno) + 1;

	for (i = 0; i < MAX_TCPSND_WINDOW; i++) {
		tcp_conn->send_queue[i].buf = jalloc_page(MEM_NETBUF);
		tcp_conn->send_queue[i].len = 0;
		tcp_conn->send_queue[i].seq_no = 0;
		g_stat_tcpBufAllocs++;
	}
	tcp_conn->send_tcp_pkt(TCP_SYN | TCP_ACK, 0, 0,tcp_conn->send_seq_no);
	tcp_conn->state = TCP_CONN_ESTABILISHED;

	conn->new_tcp_conn[empty_i] = tcp_conn ;
	if (g_stat_tcpNewConn ==0 )
		g_stat_tcpNewConn++;
	tcp_conn->conn_no = g_stat_tcpNewConn; /* make sure this is not zero */
	g_stat_tcpNewConn++;
	return 1;
}
int tcp_conn_free(tcp_connection *tcp_conn) {
	int i;
	for (i = 0; i < MAX_TCPSND_WINDOW; i++) {
		if (tcp_conn->send_queue[i].buf != 0) {
			jfree_page(tcp_conn->send_queue[i].buf);
			tcp_conn->send_queue[i].buf =0;
			g_stat_tcpBufFrees++;
		}
	}
	ut_free(tcp_conn);
}
void tcp_connection::print_stats(unsigned char *arg1,unsigned char *arg2){
	int i;

	ut_printf("   TCP  ack_update sendseqno:%d  ackno: %d  state:%d \n",send_seq_no, send_ack_no ,state);
	for (i = 0; i < MAX_TCPSND_WINDOW; i++) {
		if (send_queue[i].len == 0){
			continue;
		}
		ut_printf("          seq: %d len:%d \n",send_queue[i].seq_no,send_queue[i].len);
	}
	return;
}

/* TODO: merge all segments and send in bigger buffer */
void tcp_connection::housekeeper() {
	int i;

	if ((retransmit_inuse == 1) || (squeue_size.counter==0) || (retransmit_ts==g_jiffies)){
		return;
	}
	retransmit_inuse = 1;

	for (i = 0; i < MAX_TCPSND_WINDOW; i++) {
		uint8_t flags = TCP_ACK;
		if (send_queue[i].len == 0){
			continue;
		}
		if (send_ack_no > send_queue[i].seq_no) {
			send_queue[i].len = 0;
			atomic_dec(&squeue_size);
			continue;
		}
		if (g_jiffies == send_queue[i].lastsend_ts){
			continue;
		}
		if (send_tcp_pkt( flags,
				send_queue[i].buf,
				send_queue[i].len,
				send_queue[i].seq_no) == JSUCCESS) {
			if (send_queue[i].lastsend_ts != 0){ /* this is not the first time */
				g_stat_tcpOutRetrans++;
			}else{
				g_stat_tcpOutData++;
			}
			send_queue[i].lastsend_ts = g_jiffies ;
		}
	}
	retransmit_ts = g_jiffies;
	retransmit_inuse = 0;
	return;
}

/* TODO: send entire user data in multiple iterations */
int tcp_connection::tcp_write( uint8_t *app_data, int app_maxlen) {
	int i;
	int copied = 0;

	if (app_maxlen > (PAGE_SIZE - 100)) {
		app_maxlen = PAGE_SIZE - 100;
	}

	for (i = 0; i < MAX_TCPSND_WINDOW; i++) {
		if ((send_queue[i].len != 0) && (send_ack_no > send_queue[i].seq_no)) {
			send_queue[i].len = 0;
			continue;
		}
		if (send_queue[i].len == 0) {
			ut_memcpy(send_queue[i].buf, app_data, app_maxlen);
			send_queue[i].seq_no = send_seq_no;
			send_tcp_pkt(TCP_ACK, send_queue[i].buf, app_maxlen, send_queue[i].seq_no);
			send_queue[i].lastsend_ts = g_jiffies;
			atomic_inc(&squeue_size);
			send_queue[i].len = app_maxlen;

			send_seq_no = send_seq_no + app_maxlen;
			copied = 1;
			break;
		}
	}
	if (g_conf_tcpDebug == 1){
		ut_printf("tcp  send: i=%d len:%d copied:%d seq_no:%d  sendack:%d  \n",i,app_maxlen,copied,send_seq_no,send_ack_no);
	}
	if (copied == 0) {
		g_stat_tcpSendBlocked++;
		return 0;
	}

	return app_maxlen;
}
/* this happens in the context of ISR or polling mode or this should in syncronous mode, this should be as fast as possible */
int tcp_read_listen_pkts(network_connection *conn, uint8_t *recv_data, int recv_len)   {
	struct ether_pkt *recv_pkt = (struct ether_pkt *) (recv_data + 10);
	struct tcpip_hdr *recv_tcp_hdr, *send_tcp_hdr;
	int ret;
	uint8_t flags = 0;
	int tcp_data_len = 0;

	recv_tcp_hdr = (struct tcpip_hdr *) &(recv_pkt->iphdr);
	int tcp_header_len = (recv_tcp_hdr->tcpoffset >> 4) * 4;

	if ((recv_tcp_hdr->flags & TCP_SYN)) { /* create new connection */
		return tcp_conn_new(conn, recv_pkt);
	}
	g_stat_tcpInCtlDiscard++;
	return 0;
}

/* this happens in the context of ISR or polling mode or this should in syncronous mode, this should be as fast as possible */
int tcp_connection::tcp_read(uint8_t *recv_data, int recv_len)   {
	struct ether_pkt *recv_pkt = (struct ether_pkt *) (recv_data + 10);
	struct tcpip_hdr *recv_tcp_hdr, *send_tcp_hdr;
	int ret;
	uint8_t flags = 0;
	int tcp_data_len = 0;

	recv_tcp_hdr = (struct tcpip_hdr *) &(recv_pkt->iphdr);
	int tcp_header_len = (recv_tcp_hdr->tcpoffset >> 4) * 4;

	tcp_data_len = htons(recv_pkt->iphdr.tot_len) - (20 + tcp_header_len); /* TODO: ip header assumed 20 */
	if (recv_tcp_hdr->flags & TCP_ACK) {
		int ackno = net_htonl(recv_tcp_hdr->ackno);
		send_ack_no = ackno ; /* TODO: need to check rounding the number  and large ack */
		if (tcp_data_len == 0){
			flags = TCP_ACK;
		}
	}
	if ((recv_tcp_hdr->flags & TCP_RST) || (recv_tcp_hdr->flags & TCP_FIN)) {
		if (g_conf_tcpDebug == 1){
				ut_log("tcp  FIN:  seq_no:%d  sendack:%d  \n",send_seq_no,send_ack_no);
		}
		flags = flags | TCP_ACK;
		if (recv_tcp_hdr->flags & TCP_RST) {
			send_tcp_pkt( flags | TCP_RST, 0, 0, send_seq_no);
		}else{
			send_tcp_pkt( flags | TCP_FIN, 0, 0, send_seq_no);
		}
		if (state == TCP_CONN_ESTABILISHED){
			state = TCP_CONN_CLOSED_RECV;
		}
		return 0;
	}
	if (tcp_data_len > 0) {
		if (recv_seq_no == net_htonl(recv_tcp_hdr->seqno)) {
			recv_seq_no = net_htonl(recv_tcp_hdr->seqno) + tcp_data_len;

			struct tcp_data *app_data=(struct tcp_data *) recv_data;
			app_data->len = tcp_data_len;
			app_data->consumed = 0;
			app_data->offset = 10 + 14 + 20 + tcp_header_len - TCP_USER_DATA_HDR ;  /* TODO: ip header assumed 20 */
			if ((app_data->len+ app_data->offset) > 4096){
				BUG();
			}
			g_stat_tcpInData++;
			//flags = flags | TCP_ACK;
		} else {
			tcp_data_len = 0;
			g_stat_tcpInDataDiscard++;
		}
	}
	if (flags !=0){
		send_tcp_pkt(flags, 0, 0, send_seq_no);
	}

	return tcp_data_len;
}


