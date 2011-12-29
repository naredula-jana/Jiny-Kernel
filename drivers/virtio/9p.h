#ifndef JINY_9P_H
#define JINY_9P_H
#include <stdarg.h>
#include "interface.h"
#include "types.h"

enum p9_msg_t {
        P9_TYPE_TLERROR = 6,
        P9_TYPE_RLERROR,
        P9_TYPE_TSTATFS = 8,
        P9_TYPE_RSTATFS,
        P9_TYPE_TLOPEN = 12,
        P9_TYPE_RLOPEN,
        P9_TYPE_TLCREATE = 14,
        P9_TYPE_RLCREATE,
        P9_TYPE_TSYMLINK = 16,
        P9_TYPE_RSYMLINK,
        P9_TYPE_TMKNOD = 18,
        P9_TYPE_RMKNOD,
        P9_TYPE_TRENAME = 20,
        P9_TYPE_RRENAME,
        P9_TYPE_TREADLINK = 22,
        P9_TYPE_RREADLINK,
        P9_TYPE_TGETATTR = 24,
        P9_TYPE_RGETATTR,
        P9_TYPE_TSETATTR = 26,
        P9_TYPE_RSETATTR,
        P9_TYPE_TXATTRWALK = 30,
        P9_TYPE_RXATTRWALK,
        P9_TYPE_TXATTRCREATE = 32,
        P9_TYPE_RXATTRCREATE,
        P9_TYPE_TREADDIR = 40,
        P9_TYPE_RREADDIR,
        P9_TYPE_TFSYNC = 50,
        P9_TYPE_RFSYNC,
        P9_TYPE_TLOCK = 52,
        P9_TYPE_RLOCK,
        P9_TYPE_TGETLOCK = 54,
        P9_TYPE_RGETLOCK,
        P9_TYPE_TLINK = 70,
        P9_TYPE_RLINK,
        P9_TYPE_TMKDIR = 72,
        P9_TYPE_RMKDIR,
        P9_TYPE_TVERSION = 100,
        P9_TYPE_RVERSION,
        P9_TYPE_TAUTH = 102,
        P9_TYPE_RAUTH,
        P9_TYPE_TATTACH = 104,
        P9_TYPE_RATTACH,
        P9_TYPE_TERROR = 106,
        P9_TYPE_RERROR,
        P9_TYPE_TFLUSH = 108,
        P9_TYPE_RFLUSH,
        P9_TYPE_TWALK = 110,
        P9_TYPE_RWALK,
        P9_TYPE_TOPEN = 112,
        P9_TYPE_ROPEN,
        P9_TYPE_TCREATE = 114,
        P9_TYPE_RCREATE,
        P9_TYPE_TREAD = 116,
        P9_TYPE_RREAD,
        P9_TYPE_TWRITE = 118,
        P9_TYPE_RWRITE,
        P9_TYPE_TCLUNK = 120,
        P9_TYPE_RCLUNK,
        P9_TYPE_TREMOVE = 122,
        P9_TYPE_RREMOVE,
        P9_TYPE_TSTAT = 124,
        P9_TYPE_RSTAT,
        P9_TYPE_TWSTAT = 126,
        P9_TYPE_RWSTAT,
};

#define MAX_P9_FILEDEPTH 10
#define MAX_P9_FILELENGTH 200
typedef struct {
	unsigned char name[MAX_P9_FILELENGTH];
	uint32_t parent_fid,fid;
}p9_file_t;
#define MAX_P9_FILES 100
typedef struct  {
	uint16_t tag;
	uint8_t type,recv_type;
	unsigned long addr;


	unsigned char *pkt_buf;
	unsigned long pkt_len;

	unsigned char *user_data;
	unsigned long userdata_len;
	void *lock;

	uint32_t root_fid;
	uint32_t next_free_fid;
	p9_file_t files[MAX_P9_FILES];
}p9_client_t;

/*
* See Also: http://plan9.bell-labs.com/magic/man2html/2/fcall
*/
typedef struct p9_fcall {
	size_t size; /*  current size inclcive of itself , used while writing */
	uint8_t type;
	uint16_t tag;

	size_t offset; /* current offset , used while reading */

	size_t capacity; /* maximum sdata size */
	p9_client_t *client;
	uint8_t *sdata;
}p9_fcall_t;




extern int p9pdu_init(p9_fcall_t *pdu, uint8_t type, uint16_t tag, p9_client_t *client, unsigned long addr, unsigned long len);
extern int p9pdu_read_v(p9_fcall_t *pdu, const char *fmt, ...);
extern int p9pdu_read(p9_fcall_t *pdu, const char *fmt, va_list ap);
extern int p9pdu_write(p9_fcall_t *pdu,  const char *fmt, va_list ap);
extern unsigned long p9_write_rpc(p9_client_t *client, const char *fmt, ...);
extern int p9_read_rpc(p9_client_t *client, const char *fmt, ...);
extern int p9pdu_finalize(p9_fcall_t *pdu);

#endif
