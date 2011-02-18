#define FS_VERSION 2
#define FS_MAGIC 0x12345678
#define MAX_FILENAMELEN 200
#define MAX_BUF 4096
#define MAX_FILES 100
#define MAX_REQUESTS 100
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
	
typedef struct {
	unsigned char state;
	unsigned char filename[MAX_FILENAMELEN];
	unsigned char filePtr[MAX_BUF+1];
	unsigned long offset;
	unsigned long len;
}ServerFiles_t;

typedef struct {
	unsigned char state;
	unsigned char filename[MAX_FILENAMELEN];
	unsigned char server_response;	
	unsigned long offset;
	unsigned long len;
}ClientRequest_t;
	
typedef struct {
	unsigned long magic_number;
	unsigned char version;
	unsigned char state;
	unsigned int server_highindex;
	int client_highindex;
	ClientRequest_t clientRequests[MAX_REQUESTS];
	ServerFiles_t serverFiles[MAX_FILES];
}fileCache_t ;

