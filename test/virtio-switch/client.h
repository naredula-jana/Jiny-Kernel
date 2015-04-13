/*
 * client.h
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <limits.h>

#include "common.h"
#include "fd_list.h"

struct AppHandlers;

struct ClientStruct {
    char sock_path[PATH_MAX];
    int status;
    int sock;
    FdList fd_list;
    struct AppHandlers handlers;
};

typedef struct ClientStruct Client;

enum VhostUserRequest;

Client* new_client(const char* path);
int init_client(Client* client);
int end_client(Client* client);
int set_handler_client(Client* client, struct AppHandlers* handlers);
int loop_client(Client* client);

int vhost_ioctl(Client* client, enum VhostUserRequest request, ...);

#endif /* CLIENT_H_ */
