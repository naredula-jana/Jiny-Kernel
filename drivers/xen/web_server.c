
//#include <netfront.h>
#include <lwip/api.h>

//#define __types_h
//#include "xen.h"


#include "common.h"

extern unsigned char g_dmesg[MAX_DMESG_LOG];
extern unsigned long g_dmesg_index;


static char message[300];

static void run_webserver(void *p)
{
    struct ip_addr listenaddr = { 0 };
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

        session = netconn_accept(listener);
        if (session == NULL)
            continue;

        ut_sprintf(message, "<html><body><pre> Jiny Kernel Dmesg max_len:%d  curr_len:%d \n",MAX_DMESG_LOG,g_dmesg_index);
        (void) netconn_write(session, message, ut_strlen(message), NETCONN_COPY);

        i=0;
        while (i<g_dmesg_index)
        {
        	if (g_dmesg_index < MAX_DMESG_LOG)
        		(void) netconn_write(session, &g_dmesg[i],100, NETCONN_COPY);
        	i=i+100;
        }

        ut_sprintf(message, "</pre></body></html>");
        (void) netconn_write(session, message, ut_strlen(message), NETCONN_COPY);

        (void) netconn_disconnect(session);
        (void) netconn_delete(session);
    }
}

int start_webserver()
{
	int ret;
    //create_thread("server", run_webserver, NULL);
    ret=sc_createKernelThread(run_webserver,NULL,"web_server");

    return 0;
}
