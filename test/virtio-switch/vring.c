/*
 * vring.c
 */

#ifndef VRING_C_
#define VRING_C_

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "vhost_server.h"
#include "vring.h"
#include "shm.h"
#include "vhost_user.h"
//#define  DUMP_PACKETS1 1
#define VRING_IDX_NONE          ((uint16_t)-1)

/* jana Changed below */
int send_pkt(VringTable* vring_table, uint32_t v_idx,  void* input_buf, size_t size);
unsigned long stat_send_succ=0;
uint32_t stat_send_err=0;

static int flush_bufs(VringTable* vring_table){
	int ret=0;
	unsigned long i;
	int k,tret;

	if ((vring_table->start_i == vring_table->end_i)){
		return 0;
	}
	for (i=vring_table->start_i; i<vring_table->end_i; i++){
		k = i % MAX_BUFS;
		tret =  send_pkt(vring_table, VHOST_CLIENT_VRING_IDX_RX, vring_table->data[k].buf, vring_table->data[k].len);
		if (tret < 0){
			break;
		}else{
			vring_table->start_i =i+1;
			ret++;
		}
	}
	if (vring_table->start_i == vring_table->end_i ){
		vring_table->end_i = 0;
		vring_table->start_i = 0;
	}
	return ret;
}
int insert_buf(VringTable* vring_table,  void* input_buf, size_t size){
	unsigned long i;

	if (vring_table->end_i >= (vring_table->start_i+MAX_BUFS)){
		vring_table->dropped++;
		return 0;
	}
	i = vring_table->end_i % MAX_BUFS;
	vring_table->data[i].len = size;
	memcpy(vring_table->data[i].buf, input_buf, size);
	vring_table->end_i++;
	return 1;
}
int send_cached_pkts(VhostServer* port,  void* input_buf, size_t size){
	int ret=0;
	VringTable* vring_table = &port->vring_table;

	ret = flush_bufs(vring_table);
	if (input_buf != 0) {
		if (vring_table->start_i == vring_table->end_i) {
			ret = send_pkt(&port->vring_table, VHOST_CLIENT_VRING_IDX_RX,input_buf, size);
			if (ret < 0) {
				ret = 0;
				ret = insert_buf(vring_table, input_buf, size);
			}
			return ret;
		} else {
			ret = ret + insert_buf(vring_table, input_buf, size);
		}
	}

	return ret;
}
int send_pkt(VringTable* vring_table, uint32_t v_idx,  void* input_buf, size_t size)
{
    struct vring_desc* desc = vring_table->vring[v_idx].desc;
    struct vring_avail* avail = vring_table->vring[v_idx].avail;
    struct vring_used* used = vring_table->vring[v_idx].used;
    unsigned int num = vring_table->vring[v_idx].num;
    ProcessHandler* handler = &vring_table->handler;
    uint16_t u_idx = vring_table->vring[v_idx].last_used_idx % num;
    uint32_t a_idx=vring_table->vring[v_idx].last_avail_idx % num;
    uint16_t d_idx = avail->ring[a_idx];
    uint32_t i, len = 0;
    size_t hdr_len = sizeof(struct virtio_net_hdr);
    int k;

    if (vring_table->vring[v_idx].last_used_idx == avail->idx){
    	if ((stat_send_err%2500000) ==0 ){
   // 	if (((vring_table->dropped % 200)==0) && vring_table->dropped!=0){
    		fprintf(stdout, "%p: ERROR : NO-SPACE... in Avail:%d ava  error:%d DROP:%d max_buf:%d\n",vring_table,avail->idx,stat_send_err,vring_table->dropped,MAX_BUFS);
    	}
    	stat_send_err++;
        return -1;
    }

    i=d_idx;
    for (k=0; k<2; k++) {
        void* cur = 0;
        uint32_t cur_len = desc[i].len;

        // map the address
        if (handler && handler->map_handler) {
            cur = (void*)handler->map_handler(handler->context, desc[i].addr);
        } else {
            cur = (void*) (uintptr_t) desc[i].addr;
        }

        if (k==1) {
        	memcpy(cur, input_buf, size);
        	cur_len =size;
        }else{
            memset(cur, 0, hdr_len);
            cur_len=hdr_len;
        }
        len = len + cur_len;

        if (desc[i].flags & VIRTIO_DESC_F_NEXT) {
            i = desc[i].next;
        } else {
            break;
        }
    }

    if (!len){
    	stat_send_err++;
        return -1;
    }
    stat_send_succ++;
    // add it to the used ring
    used->ring[u_idx].id = d_idx;
    used->ring[u_idx].len = len;
#ifdef DUMP_PACKETS1
    fprintf(stdout, "sending the len:%d  ",len);
#endif
    if  (used->flags == 0){
    	used->flags = 1;
    }
    sync_shm();
    vring_table->vring[v_idx].last_used_idx++;
    used->idx = vring_table->vring[v_idx].last_used_idx;
    vring_table->vring[v_idx].last_avail_idx++;
    sync_shm();

    return 0;
}
#if 1
static int free_vring(VringTable* vring_table, uint32_t v_idx, uint32_t d_idx)
{
    struct vring_desc* desc = vring_table->vring[v_idx].desc;
    uint16_t f_idx = vring_table->vring[v_idx].last_avail_idx;

    assert(d_idx>=0 && d_idx<VHOST_VRING_SIZE);

    // return the descriptor back to the free list
    desc[d_idx].len = BUFFER_SIZE;
    desc[d_idx].flags |= VIRTIO_DESC_F_WRITE;
    desc[d_idx].next = f_idx;
    vring_table->vring[v_idx].last_avail_idx = d_idx;

    return 0;
}
#endif
int process_used_vring(VringTable* vring_table, uint32_t v_idx)
{
    struct vring_used* used = vring_table->vring[v_idx].used;
    unsigned int num = vring_table->vring[v_idx].num;
    uint16_t u_idx = vring_table->vring[v_idx].last_used_idx;

    for (; u_idx != used->idx; u_idx = (u_idx + 1) % num) {
        free_vring(vring_table, v_idx, used->ring[u_idx].id);
    }

    vring_table->vring[v_idx].last_used_idx = u_idx;

    return 0;
}
unsigned long stat_recv_succ=0;
unsigned long stat_recv_err=0;
static int read_pkt(VhostServer* send_port,VringTable* vring_table, uint32_t v_idx, uint32_t a_idx)
{
    struct vring_desc* desc = vring_table->vring[v_idx].desc;
    struct vring_avail* avail = vring_table->vring[v_idx].avail;
    struct vring_used* used = vring_table->vring[v_idx].used;
    unsigned int num = vring_table->vring[v_idx].num;
    ProcessHandler* handler = &vring_table->handler;
    uint16_t u_idx = vring_table->vring[v_idx].last_used_idx % num;
    uint16_t d_idx = avail->ring[a_idx];
    uint32_t i, len = 0;
    size_t buf_size = ETH_PACKET_SIZE;
    uint8_t buf[buf_size];
    struct virtio_net_hdr *hdr = 0;
    size_t hdr_len = sizeof(struct virtio_net_hdr);

#ifdef DUMP_PACKETS1
    fprintf(stdout, "Receving chunks on %d usedIdx:%d(%d): user_idx:%d num:%d ",v_idx,vring_table->vring[v_idx].last_used_idx,used->idx,u_idx,num);
#endif

    i=d_idx;
    for (;;) {
        void* cur = 0;
        uint32_t cur_len = desc[i].len;

        // map the address
        if (handler && handler->map_handler) {
            cur = (void*)handler->map_handler(handler->context, desc[i].addr);
        } else {
            cur = (void*) (uintptr_t) desc[i].addr;
        }
#ifdef DUMP_PACKETS1
            fprintf(stdout, " desc len: %d ", cur_len);
#endif
        if (len + cur_len < buf_size) {
            memcpy(buf + len, cur, cur_len);
#ifdef DUMP_PACKETS1
            fprintf(stdout, "%d ", cur_len);
#endif
        } else {
            break;
        }

        len += cur_len;

        if (desc[i].flags & VIRTIO_DESC_F_NEXT) {
            i = desc[i].next;
        } else {
            break;
        }
    }

    if (!len){
#ifdef DUMP_PACKETS1
            fprintf(stdout, "ERROR: desclen:%d\n",desc[d_idx].len);
#endif
    	stat_recv_err++;
        return -1;
    }
    stat_recv_succ++;

    // add it to the used ring
    used->ring[u_idx].id = d_idx;
    used->ring[u_idx].len = len;
    if  (used->flags == 0){
    	used->flags = 1;
    }

#ifdef DUMP_PACKETS1
    fprintf(stdout, "len: %d \n",len);
#endif

    // check the header
    hdr = (struct virtio_net_hdr *)buf;

    if ((hdr->flags != 0) || (hdr->gso_type != 0) || (hdr->hdr_len != 0)
         || (hdr->gso_size != 0) || (hdr->csum_start != 0)
         || (hdr->csum_offset != 0)) {
        fprintf(stderr, "wrong flags\n");
    }

    // consume the packet
  //  send_pkt(&send_port->vring_table, VHOST_CLIENT_VRING_IDX_RX,
   // 		buf + hdr_len, len - hdr_len);
    send_cached_pkts(send_port, buf + hdr_len, len - hdr_len);

    return 0;
}

