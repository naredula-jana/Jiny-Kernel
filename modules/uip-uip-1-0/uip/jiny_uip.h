#ifndef __JINYUIP_H__
#define __JINYUIP_H__

#include "types.h"

struct jiny_uip{
	u8_t *uip_buf;

	void *uip_appdata;               /* The uip_appdata pointer points to
	                    application data. */

	void *uip_sappdata;
	u16_t uip_len, uip_slen;   /* The uip_len is either 8 or 16 bits,
	                depending on the maximum packet
	                size. */

	u8_t uip_flags;     /* The uip_flags variable is used for
	                communication between the TCP/IP stack
	                and the application program. */
//	struct uip_conn *uip_conn;
	struct uip_udp_conn_struct *uip_udp_conn_s;
	unsigned char *data;
	unsigned long data_len;
	u8_t callback_flags;
}  __attribute__ ((aligned (64)));


#define MAX_CPUS 10

extern struct jiny_uip jiny_uip[MAX_CPUS];

#define uip_buf     jiny_uip[getcpuid()].uip_buf
#define uip_appdata jiny_uip[getcpuid()].uip_appdata
#define uip_sappdata jiny_uip[getcpuid()].uip_sappdata
#define uip_len jiny_uip[getcpuid()].uip_len
#define uip_slen jiny_uip[getcpuid()].uip_slen
#define uip_flags jiny_uip[getcpuid()].uip_flags
//#define uip_conn jiny_uip[getcpuid()].uip_conn
#define uip_udp_conn jiny_uip[getcpuid()].uip_udp_conn_s
#define jiny_uip_data jiny_uip[getcpuid()].data
#define jiny_uip_dlen jiny_uip[getcpuid()].data_len
#define jiny_uip_callback_flags jiny_uip[getcpuid()].callback_flags

#endif



