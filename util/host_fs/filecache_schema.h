#ifndef __FILECACHESCHEMA_H__
#define __FILECACHESCHEMA_H__

#define FS_VERSION 4
#define FS_MAGIC 0x12345678
#define MAX_FILENAMELEN 200
#define MAX_REQUESTS 100
#define PC_PAGESIZE 4096

enum {
	STATE_INVALID =0,
	STATE_VALID=1,
	STATE_UPDATE_INPROGRESS=2
};
enum {
	RESPONSE_NONE=0,
	RESPONSE_FAILED=1,
	RESPONSE_DONE=2
};
enum {
	REQUEST_OPEN=0,
	REQUEST_READ=1,
	REQUEST_WRITE=2,
	REQUEST_REMOVE=3,
	REQUEST_STAT=4,
	REQUEST_CLOSE=5,
	REQUEST_READDIR=6
};
enum {
	FLAG_CREATE=1,
	FLAG_OPEN
};

typedef struct {
	unsigned char state;
	unsigned char type;
	unsigned char flags; /* used when open operation */
	char filename[MAX_FILENAMELEN];
	unsigned long file_offset;
	unsigned long request_len;
	unsigned long shm_offset ; /* offset from the begining of shared memory */

	int guestos_pos;

	unsigned char response;	
	unsigned long response_len;
	unsigned long mtime_sec;
	unsigned long mtime_nsec;
	unsigned char data[PC_PAGESIZE];
}Request_t;
	
typedef struct {
	unsigned long magic_number;
	unsigned char version;
	unsigned char state;
	unsigned char generate_interrupt;
	int request_highindex;
	Request_t requests[MAX_REQUESTS];
}fileCache_t ;
#endif
