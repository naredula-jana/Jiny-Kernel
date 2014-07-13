
#include "../network.hh"

unsigned short in_cksum(unsigned short *addr, int len){
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *) (&answer) = *(unsigned char *) w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}

int network_stack::open(void *conn, int flags){
	return JSUCCESS;
}
int network_stack::close(void *conn){
	return JSUCCESS;
}
int network_stack::read(void *conn, uint8_t *raw_data, int raw_len, uint8_t *app_data, int app_maxlen){
	struct ether_pkt *pkt=(struct ether_pkt *)(raw_data+10);
	int len;
	int hdr_len;
	ut_printf("raw received length :%x \n",raw_len);

	if (app_data ==0) return 0;
	len=ntohs(pkt->iphdr.tot_len);
	hdr_len = sizeof(struct iphdr) + sizeof(struct udphdr);
	len = len - hdr_len;
	if (len < 0) return 0;
	if ((len>(raw_len-hdr_len)) || (len > app_maxlen)){
		return 0;
	}
	ut_memcpy(app_data,&pkt->data,len);
	ut_printf("received length :%x \n",len);

	return len;
}


int network_stack::write(void *conn, uint8_t *app_data, int app_len){
     struct ether_pkt *pkt=(struct ether_pkt *)temp_buff;
     int len;
     unsigned char brdcst[7]={0xff,0xff,0xff,0xff,0xff,0xff,0xff};
#if 0
    /* mac header */
     ut_memcpy(pkt->machdr.src,g_mac,6);
     ut_memcpy(pkt->machdr.dest, brdcst,6);
     pkt->machdr.type[0] = 0x08 ;

    /* ip header */
    pkt->iphdr.version=4;
    pkt->iphdr.ihl=5;
    pkt->iphdr.tos=0;
    pkt->iphdr.id=0;
    pkt->iphdr.frag_off=0;
    pkt->iphdr.ttl=255;
    pkt->iphdr.protocol=IPPROTO_UDP;
    pkt->iphdr.check=0;
    pkt->iphdr.saddr=htonl(conn->src_ip);
 //   pkt->iphdr.daddr=conn->dest_ip;
    pkt->iphdr.tot_len=htons(sizeof(struct iphdr)+sizeof(struct udphdr)+app_len);
    pkt->iphdr.check=in_cksum((unsigned short*) &(pkt->iphdr),sizeof(struct iphdr));

    /* udp header */

    if (conn->src_port==0){
    	conn->src_port = 100;
    }

    pkt->udphdr.source=conn->src_port;
    pkt->udphdr.dest=conn->dest_port;
    pkt->udphdr.checksum=0;
    pkt->udphdr.len=htons(sizeof(struct udphdr)+app_len);
    pkt->udphdr.checksum=in_cksum((unsigned short*) &(pkt->udphdr),8+app_len);

    ut_memcpy((unsigned char *)&pkt->data, app_data, app_len);

    len=sizeof(struct ether_pkt) -1 +app_len;
#endif
 //   return conn->net_dev->write(0,(unsigned char *)temp_buff,len);
    return 1; // TODO need to send the packet
}
network_stack net_stack;
extern "C" {
void *init_udpstack(){
	net_stack.name = "jiny_udp ip stack";
//	socket::net_stack_list[0] = &net_stack;
	ut_log(" initilizing Jiny udpip stack \n");
	return &net_stack;
}
}
