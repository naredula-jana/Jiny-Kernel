

typedef   unsigned char uint8_t;
void old_copy(uint8_t *dest, uint8_t *src, long len){

	uint8_t *sp = (const uint8_t *)src;
	uint8_t *dp = (uint8_t *)dest;
	long i=0;
	if (1){
			unsigned long *dst_p = dp;
			unsigned long *src_p = sp;
			while (len > 8) {
				*dst_p = *src_p;
				dst_p++;
				src_p++;
				len = len - 8;
			}
			dp = dst_p;
			sp = src_p;
		}
		while (len > 0) {
			*dp = *sp;
			dp++;
			sp++;
			len--;
		}
}
#include "rte_memcpy.h"

int std_rte_memcpy(uint8_t *dest, uint8_t *src, long len){
	return rte_memcpy(dest,src,len);
}
