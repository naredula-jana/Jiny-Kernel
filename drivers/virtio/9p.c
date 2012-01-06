#define DEBUG_ENABLE 1
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
	ut_memcpy(&pdu->sdata[pdu->size], data, len);
	pdu->size += len;
	return size - len;
}

int p9pdu_init(struct p9_fcall *pdu, uint8_t type, uint16_t tag, p9_client_t *client, unsigned long addr, unsigned long len) {
    pdu->client= client;
	pdu->capacity = len;
	pdu->sdata = addr;
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
int p9pdu_read_v(struct p9_fcall *pdu, const char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);
	return p9pdu_read(pdu, fmt, ap);
}
int p9pdu_write_v(struct p9_fcall *pdu, const char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);
	return p9pdu_write(pdu, fmt, ap);
}
int p9pdu_finalize(struct p9_fcall *pdu) {
	int size = pdu->size;
	int err;

	pdu->size = 0;
	err = p9pdu_write_v(pdu, "dbw", size, pdu->type, pdu->tag);
	if (pdu->client->type == P9_TYPE_TWRITE)
		pdu->size = size + pdu->client->userdata_len;
	else
	    pdu->size = size;

#ifdef CONFIG_NET_9P_DEBUG
	if ((p9_debug_level & P9_DEBUG_PKT) == P9_DEBUG_PKT)
	p9pdu_dump(0, pdu);
#endif

	//  P9_DPRINTK(P9_DEBUG_9P, ">>> size=%d type: %d tag: %d\n", pdu->size,
	//                                                pdu->id, pdu->tag);

	return err;
}

int p9pdu_read(struct p9_fcall *pdu, const char *fmt, va_list ap) {
	const char *ptr;
	int errcode = 0;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b': {
			unsigned char *val = va_arg(ap, int8_t *);
			if (pdu_read(pdu, val, sizeof(*val))) {
				errcode = -1;
				break;
			}
		}
			break;
		case 'w': {
			uint16_t *val = va_arg(ap, int16_t *);
			uint16_t le_val;
			if (pdu_read(pdu, &le_val, sizeof(le_val))) {
				errcode = -2;
				break;
			}
			*val = le16_to_cpu(le_val);
		}
			break;
		case 'd': {
			uint32_t *val = va_arg(ap, int32_t *);
			uint32_t le_val;
			if (pdu_read(pdu, &le_val, sizeof(le_val))) {
				errcode = -3;
				break;
			}
			*val = le32_to_cpu(le_val);
		}
			break;

		case 'q': {
			uint64_t *val = va_arg(ap, int64_t *);
			uint64_t le_val;
			if (pdu_read(pdu, &le_val, sizeof(le_val))) {
				errcode = -6;
				break;
			}
			*val = le64_to_cpu(le_val);
		}
			break;

		case 's': {
			char *sptr = va_arg(ap, char **);
			uint16_t len;

			errcode = p9pdu_read_v(pdu, "w", &len);
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
int p9pdu_write(struct p9_fcall *pdu, const char *fmt, va_list ap) {
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
			errcode = p9pdu_write_v(pdu, "w", len);
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
