#ifndef _JINYKERNEL_NETWORK_STACK_HH
#define _JINYKERNEL_NETWORK_STACK_HH

extern "C" {
#include "atomic.h"
extern void netstack_lock();
extern void netstack_unlock();
}

struct iovec {                    /* Scatter/gather array items */
    void  *iov_base;              /* Starting address */
    size_t iov_len;               /* Number of bytes to transfer */
};
#define WRITE_SLEEP_TILL_SEND 0x1
enum _socket_type
{
  SOCK_STREAM = 1,      /* Sequenced,reliable andconnection-based byte streams.  */
  SOCK_DGRAM = 2,       /* Connectionless, unreliable datagrams.  */
  SOCK_RAW = 3,         /* Raw protocol type.  */
  SOCK_RDM = 4,
  SOCK_STREAM_CHILD=200
};
#define AF_INET 2  /* family */
#define AF_INET6 10  /* family */

#define IPPROTO_UDP 0x11 /* protocol type */
#define IPPROTO_TCP 0x06
class network_connection ;

#define MAX_TCP_LISTEN 10
class network_connection{
public:
	int magic_no;
	int family;
	int type; /* udp or tcp */

	uint32_t dest_ip,src_ip;
	uint16_t dest_port,src_port;
	uint8_t 	protocol; /* ip_protocol , tcp or udp */

	void *proto_connection;  /* protocol connection */
	struct tcp_connection *tcp_conn;
	struct tcp_connection *new_tcp_conn[MAX_TCP_LISTEN];
};
class network_stack{
public:
	unsigned char *name;

	int open(network_connection *conn, int flag);
    int write(network_connection *conn, struct iovec *msg_iov, int iov_len);
    int read(network_connection *conn, uint8_t *raw_data, int raw_len, uint8_t *app_data, int app_maxlen);
	int close(network_connection *conn);
	int bind(network_connection *conn, uint16_t port);
	int connect(network_connection *conn);
};

#endif
