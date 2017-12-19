#ifndef _JINYKERNEL_NETWORK_STACK_HH
#define _JINYKERNEL_NETWORK_STACK_HH

extern "C" {

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

struct tcp_connection{
	uint32_t magic_no;
	uint32_t send_seq_no,send_ack_no;
	uint32_t recv_seq_no,recv_ack_no;

#define MAX_TCPSND_WINDOW 40
	struct {
		unsigned char *buf;
		int len;
		uint32_t seq_no;
	}send_queue[MAX_TCPSND_WINDOW];

	uint16_t srcport, destport;
	uint32_t ip_saddr,ip_daddr;
	uint8_t  mac_dest[6],mac_src[6];

};

enum connection_state
{
	NETWORK_CONN_CREATED =0,
	NETWORK_CONN_INITIATED = 1,
	NETWORK_CONN_ESTABILISHED = 2,
	NETWORK_CONN_LISTEN = 3,
	NETWORK_CONN_CLOSED = 4
};
class network_connection{
public:
	int magic_no;
	int family;
	int type; /* udp or tcp */
	connection_state state;
	uint32_t dest_ip,src_ip;
	uint16_t dest_port,src_port;
	uint8_t 	protocol; /* ip_protocol , tcp or udp */

	struct {
		uint32_t dest_ip,src_ip;
		uint16_t dest_port,src_port;
	}new_child_connection; /* this for listening  tcp connection */

	void *proto_connection;  /* protocol connection */
	struct tcp_connection *tcp_conn;
};
class network_stack{
//	char temp_buff[8192];

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
