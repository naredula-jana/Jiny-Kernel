/*
 * vhost_server.c
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fd_list.h"
#include "shm.h"
#include "vhost_server.h"
#include "vring.h"

typedef int (*MsgHandler)(VhostServer* vhost_server, ServerMsg* msg);

//static int avail_handler_server(void* context, void* buf, size_t size);
static uintptr_t map_handler(void* context, uint64_t addr);

extern int app_running;
static int in_msg_server(void* context, ServerMsg* msg);
int poll_server(void* context, void *other_context);
static AppHandlers port1_handlers =
{
        .context = 0,
        .in_handler = in_msg_server,
        .poll_handler = poll_server
};
static AppHandlers port2_handlers =
{
        .context = 0,
        .in_handler = in_msg_server,
        .poll_handler = poll_server
};
VhostServer* new_vhost_server(const char* path, int is_listen)
{
    VhostServer* vhost_server = (VhostServer*) calloc(1, sizeof(VhostServer));
    int idx;

    //TODO: handle errors here

    vhost_server->server = new_server(path);
    init_server(vhost_server->server, is_listen);

    vhost_server->memory.nregions = 0;

    // VringTable initalization
    vhost_server->vring_table.handler.context = (void*) vhost_server;
   // vhost_server->vring_table.handler.avail_handler = avail_handler_server;
    vhost_server->vring_table.handler.map_handler = map_handler;

    for (idx = 0; idx < VHOST_CLIENT_VRING_NUM; idx++) {
        vhost_server->vring_table.vring[idx].kickfd = -1;
        vhost_server->vring_table.vring[idx].callfd = -1;
        vhost_server->vring_table.vring[idx].desc = 0;
        vhost_server->vring_table.vring[idx].avail = 0;
        vhost_server->vring_table.vring[idx].used = 0;
        vhost_server->vring_table.vring[idx].num = 0;
        vhost_server->vring_table.vring[idx].last_avail_idx = 0;
        vhost_server->vring_table.vring[idx].last_used_idx = 0;
    }

    vhost_server->buffer_size = 0;
    vhost_server->is_polling = 0;
    init_stat(&vhost_server->stat);

    return vhost_server;
}

int end_vhost_server(VhostServer* vhost_server)
{
    int idx;

    // End server
    end_server(vhost_server->server);
    free(vhost_server->server);
    vhost_server->server = 0;

    for (idx = 0; idx < vhost_server->memory.nregions; idx++) {
        VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];
        end_shm(vhost_server->server->path,
                (void*) (uintptr_t) region->userspace_addr,
                region->memory_size, idx);
    }

    return 0;
}

static uintptr_t _map_guest_addr(VhostServer* vhost_server, uint64_t addr)
{
    uintptr_t result = 0;
    int idx;

    for (idx = 0; idx < vhost_server->memory.nregions; idx++) {
        VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

        if (region->guest_phys_addr <= addr
                && addr < (region->guest_phys_addr + region->memory_size)) {
            result = region->mmap_addr + addr - region->guest_phys_addr;
            break;
        }
    }

    return result;
}

static uintptr_t _map_user_addr(VhostServer* vhost_server, uint64_t addr)
{
    uintptr_t result = 0;
    int idx;

    for (idx = 0; idx < vhost_server->memory.nregions; idx++) {
        VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

        if (region->userspace_addr <= addr
                && addr < (region->userspace_addr + region->memory_size)) {
            result = region->mmap_addr + addr - region->userspace_addr;
            break;
        }
    }

    return result;
}

static int _get_features(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    msg->msg.u64 = 0; // no features
    msg->msg.size = MEMB_SIZE(VhostUserMsg,u64);

    return 1; // should reply back
}

static int _set_features(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_owner(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _reset_owner(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_mem_table(VhostServer* vhost_server, ServerMsg* msg)
{
    int idx;
    fprintf(stdout, "%s\n", __FUNCTION__);

    vhost_server->memory.nregions = 0;

    for (idx = 0; idx < msg->msg.memory.nregions; idx++) {
        if (msg->fds[idx] > 0) {
            VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

            region->guest_phys_addr = msg->msg.memory.regions[idx].guest_phys_addr;
            region->memory_size = msg->msg.memory.regions[idx].memory_size;
            region->userspace_addr = msg->msg.memory.regions[idx].userspace_addr;

            assert(idx < msg->fd_num);
            assert(msg->fds[idx] > 0);

            region->mmap_addr =
                    (uintptr_t) init_shm_from_fd(msg->fds[idx], region->memory_size);

            vhost_server->memory.nregions++;
        }
    }

    fprintf(stdout, "Got memory.nregions %d\n", vhost_server->memory.nregions);

    return 0;
}

static int _set_log_base(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_log_fd(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_vring_num(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.state.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    vhost_server->vring_table.vring[idx].num = msg->msg.state.num;

    return 0;
}

static int _set_vring_addr(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.addr.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    vhost_server->vring_table.vring[idx].desc =
            (struct vring_desc*) _map_user_addr(vhost_server,
                    msg->msg.addr.desc_user_addr);
    vhost_server->vring_table.vring[idx].avail =
            (struct vring_avail*) _map_user_addr(vhost_server,
                    msg->msg.addr.avail_user_addr);
    vhost_server->vring_table.vring[idx].used =
            (struct vring_used*) _map_user_addr(vhost_server,
                    msg->msg.addr.used_user_addr);

    vhost_server->vring_table.vring[idx].last_used_idx =
            vhost_server->vring_table.vring[idx].used->idx;

    return 0;
}

static int _set_vring_base(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.state.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    vhost_server->vring_table.vring[idx].last_avail_idx = msg->msg.state.num;

    return 0;
}

static int _get_vring_base(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.state.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);
    fprintf(stdout, "%d: num :%d \n",idx,vhost_server->vring_table.vring[idx].last_avail_idx);
    msg->msg.state.num = vhost_server->vring_table.vring[idx].last_avail_idx;
    msg->msg.size = MEMB_SIZE(VhostUserMsg,state);

    return 1; // should reply back
}

static uintptr_t map_handler(void* context, uint64_t addr)
{
    VhostServer* vhost_server = (VhostServer*) context;
    return _map_guest_addr(vhost_server, addr);
}

int process_input_fromport(VhostServer* vhost_server,VhostServer* other_vhost_server);
void print_rings(char *str,VhostServer* vhost_server);
static int _kick_server(FdNode* node)
{
	VhostServer* other_vhost_server;
    VhostServer* vhost_server = (VhostServer*) node->context;
    int kickfd = node->fd;
    ssize_t r;
    uint64_t kick_it = 0;

    r = read(kickfd, &kick_it, sizeof(kick_it));

    if (r < 0) {
        perror("recv kick");
    } else if (r == 0) {
        fprintf(stdout, "New Kick fd closed\n");
        del_fd_list(&vhost_server->server->fd_list, FD_READ, kickfd);
    } else {
#if 1
        fprintf(stdout, "Got kick %"PRId64"\n", kick_it);
#endif
        if (port1_handlers.context == vhost_server){
        	other_vhost_server = port2_handlers.context;
        }else{
        	other_vhost_server = port1_handlers.context;
        }

        process_input_fromport(vhost_server,other_vhost_server);
        print_rings("port1: ",port1_handlers.context);
        print_rings("port2: ",port2_handlers.context);
    }

    return 0;
}

static int _set_vring_kick(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.u64 & VHOST_USER_VRING_IDX_MASK;
    int validfd = (msg->msg.u64 & VHOST_USER_VRING_NOFD_MASK) == 0;

    assert(idx<VHOST_CLIENT_VRING_NUM);
    if (validfd) {
        assert(msg->fd_num == 1);

        vhost_server->vring_table.vring[idx].kickfd = msg->fds[0];

        fprintf(stdout, "Got kickfd 0x%x\n", vhost_server->vring_table.vring[idx].kickfd);

        if (idx == VHOST_CLIENT_VRING_IDX_TX) {
            add_fd_list(&vhost_server->server->fd_list, FD_READ,
                    vhost_server->vring_table.vring[idx].kickfd,
                    (void*) vhost_server, _kick_server);
            fprintf(stdout, "Listening for kicks on 0x%x\n", vhost_server->vring_table.vring[idx].kickfd);
        }
        vhost_server->is_polling = 0;
    } else {
        fprintf(stdout, "Got empty kickfd. Start polling.\n");
        vhost_server->is_polling = 1;
    }

    return 0;
}

static int _set_vring_call(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.u64 & VHOST_USER_VRING_IDX_MASK;
    int validfd = (msg->msg.u64 & VHOST_USER_VRING_NOFD_MASK) == 0;

    assert(idx<VHOST_CLIENT_VRING_NUM);
    if (validfd) {
        assert(msg->fd_num == 1);

        vhost_server->vring_table.vring[idx].callfd = msg->fds[0];

        fprintf(stdout, "Got callfd 0x%x\n", vhost_server->vring_table.vring[idx].callfd);
    }

    return 0;
}

static int _set_vring_err(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static MsgHandler msg_handlers[VHOST_USER_MAX] = {
        0,                  // VHOST_USER_NONE
        _get_features,      // VHOST_USER_GET_FEATURES
        _set_features,      // VHOST_USER_SET_FEATURES
        _set_owner,         // VHOST_USER_SET_OWNER
        _reset_owner,       // VHOST_USER_RESET_OWNER
        _set_mem_table,     // VHOST_USER_SET_MEM_TABLE
        _set_log_base,      // VHOST_USER_SET_LOG_BASE
        _set_log_fd,        // VHOST_USER_SET_LOG_FD
        _set_vring_num,     // VHOST_USER_SET_VRING_NUM
        _set_vring_addr,    // VHOST_USER_SET_VRING_ADDR
        _set_vring_base,    // VHOST_USER_SET_VRING_BASE
        _get_vring_base,    // VHOST_USER_GET_VRING_BASE
        _set_vring_kick,    // VHOST_USER_SET_VRING_KICK
        _set_vring_call,    // VHOST_USER_SET_VRING_CALL
        _set_vring_err,     // VHOST_USER_SET_VRING_ERR
        };

static int in_msg_server(void* context, ServerMsg* msg)
{
    VhostServer* vhost_server = (VhostServer*) context;
    int result = 0;

    fprintf(stdout, "Processing message: %s\n", cmd_from_vhostmsg(&msg->msg));

    assert(msg->msg.request > VHOST_USER_NONE && msg->msg.request < VHOST_USER_MAX);

    if (msg_handlers[msg->msg.request]) {
        result = msg_handlers[msg->msg.request](vhost_server, msg);
    }

    return result;
}
int send_cached_pkts(VhostServer* port,  void* input_buf, size_t size);
int poll_server(void* context, void *other_context){
	int ret=0;
    VhostServer* port = (VhostServer*) context;
    VhostServer* other_port = (VhostServer*) other_context;
 //   int tx_idx = VHOST_CLIENT_VRING_IDX_TX;
    int rx_idx = VHOST_CLIENT_VRING_IDX_RX;

    if (port->vring_table.vring[rx_idx].desc) {
        	ret =process_input_fromport(port,other_port);
    }
    ret = ret + send_cached_pkts(port,0,0);
    ret = ret + send_cached_pkts(other_port,0,0);
    return ret;
}

void print_rings(char *str,VhostServer* vhost_server){
	int i;

	for (i=0; i<2; i++){
		struct vring_avail *avail=vhost_server->vring_table.vring[i].avail;
		struct vring_used *used=vhost_server->vring_table.vring[i].used;
		if (avail!=0 && used!=0){
			printf("%s  %d: AAvail(%p) inx:%d flag:%x   Used(%p) idx:%d flag:%x \n",str,i,(void *)avail,avail->idx,avail->flags,(void *)used,used->idx,used->flags);
		}
	}
}
int run_vhost_server(VhostServer* port1,VhostServer* port2)
{
	int ret=0;
	int sleep;
    port1_handlers.context = port1;
    port2_handlers.context = port2;
    set_handler_server(port1->server, &port1_handlers);
    set_handler_server(port2->server, &port2_handlers);

    start_stat(&port1->stat);
    start_stat(&port2->stat);

    app_running = 1; // externally modified
port1->vring_table.start_i =0;
port2->vring_table.start_i =0;
port1->vring_table.end_i =0;
port2->vring_table.end_i =0;
    while (app_running) {
    //	print_rings("Before",vhost_server);
        ret = loop_server(port1->server,port2->server,sleep);
        ret = ret + loop_server(port2->server,port1->server,sleep);
        if (ret == 0){
        	//sleep =1;
        	sleep =0;
        }else{
        	sleep =0;
        }
    }
    stop_stat(&port1->stat);
    stop_stat(&port2->stat);

    return 0;
}