int process_input_fromport(VhostServer* vhost_server,VhostServer* send_vhost_server)
{
	int v_idx = VHOST_CLIENT_VRING_IDX_TX;
	VringTable* vring_table= &vhost_server->vring_table;
    struct vring_avail* avail = vring_table->vring[v_idx].avail;
    struct vring_used* used = vring_table->vring[v_idx].used;
    unsigned int num = vring_table->vring[v_idx].num;
    int loop;

    uint32_t count = 0;
    if (avail==0 || vhost_server->vring_table.vring[v_idx].avail==0){
    	return 0;
    }
    uint16_t a_idx = vring_table->vring[v_idx].last_avail_idx % num;

    // Loop all avail descriptors
    for (loop=0; loop<10; loop++) {
        /* we reached the end of avail */
        if (vring_table->vring[v_idx].last_avail_idx == avail->idx) {
            break;
        }

        read_pkt(send_vhost_server,vring_table, v_idx, a_idx);
        a_idx = (a_idx + 1) % num;
        vring_table->vring[v_idx].last_avail_idx++;
        vring_table->vring[v_idx].last_used_idx++;
        count++;

#ifdef DUMP_PACKETS1
            fprintf(stdout, "Recevied inside count :%d used->idx :%d \n",count,used->idx);
#endif

    }
    if (count ==0){
    	return 0;
    }
	sync_shm();  /* all memory buffers are seen before used-idx is seen */
    used->idx = vring_table->vring[v_idx].last_used_idx;
    sync_shm();  /* used->idx  need to seen by others */

#ifdef DUMP_PACKETS1
     fprintf(stdout, "vhost:%p Recevied count :%d used->idx :%d \n",(void *)vhost_server,count,used->idx);
#endif
    return count;
}
/************************************************   New functions ****************************/
struct struct_mbuf{
	void *input_buf;
	int len;

