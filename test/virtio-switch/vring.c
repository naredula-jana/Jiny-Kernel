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
    		fprintf(stdout, "%p: ERROR : NO-SPACE in Avail:%d ava  error:%d DROP:%d max_buf:%d\n",vring_table,avail->idx,stat_send_err,vring_table->dropped,MAX_BUFS);
    	}
    	stat_send_err++;
        return -1;
    }

#ifdef DUMP_PACKETS1
    fprintf(stdout, "SENDING chunks on %d - size:%d : ",v_idx,(int)size);
#endif

    i=d_idx;
    for (k=0; k<2; k++) {
        void* cur = 0;
        uint32_t cur_len = desc[i].len;

#ifdef DUMP_PACKETS1
        fprintf(stdout, "desc_len:%d addr:%p ", cur_len,(void *)desc[i].addr);
#endif

        // map the address
        if (handler && handler->map_handler) {
            cur = (void*)handler->map_handler(handler->context, desc[i].addr);
        } else {
            cur = (void*) (uintptr_t) desc[i].addr;
        }

        if (k==1) {
        	//if (cur_len > size){
        	if (1){
        		memcpy(cur, input_buf, size);
        		cur_len =size;
        	}{
        		//cur_len=0;
        	}
#ifdef DUMP_PACKETS1
            fprintf(stdout, "New %d(%d) ", (int)size,cur_len);
#endif
        }else{
            memset(cur, 0, hdr_len);
            cur_len=hdr_len;
#ifdef DUMP_PACKETS1
            fprintf(stdout, "%d(%d) ",(int)hdr_len,cur_len);
#endif
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
    if  (used->flags == 0){
    	used->flags = 1;
    }
    sync_shm();
    vring_table->vring[v_idx].last_used_idx++;
    used->idx = vring_table->vring[v_idx].last_used_idx;
    vring_table->vring[v_idx].last_avail_idx++;
    sync_shm();

    kick(vring_table,v_idx);
#ifdef DUMP_PACKETS1
    fprintf(stdout, "  len=%d num:%d\n",len,num);
#endif

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
    fprintf(stdout, "\n");
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

int kick(VringTable* vring_table, uint32_t v_idx){

//printf("kicking :%d \n",v_idx);
    return 0;
}

#endif /* VRING_C_ */
