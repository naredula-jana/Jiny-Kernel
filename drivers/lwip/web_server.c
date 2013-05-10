
#define DEBUG_ENABLE 1
#include <lwip/api.h>

#include "common.h"

extern unsigned char g_dmesg[MAX_DMESG_LOG];
extern unsigned long g_dmesg_index;


static unsigned char message[300];

static void webserver_thread(void *p)
{
    struct ip_addr listenaddr = { htonl(0x0ad180b0) };
    struct netconn *listener;
    struct netconn *session;
    struct timeval tv;
    err_t rc;

    DEBUG("Opening connection fir webserver \n");

    listener = netconn_new(NETCONN_TCP);
    DEBUG("Connection at %x\n", &listener);

    rc = netconn_bind(listener, &listenaddr, 80);
    if (rc != ERR_OK) {
    	DEBUG("Failed to bind connection: %i\n", rc);
        return;
    }

    rc = netconn_listen(listener);
    if (rc != ERR_OK) {
        DEBUG("Failed to listen on connection: %i\n", rc);
        return;
    }
    DEBUG("sucessfully listening the webserver \n");

    while (1) {
    	int i;
        DEBUG("BEFORE accept the new connection \n");
        netconn_accept(listener,&session);
        if (session == NULL)
            continue;

        ut_sprintf(message, "<html><body><pre> Jiny Kernel Dmesg max_len:%d  curr_len:%d \n",MAX_DMESG_LOG,g_dmesg_index);
        (void) netconn_write(session, message, ut_strlen(message), NETCONN_COPY);
#if 0
        i=0;
        while (i<g_dmesg_index)
        {
        	if (g_dmesg_index < MAX_DMESG_LOG)
        		(void) netconn_write(session, &g_dmesg[i],100, NETCONN_COPY);
        	i=i+100;
        }
#endif
        ut_sprintf(message, "</pre></body></html>");
        (void) netconn_write(session, message, ut_strlen(message), NETCONN_COPY);

        (void) netconn_disconnect(session);
        (void) netconn_delete(session);
    }
}

int start_webserver()
{
	int ret;

    ret=sc_createKernelThread(webserver_thread,NULL,(unsigned char *)"web_server");

    return 0;
}