	void *raw_addr[3];
	int raw_index[3];
};
//int Bulk_send_pkt(VringTable* vring_table, uint32_t v_idx,  void* input_buf, size_t size){
int Bulk_send_pkt(VringTable* vring_table,  struct struct_mbuf *mbuf_list, int list_size){
    struct vring_desc* desc = vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].desc;
    struct vring_avail* avail = vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].avail;
    struct vring_used* used = vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].used;
    unsigned int num = vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].num;
    ProcessHandler* handler = &vring_table->handler;
    uint16_t u_idx = vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_used_idx % num;
    uint32_t a_idx=vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_avail_idx % num;
    uint16_t d_idx = avail->ring[a_idx];
    uint32_t i, len = 0;
    size_t hdr_len = sizeof(struct virtio_net_hdr);
    int k;
    uint16_t pkt;
    int ret=0;
    uint16_t avail_idx = avail->idx;

    //sync_shm();
	for (pkt = 0; pkt < list_size; pkt++) {
		if ((vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_used_idx +pkt) == avail_idx) {
			if ((stat_send_err % 2500000) == 0) {
				// 	if (((vring_table->dropped % 200)==0) && vring_table->dropped!=0){
				fprintf(stdout,
						"%p: ERROR : NO-SPACE.. in Avail:%d ava  error:%d DROP:%d max_buf:%d\n",
						vring_table, avail->idx, stat_send_err,
						vring_table->dropped, MAX_BUFS);
			}
			stat_send_err = stat_send_err + list_size - pkt ;
			goto last;
		}
		d_idx = avail->ring[(a_idx+pkt)% num];
		i = d_idx;
		for (k = 0; k < 2; k++) {
			void* cur = 0;
			uint32_t cur_len = desc[i].len;

			// map the address
			if (handler && handler->map_handler) {
				cur = (void*) handler->map_handler(handler->context,
						desc[i].addr);
			} else {
				cur = (void*) (uintptr_t) desc[i].addr;
			}
			if (desc[i].addr == 0  && k==0){
				stat_send_err++;
				goto last;
			}
			if (k == 1) {
				memcpy(cur, mbuf_list[pkt].input_buf+hdr_len, mbuf_list[pkt].len-hdr_len );
				cur_len = mbuf_list[pkt].len-hdr_len;
			} else {
				memset(cur, 0, hdr_len);
				cur_len = hdr_len;
			}
			len = len + cur_len;

			if (desc[i].flags & VIRTIO_DESC_F_NEXT) {
				i = desc[i].next;
			} else {
				break;
			}
		}

		if (!len) {
			stat_send_err++;
			goto last;
		}
		stat_send_succ++;
		// add it to the used ring
		used->ring[(u_idx+ret) % num].id = d_idx;
		used->ring[(u_idx+ret) % num].len = len;
		if (used->flags == 0) {
			used->flags = 1;
		}
		ret++;
	}
