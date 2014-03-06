
#include "network.hh"

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


class network_stack udpip_stack;
int network_stack::read(network_conn *conn, uint8_t *raw_data, unsigned long raw_len, uint8_t *app_data, unsigned long app_maxlen){
	return 1;
}

int network_stack::write(network_conn *conn,uint8_t *app_data, unsigned long app_len){
    struct pkt *pkt=(struct pkt *)temp_buff;
    struct iphdr *ip=&(pkt->iphdr);

    /* mac header */

    /* ip header */
    ip->version=4;
    ip->ihl=5;
    ip->tos=0;
    ip->id=0;
    ip->frag_off=0;
    ip->ttl=255;
    ip->protocol=IPPROTO_UDP;
    ip->check=0;
    ip->saddr=source_ip;
    ip->daddr=dest_ip;
 //   ip->saddr=inet_addr("1.2.3.4");
 //   ip->daddr=inet_addr("127.0.0.1");


     ip->tot_len=ut_htons(sizeof(struct iphdr)+sizeof(struct udphdr)+len);

    /* udp header */
    struct udphdr *udp=&(pkt->udphdr);
    udp->source=ut_htons(40000);
    udp->dest=ut_htons(dport);
    udp->checksum=0;
//    char* data=(char*)buf+sizeof(struct iphdr)+sizeof(struct udphdr);ut_strcpy(data,"Harry Potter and the Philosopher's Stone");
  //  udp->len=htons(sizeof(struct udphdr)+strlen(data));
 //   udp->checksum=in_cksum((unsigned short*) udp,8+strlen(data));


    ut_memcpy((unsigned char *)&pkt->data, app_data, app_len);
    len=sizeof(struct pkt) -1 +app_len;
    network_device->write(0,(unsigned char *)temp_buff,len);

    return 1;
}
network_stack net_stack;
void init(){
	socket::net_stack = net_stack;
}
