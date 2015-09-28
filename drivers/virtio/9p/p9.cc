//#define DEBUG_ENABLE 1
#include "file.hh"
#include "jdevice.h"
extern "C"{
#include "9p.h"
#define cpu_to_le16(x) x /* already intel is little indian */
#define cpu_to_le32(x) x /* already intel is little indian */
#define cpu_to_le64(x) x /* already intel is little indian */
#define le16_to_cpu(x) x
#define le32_to_cpu(x) x
#define le64_to_cpu(x) x

#define min(x,y) x<y?x:y

static size_t pdu_read(struct p9_fcall *pdu, void *data, size_t size) {
	size_t len = min(pdu->size - pdu->offset, size);
	ut_memcpy(data, &pdu->sdata[pdu->offset], len);
	pdu->offset += len;
	return size - len;
}

static size_t pdu_write(struct p9_fcall *pdu, const void *data, size_t size) {
	size_t len = min(pdu->capacity - pdu->size, size);
	ut_memcpy(&pdu->sdata[pdu->size], (unsigned char *)data, len);
	pdu->size += len;
	return size - len;
}

int p9_pdu_init(struct p9_fcall *pdu, uint8_t type, uint16_t tag, p9_client_t *client, unsigned long addr, unsigned long len) {
    pdu->client= client;
	pdu->capacity = len;
	pdu->sdata = (unsigned char *)addr;
	if (type != 0) {
		pdu->type = type;
		pdu->tag = tag;
		pdu->size = 7;
	} else {
		pdu->type = 0;
		pdu->tag = 0;
		pdu->size = len;
		pdu->offset = 0;
	}

	return 1;
}
int p9_pdu_read_v(struct p9_fcall *pdu, const char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);
	return p9_pdu_read(pdu, fmt, ap);
}
int p9_pdu_write_v(struct p9_fcall *pdu, const char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);
	return p9_pdu_write(pdu, fmt, ap);
}
int p9_pdu_finalize(struct p9_fcall *pdu) {
	int size = pdu->size;
	int err;

	pdu->size = 0;
	err = p9_pdu_write_v(pdu, "dbw", size, pdu->type, pdu->tag);
	if (pdu->client->type == P9_TYPE_TWRITE){
		pdu->size = size + pdu->client->userdata_len;
	}else{
	    pdu->size = size;
	}

#ifdef CONFIG_NET_9P_DEBUG
	if ((p9_debug_level & P9_DEBUG_PKT) == P9_DEBUG_PKT)
	p9pdu_dump(0, pdu);
#endif

	//  P9_DPRINTK(P9_DEBUG_9P, ">>> size=%d type: %d tag: %d\n", pdu->size,
	//                                                pdu->id, pdu->tag);

	return err;
}

int p9_pdu_read(struct p9_fcall *pdu, const char *fmt, va_list ap) {
	const char *ptr;
	int errcode = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b': {
			unsigned char *val =(unsigned char *) va_arg(ap, int8_t *);
			if (pdu_read(pdu, val, sizeof(*val))) {
				errcode = -1;
				break;
			}
		}
			break;
		case 'w': {
			uint16_t *val = (uint16_t *)va_arg(ap, int16_t *);
			uint16_t le_val;
			if (pdu_read(pdu, &le_val, sizeof(le_val))) {
				errcode = -2;
				break;
			}
			*val = le16_to_cpu(le_val);
		}
			break;
		case 'd': {
			uint32_t *val = (uint32_t *)va_arg(ap, int32_t *);
			uint32_t le_val;
			if (pdu_read(pdu, &le_val, sizeof(le_val))) {
				errcode = -3;
				break;
			}
			*val = le32_to_cpu(le_val);
		}
			break;

		case 'q': {
			uint64_t *val =(uint64_t *) va_arg(ap, int64_t *);
			uint64_t le_val;
			if (pdu_read(pdu, &le_val, sizeof(le_val))) {
				errcode = -6;
				break;
			}
			*val = le64_to_cpu(le_val);
		}
			break;

		case 's': {
			char *sptr =(char *) va_arg(ap, char **);
			uint16_t len;

			errcode = p9_pdu_read_v(pdu, "w", &len);
			if (errcode)
				break;

			if (pdu_read(pdu, sptr, len)) {
				errcode = -5;

			} else
				sptr[len] = 0;
		}
			break;

		}
	}

	return errcode;
}
int p9_pdu_write(struct p9_fcall *pdu, const char *fmt, va_list ap) {
	const char *ptr;
	int errcode = 0;

	DEBUG("P9Write: ");
	for (ptr = fmt; *ptr; ptr++) {

		switch (*ptr) {
		case 'b': {
			unsigned char val = va_arg(ap, int);
			DEBUG(" b:%x",val);
			if (pdu_write(pdu, &val, sizeof(val)))
				errcode = -1;
		}
			break;
		case 'w': {
			uint16_t val = cpu_to_le16(va_arg(ap, int));
			DEBUG(" w:%x(%d)",val,val);
			if (pdu_write(pdu, &val, sizeof(val)))
				errcode = -2;
		}
			break;
		case 'd': {
			uint32_t val = cpu_to_le32(va_arg(ap, int32_t));
			DEBUG(" d:%x(%d)",val,val);
			if (pdu_write(pdu, &val, sizeof(val)))
				errcode = -3;
		}
			break;
		case 'q': {
			uint64_t val = cpu_to_le64(va_arg(ap, int64_t));
			DEBUG(" q:%x(%d)",val,val);
			if (pdu_write(pdu, &val, sizeof(val)))
				errcode = -3;
		}
			break;
		case 's': {
			const char *sptr = va_arg(ap, const char *);

			uint16_t len = 0;
			if (sptr) {
				len = ut_strlen(sptr);
				//len = min_t(uint16_t, strlen(sptr),
				//                       USHRT_MAX);
			}
			errcode = p9_pdu_write_v(pdu, "w", len);
			if (!errcode && pdu_write(pdu, sptr, len))
				errcode = -1;
			DEBUG(" s:%s",sptr);
		}
			break;

		}
	}
	DEBUG(" \n");

	return errcode;
}
/****************************************************************************************/
static int stat_request=0;

