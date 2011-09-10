
//#include <netfront.h>
#include <lwip/api.h>

//#define __types_h
//#include "xen.h"
#include "common.h"

static char message[29];

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
        session = netconn_accept(listener);
        if (session == NULL)
            continue;

        ut_sprintf(message, "<html><body> Jiny Kernel Web server </body></html>\n");
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
