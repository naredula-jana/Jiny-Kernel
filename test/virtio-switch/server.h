/*
 * server.h
 */

#ifndef SERVER_H_
#define SERVER_H_

#include <limits.h>

#include "common.h"
#include "fd_list.h"
#include "vhost_user.h"

struct ServerMsg {
    struct VhostUserMsg msg;
    size_t fd_num;
    int fds[VHOST_MEMORY_MAX_NREGIONS];
};

typedef struct ServerMsg ServerMsg;

struct ServerStruct {
    char path[PATH_MAX + 1];
    int status;
    int sock;
    FdList fd_list;
    AppHandlers handlers;
};

typedef struct ServerStruct Server;

Server* new_server(const char* path);
int set_handler_server(Server* server, AppHandlers* handlers);
int init_server(Server* server, int is_listen);
int loop_server(Server* server,Server* other_server, int sleep);
int end_server(Server* server);

#endif /* SERVER_H_ */