last:
    sync_shm();
    vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_used_idx = ret + vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_used_idx;
    used->idx = vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_used_idx;
    vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_avail_idx = ret + vring_table->vring[VHOST_CLIENT_VRING_IDX_RX].last_avail_idx;
    sync_shm();

    return ret;
}
#define MAX_PKT 64
extern int test_mode;
extern void sigTerm(int s);
static uint16_t test_Bulk_read_pkt(VhostServer* send_port,VringTable* vring_table,  uint32_t a_idx){
    struct vring_desc* desc = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].desc;
    struct vring_avail* avail = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].avail;
    struct vring_used* used = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].used;
    unsigned int num = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].num;
  //  ProcessHandler* handler = &vring_table->handler;
    uint16_t u_idx = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_used_idx % num;
    uint16_t d_idx = avail->ring[a_idx];
    uint32_t  len = 0;
    struct struct_mbuf mbuf_list[MAX_PKT];
 //   struct virtio_net_hdr *hdr = 0;
    uint16_t pkt;
    uint16_t ret=0;
    uint16_t i;
    //int k;
    //void* cur = 0;

	for (pkt = 0; pkt < 64; pkt++) {
		if ((vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx+pkt) == avail->idx) {
			goto last;
		}
		a_idx = (vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx + pkt) % num;
		d_idx = avail->ring[a_idx];
		i = d_idx;
		len =0;

		if (desc[i].addr == 0  ){
						stat_recv_err++;
						goto last;
					}
#if 0
		for (k = 0; k < 2; k++) {
			cur = 0;
			uint32_t cur_len = desc[i].len;

			// map the address
			if (handler && handler->map_handler) {
				cur = (void*) handler->map_handler(handler->context,
						desc[i].addr);
			} else {
				cur = (void*) (uintptr_t) desc[i].addr;
			}
			if (len ==0){
				mbuf_list[pkt].input_buf = cur;
				mbuf_list[pkt].len = 0;
			}
			if (desc[i].addr == 0  && k==0){
				stat_recv_err++;
				goto last;
			}

			mbuf_list[pkt].len += cur_len;
			len += cur_len;

			if (desc[i].flags & VIRTIO_DESC_F_NEXT) {
				i = desc[i].next;
			} else {
				break;
			}
		}

		if (!len) {
			fprintf(stderr, "ERROR:... WRONG Len\n");
			sigTerm(1);
			stat_recv_err++;
			goto last;
		}
		// check the header
		hdr = (struct virtio_net_hdr *) mbuf_list[pkt].input_buf;

		if ((hdr->flags != 0) || (hdr->gso_type != 0) || (hdr->hdr_len != 0)
						|| (hdr->gso_size != 0) || (hdr->csum_start != 0)
						|| (hdr->csum_offset != 0)) {
				fprintf(stderr, "ERROR:... WRONG flags  avail_index: %d   descr addr:%p cur:%p count:%d k:%d\n",i,(void *)desc[i].addr,cur,pkt,k);
				sigTerm(1);
				stat_recv_err++;
				fprintf(stderr, "..... going infinite loop \n");
				while(1);
				goto last;
		}
#endif
		stat_recv_succ++;
		// add it to the used ring
		used->ring[(u_idx+pkt) % num].id = d_idx;
		used->ring[(u_idx+pkt) % num].len = len;
		if (used->flags == 0) {
			used->flags = 1;
		}
		ret++;
	}
