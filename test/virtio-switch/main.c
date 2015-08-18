/*
 * server.c
 */

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "vhost_client.h"
#include "vhost_server.h"

static struct sigaction sigact;
int app_running = 0;

static void signal_handler(int);
static void init_signals(void);
static void cleanup(void);
int thr_mode=0;
int main(int argc, char* argv[])
{
//    int opt = 0;
    VhostServer *port1 = 0;
    VhostServer *port2 = 0;

    atexit(cleanup);
    init_signals();

    printf(" <port1-file>: %s <port2-file>: %s \n",argv[1],argv[2]);
    port1 = new_vhost_server(argv[1], 1 /*is_listen*/);
    port2 = new_vhost_server(argv[2], 1 /*is_listen*/);
    thr_mode = atoi(argv[3]);
    if (port1 && port2) {
        run_vhost_server(port1,port2);
        end_vhost_server(port1);
        end_vhost_server(port2);
        free(port1);
        free(port2);
    }

    return EXIT_SUCCESS;

}

static void signal_handler(int sig){
    switch(sig)
    {
    case SIGINT:
    case SIGKILL:
    case SIGTERM:
        app_running = 0;
        break;
    default:
        break;
    }
}
extern unsigned long stat_recv_succ;
extern unsigned long stat_recv_err;
extern unsigned long stat_send_succ;
extern uint32_t stat_send_err;
extern void sigTerm(int s);
static void init_signals(void){
    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, (struct sigaction *)NULL);

	signal(SIGTERM, sigTerm);
	signal(SIGINT, sigTerm);
}

static void cleanup(void){
    sigemptyset(&sigact.sa_mask);
    app_running = 0;
}
