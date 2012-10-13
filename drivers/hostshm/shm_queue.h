typedef	unsigned long	u64;
typedef unsigned short  u16;
typedef unsigned int    u32;
#define SHM_SIZE 2*1024*1024
#define SHM_PAGE_SIZE 4096
#define RECV 1
#define SEND 2
#define MAX_BUF (SHM_PAGE_SIZE/sizeof(struct queue_desc ))
/* queue ring descriptors: 16 bytes.   */
struct queue_desc {
	u64 addr_offset; /* offset from the start of the shared memory */
	/* Length. */
	u32 len;
	/* The flags as indicated above. */
	u16 flags;
	/* We chain unused descriptors via this, too */
	u16 next;
};


struct buf_avail {
	u16 flags;
	u16 idx;
	u16 ring[MAX_BUF];
};

/* u32 is used here for ids for padding reasons. */
struct buf_used_elem {
	/* Index of start of used descriptor chain. */
	u32 id;
	/* Total length of the descriptor chain which was used (written to) */
	u32 len;
};

struct buf_used {
	u16 flags;
	u16 idx;
	struct buf_used_elem ring[MAX_BUF];
};
struct shm_queue {
	struct queue_desc desc[MAX_BUF];
	struct buf_avail avail;
	unsigned char pad[SHM_PAGE_SIZE-sizeof(struct buf_avail)];
	struct buf_used used;
};


/***********************************************************/
struct client_queue {
	struct cl_queue {
		struct shm_queue *shm_queue;
		int desc_head;
		int desc_list_length;

		int used_id;
	}recv_q,send_q;
	unsigned char *shm_start_addr;
};

struct server_queue {
	struct sr_queue {
		struct shm_queue *shm_queue;
		int avail_id;
	}recv_q,send_q;
	unsigned char *shm_start_addr;
};
struct buf_desc {
	unsigned char *buf;
	int len;
	int descr_id;
	int type;
	struct client_queue *q;
};