last:
     if (ret > 0  && test_mode==0){
    //if (ret > 0 ){
    	 Bulk_send_pkt(&send_port->vring_table,&mbuf_list[0], pkt);
     }

    return ret;
}static uint16_t Bulk_read_pkt(VhostServer* send_port,VringTable* vring_table,  uint32_t a_idx,uint64_t mmap_addr, uint64_t memory_size){
    struct vring_desc* desc = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].desc;
    struct vring_avail* avail = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].avail;
    struct vring_used* used = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].used;
    unsigned int num = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].num;
    //ProcessHandler* handler = &vring_table->handler;
    uint16_t u_idx = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_used_idx % num;
    uint16_t d_idx = avail->ring[a_idx];
    uint32_t  len = 0;
    struct struct_mbuf mbuf_list[MAX_PKT+1];
    struct virtio_net_hdr *hdr = 0;
    uint16_t pkt;
    uint16_t ret=0;
    uint16_t i;
    int k;
    void* cur = 0;
    uint16_t avail_idx = avail->idx;
   // VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

    //sync_shm();
	for (pkt = 0; pkt < MAX_PKT; pkt++) {
		if ((vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx+pkt) == avail_idx) {
			goto last;
		}
		a_idx = (vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx + pkt) % num;
		d_idx = avail->ring[a_idx];
		i = d_idx;
		len =0;
		for (k = 0; k < 2; k++) {
			cur = 0;
			uint32_t cur_len = desc[i].len;
			mbuf_list[pkt].raw_addr[k] = (void *)desc[i].addr;
			mbuf_list[pkt].raw_index[k] =i;
			// map the address
#if 0
			if (handler && handler->map_handler) {
				cur = (void*) handler->map_handler(handler->context,
						mbuf_list[pkt].raw_addr[k]);
			} else {
				cur = (void*) (uintptr_t) desc[i].addr;
			}
#else
			if ((uint64_t)mbuf_list[pkt].raw_addr[k] > memory_size){
				fprintf(stderr, "ERROR:**** address exceeds memory size addr:%p, size:%p \n",(void *)mbuf_list[pkt].raw_addr[k],(void *)memory_size);
				while(1);
			}
			cur = mmap_addr + mbuf_list[pkt].raw_addr[k] ;
#endif

			if (k == 0){
				mbuf_list[pkt].input_buf = cur;
				mbuf_list[pkt].len = 0;
			}
			if (mbuf_list[pkt].raw_addr[k] == 0  && k==0){
				stat_recv_err++;
				goto last;
			}

			mbuf_list[pkt].len += cur_len;
			len += cur_len;

			if (desc[i].flags & VIRTIO_DESC_F_NEXT) {
				i = desc[i].next;
			} else {
				break;
			}
		}

		if (!len) {
			fprintf(stderr, "ERROR:**** WRONG Len\n");
			sigTerm(1);
			stat_recv_err++;
			goto last;
		}
		// check the header
		hdr = (struct virtio_net_hdr *) mbuf_list[pkt].input_buf;

		if ((hdr->flags != 0) || (hdr->gso_type != 0) || (hdr->hdr_len != 0)
						|| (hdr->gso_size != 0) || (hdr->csum_start != 0)
						|| (hdr->csum_offset != 0)) {
			char *cc= (char *)hdr;
				fprintf(stderr, "ERROR:**** WRONG flags  DESC_index: %d   descr addr:%p cur:%p count:%d k:%d HDR:%p data:%x:%x:%x:%x len:%d\n",i,(void *)desc[i].addr,cur,pkt,k,hdr,(char )cc[0],(char )cc[1],(char )cc[2],(char )cc[3],len);
				sigTerm(1);
				stat_recv_err++;
				fprintf(stderr, "..... NEW going infinite loop \n");
				while(1);
				goto last;
		}
		stat_recv_succ++;

		// add it to the used ring
		used->ring[(u_idx+pkt) % num].id = d_idx;
		used->ring[(u_idx+pkt) % num].len = len;
		if (used->flags == 0) {
			used->flags = 1;
		}
		ret++;
	}