#include "../virtio_pci.h"
void *p9_dev = 0;
wait_queue *p9_waitq;
void *virtio_jdriver_getvq(void *driver, int index);
unsigned long p9_write_rpc(p9_client_t *client, const char *fmt, ...) { /* The call will be blocked till the reply is receivied */
	p9_fcall_t pdu;
	int ret, i;
	unsigned long addr;
	va_list ap;
	va_start(ap, fmt);

	p9_pdu_init(&pdu, client->type, client->tag, client, client->pkt_buf,
			client->pkt_len);
	stat_request++;
	ret = p9_pdu_write(&pdu, fmt, ap);
	va_end(ap);
	p9_pdu_finalize(&pdu);

	//ut_printf(" cd :%x cl:%x pd:%x  \n",client->user_data, client->userdata_len,client->pkt_buf);
#if 1
	if (client->user_data!= 0){
		pc_check_valid_addr(client->user_data, client->userdata_len);
	}
#endif
	struct scatterlist sg[4];
	unsigned int out, in;
	sg[0].page_link = (unsigned long)client->pkt_buf;
	sg[0].length = 1024;
	//sg[0].length = client->pkt_len/2;
	sg[0].offset = 0;
	out = 1;
	if (client->type == P9_TYPE_TREAD) {
		sg[1].page_link = client->pkt_buf + 1024;
		sg[1].length = 11; /* exactly 11 bytes for read response header , data will be from user buffer*/
		sg[1].offset = 0;
		sg[2].page_link = client->user_data;
		sg[2].length = client->userdata_len;
		sg[2].offset = 0;
		in = 2;
	} else if (client->type == P9_TYPE_TWRITE) {
		sg[1].page_link = (unsigned long)client->user_data;
		sg[1].length = client->userdata_len;
		sg[1].offset = 0;
		sg[0].length = 23; /* this for header , eventhough it is having space pick the data from sg[1] */

		sg[2].page_link = client->pkt_buf + 1024;
		sg[2].length = client->pkt_len-1024;
		sg[2].offset = 0;
		out = 2;
		in = 1;
	} else {
		sg[1].page_link = client->pkt_buf + 1024;
		sg[1].length = client->pkt_len-1024;
		sg[1].offset = 0;
		in = 1;
	}
#if 1 /* TODO: this should be called using the driver */
	virtio_queue *vq=virtio_jdriver_getvq(p9_dev,0);
	if (vq ==0){
		BUG();
	}
#if 0
	virtio_enable_cb(vq);
	virtio_add_buf_to_queue(vq, sg, out, in, sg[0].page_link, 0);
	virtio_queuekick(vq);
#endif
	vq->virtio_enable_cb();
	vq->virtio_add_buf_to_queue( sg, out, in, sg[0].page_link, 0);
	vq->virtio_queuekick();
#endif
	p9_waitq->wait(50);
	unsigned int len;
	len = 0;
	i = 0;
	addr = 0;
	while (i < 50 && addr == 0) {
		//addr = virtio_removeFromQueue(vq, &len); /* TODO : here sometime returns zero because of some race condition, the packet is not recevied */
		addr = vq->virtio_removeFromQueue(&len);
		i++;
		if (addr == 0) {
			//ut_log("sleep in P9 so sleeping for while requests:%d intr:%d\n",stat_request,stat_intr);
			//sc_sleep(300);
			p9_waitq->wait(30);
		}
	}
	if (addr != (unsigned long)client->pkt_buf) {
		BUG();
		DEBUG("9p write : got invalid address : %x \n", addr);
		return 0;
	}
	return client->pkt_buf;
}

int p9_read_rpc(p9_client_t *client, const char *fmt, ...) {
	unsigned char *recv;
	p9_fcall_t pdu;
	int ret;
	uint32_t total_len;
	unsigned char type;
	uint16_t tag;

	va_list ap;
	va_start(ap, fmt);

	recv = client->pkt_buf + 1024;
	p9_pdu_init(&pdu, 0, 0, client, recv, 1024);
	ret = p9_pdu_read_v(&pdu, "dbw", &total_len, &type, &tag);
	client->recv_type = type;
	ret = p9_pdu_read(&pdu, fmt, ap);
	va_end(ap);
	//ut_log("Recv Header ret:%x:%d total len :%x stype:%x(%d) rtype:%x(%d) tag:%x \n", ret,ret, total_len, client->type, client->type, type, type, tag);
	if (type == 107) { // TODO better way of handling other and this error
		recv[100] = '\0';
		DEBUG(" recv error data :%s: \n ", &recv[9]);
	}
	return ret;
}
}