last:
     if (ret > 0  && test_mode==0){
    //if (ret > 0 ){
    	 Bulk_send_pkt(&send_port->vring_table,&mbuf_list[0], pkt);
     }

    return ret;
}
int Bulk_process_input_fromport(VhostServer* vhost_server,VhostServer* send_vhost_server){
	VringTable* vring_table= &vhost_server->vring_table;
    struct vring_avail* avail = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].avail;
    struct vring_used* used = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].used;
    unsigned int num = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].num;
    uint16_t ret;
    if (avail==0 || vhost_server->vring_table.vring[VHOST_CLIENT_VRING_IDX_TX].avail==0){
    	return 0;
    }
    uint16_t a_idx = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx % num;

        /* we reached the end of avail */
	if (vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx == avail->idx) {
		return 0;
	}
	//if (test_mode == 0) {
	if (1) {
		uint64_t mmap_addr = vhost_server->memory.regions[0].mmap_addr;
		uint64_t memory_size = vhost_server->memory.regions[0].memory_size;
		ret = Bulk_read_pkt(send_vhost_server, vring_table, a_idx,mmap_addr,memory_size);
	} else {
		ret = test_Bulk_read_pkt(send_vhost_server, vring_table, a_idx);
	}
	vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx = ret + vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_avail_idx;
	vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_used_idx  = ret + vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_used_idx;

    if (ret==0){
    	return 0;
    }
	sync_shm();  /* all memory buffers are seen before used-idx is seen */
    used->idx = vring_table->vring[VHOST_CLIENT_VRING_IDX_TX].last_used_idx;
    sync_shm();  /* used->idx  need to seen by others */

    return ret;
}


#endif /* VRING_C_ */
