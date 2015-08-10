/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/util.c
 *   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 *
 */
//#define DEBUG_ENABLE 1
#include <stdarg.h>
#include "common.h"
#include "interface.h"

unsigned long ut_atol(uint8_t *p, int format)
{
	unsigned long a;
	int i,m,k;
	int multiplier = 0x10;
	if (format == FORMAT_DECIMAL){
		multiplier = 10;
	}

	a=0;
	m=0;
	for (i=0; p[i]!='\0'; i++)
	{
		if (p[i] == '0' && m==0) continue;
		m++;
		if (p[i]<='9' && p[i]>='0') k=p[i]-'0';
		else k=p[i]-'a'+0xa;
		if (m>1) a=a*multiplier;
		a=a+k;
	}
	return a;
}
unsigned int ut_atoi(uint8_t *p, int format)
{
	unsigned int a;
	int i,m,k;
	int multiplier = 0x10;
	if (format == FORMAT_DECIMAL){
		multiplier = 10;
	}

	a=0;
	m=0;
	for (i=0; p[i]!='\0'; i++)
	{
		if (p[i] == '0' && m==0) continue;
		m++;
		if (p[i]<='9' && p[i]>='0') k=p[i]-'0';
		else return 0;
		if (m>1) a=a*multiplier;
		a=a+k;
	}
	return a;
}

int ut_min(int a, int b){
	if (a<b) return a;
	else return b;
}
unsigned long g_stat_memcpy_bytes = 0;
unsigned long g_stat_memcpy_reqs = 0;
// Copy len bytes from src to dest.
unsigned int g_conf_memcpy=0;
extern int std_rte_memcpy(uint8_t *dest, uint8_t *src, long len);
void ut_memcpy(uint8_t *dest, uint8_t *src, long len){
	uint8_t *sp = (const uint8_t *)src;
	uint8_t *dp = (uint8_t *)dest;
	long i=0;

	if ((ar_check_valid_address(dest,len)==JFAIL) || (ar_check_valid_address(src,len)==JFAIL)){
		ut_log(" Error Invalid address in ut_memcpy src:%x dest:%x \n",src,dest);
		BUG();
	}
	DEBUG(" src:%x dest:%x len:%x \n",src,dest,len);
#if 0
	if (g_conf_memcpy == 1 && len==4096){
		return std_rte_memcpy(dest,src,len);
	}
#endif
	g_stat_memcpy_bytes = g_stat_memcpy_bytes + len;
	g_stat_memcpy_reqs++;
//	if ((((unsigned long)dp & 0x7) == 0) && (((unsigned long)sp & 0x7)==0)) {
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
	return;
}
unsigned char test_dest[4096],test_src[4096];
int g_conf_memloops=1000;
void Jcmd_memcpy(void *arg1,void *arg2){
	int k;
	unsigned long s,e;

	s=ut_get_systemtime_ns();
	for (k=0; k<g_conf_memloops; k++){
		ut_memcpy(test_dest,test_src,4096);
	}
	e=ut_get_systemtime_ns();
	ut_printf(" duration : s:%d e:%d diff:%d us \n",s,e,(e-s)/1000);
}
// Write len copies of val into dest.
void ut_memset(uint8_t *dest, uint8_t val, long len)
{
	uint8_t *temp = (uint8_t *)dest;
	long i;
	DEBUG("memset NEW dest :%x val :%x LEN addr:%x len:%x temp:%x \n",dest,val,&len,len,&temp);/* TODO */
	for ( i=len; i != 0; i--) *temp++ = val;
	return ;
}
// Compare two strings. Should return -1 if
// str1 < str2, 0 if they are equal or 1 otherwise.
int ut_strncmp(uint8_t *str1,unsigned  char *str2, int n)
{
	int i = 0;
	int failed = 0;
	if (str1==0 || str2==0) return -1;
	while(str1[i] != '\0' && str2[i] != '\0' && i<n)
	{
		if(str1[i] != str2[i])
		{
			failed = 1;
			break;
		}
		i++;
	}
	// why did the loop exit?
	if( (str1[i] == '\0' && str2[i] != '\0') || (str1[i] != '\0' && str2[i] == '\0') )
		failed = 1;
	if (failed == 1 && str1[i] =='\0') failed=2;
	return failed;
}

// Compare two strings. Should return -1 if 
// str1 < str2, 0 if they are equal or 1 otherwise.
int ut_strcmp(uint8_t *str1,unsigned  char *str2)
{
	int i = 0;
	int failed = 0;
	if (str1==0 || str2==0) return -1;
	while(str1[i] != '\0' && str2[i] != '\0')
	{
		if(str1[i] != str2[i])
		{
			failed = 1;
			break;
		}
		i++;
	}
	// why did the loop exit?
	if( (str1[i] == '\0' && str2[i] != '\0') || (str1[i] != '\0' && str2[i] == '\0') )
		failed = 1;
	if (failed == 1 && str1[i] =='\0') failed=2;
	return failed;
}

int ut_memcmp(uint8_t *m1, uint8_t *m2,int len)
{
	int i = 0;
	while(i<len && m1[i] == m2[i])
	{
		i++;
	}
	if (i == len) return 0;
	return 1;
}

// Copy the NULL-terminated string src into dest, and
// return dest.
uint8_t *ut_strcpy(uint8_t *dest, const uint8_t *src)
{
	if (src==0 ) return dest;
	do
	{
		*dest++ = *src++;
	}
	while (*src != 0);
	*dest=0;
	return dest;
}
uint8_t *ut_strncpy(uint8_t *dest, const uint8_t *src,int n)
{
	int len=0;
        do
        {
                *dest++ = *src++;
		len++;
        }
        while (*src != 0 && len<n);
        *dest=0;
        return dest;
}
uint8_t *ut_strstr(uint8_t *s1,uint8_t *s2)
{
    int i,j;
    if (s1==0 || s2==0) return 0;

	for (i = 0; s1[i] != '\0'; i++) {
		if (s1[i] == s2[0]) {
			for (j = i; s2[j-i]!='0' && s1[j]!='\0'; j++) {
				if (s1[j] != s2[j-i]){
					break;
				}
			}
			if (s2[j-i]=='\0') return &s1[i] ;
		}
	}

    return 0;
}
// Concatenate the NULL-terminated string src onto
// the end of dest, and return dest.
uint8_t *ut_strcat(uint8_t *dest, const uint8_t *src)
{
	while (*dest != 0)
	{
	//	*dest = *dest++;  TODO: gcc 4.8 compiler interprets different from 4.4
		dest++;
 	}

	do
	{
		*dest++ = *src++;
	}
	while (*src != 0);
	*dest = 0;
	return dest;
}

int ut_strlen(const uint8_t * s)
{
	const uint8_t *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return (int)(sc - s);
}
#define ZEROPAD 1               /* pad with zero */
#define SIGN    2               /* unsigned/signed long */
#define PLUS    4               /* show plus */
#define SPACE   8               /* space if plus */
#define LEFT    16              /* left justified */
#define SPECIAL 32              /* 0x */
#define LARGE   64              /* use 'ABCDEF' instead of 'abcdef' */
static int skip_atoi(const uint8_t **s)
{
    int i=0;

    while (isdigit(**s))
        i = i*10 + *((*s)++) - '0';
    return i;
}
static char * number(uint8_t * buf, uint8_t * end, long long num, int base, int size, int precision, int type)
{
    char c,sign,tmp[66];
    const uint8_t *digits;
    const uint8_t small_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    const uint8_t large_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    digits = (type & LARGE) ? large_digits : small_digits;
    if (type & LEFT)
        type &= ~ZEROPAD;
    if (base < 2 || base > 36)
        return buf;
    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;
    if (type & SIGN) {
        if (num < 0) {
            sign = '-';
            num = -num;
            size--;
        } else if (type & PLUS) {
            sign = '+';
            size--;
        } else if (type & SPACE) {
            sign = ' ';
            size--;
        }
    }
    if (type & SPECIAL) {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
    i = 0;
    if (num == 0)
        tmp[i++]='0';
    else
    {
        /* XXX KAF: force unsigned mod and div. */
        unsigned long long num2=(unsigned long long)num;
        unsigned int base2=(unsigned int)base;
        while (num2 != 0) { tmp[i++] = digits[num2%base2]; num2 /= base2; }
    }
    if (i > precision)
        precision = i;
    size -= precision;
    if (!(type&(ZEROPAD+LEFT))) {
        while(size-->0) {
            if (buf <= end)
                *buf = ' ';
            ++buf;
        }
    }
    if (sign) {
        if (buf <= end)
            *buf = sign;
        ++buf;
    }
    if (type & SPECIAL) {
        if (base==8) {
            if (buf <= end)
                *buf = '0';
            ++buf;
        } else if (base==16) {
            if (buf <= end)
                *buf = '0';
            ++buf;
            if (buf <= end)
                *buf = digits[33];
            ++buf;
        }
    }
    if (!(type & LEFT)) {
        while (size-- > 0) {
            if (buf <= end)
                *buf = c;
            ++buf;
        }
    }
    while (i < precision--) {
        if (buf <= end)
            *buf = '0';
        ++buf;
    }
    while (i-- > 0) {
        if (buf <= end)
            *buf = tmp[i];
        ++buf;
    }
    while (size-- > 0) {
        if (buf <= end)
            *buf = ' ';
        ++buf;
    }
    return buf;
}
static int vsnprintf(uint8_t *buf, size_t size, const uint8_t *fmt, va_list args){
    int len;
    unsigned long long num;
    int i, base;
    uint8_t *str, *end, c;
    const uint8_t *s;

    int flags;          /* flags to number() */

    int field_width;    /* width of output field */
    int precision;              /* min. # of digits for integers; max
                                   number of chars for from string */
    int qualifier;              /* 'h', 'l', or 'L' for integer fields */
                                /* 'z' support added 23/7/1999 S.H.    */
                                /* 'z' changed to 'Z' --davidm 1/25/99 */

    str = buf;
    end = buf + size - 1;

    if (end < buf - 1) {
        end = ((void *) -1);
        size = end - buf + 1;
    }

    for (; *fmt ; ++fmt) {
        if (*fmt != '%') {
            if (str <= end)
                *str = *fmt;
            ++str;
            continue;
        }

        /* process flags */
        flags = 0;
    repeat:
        ++fmt;          /* this also skips first '%' */
        switch (*fmt) {
        case '-': flags |= LEFT; goto repeat;
        case '+': flags |= PLUS; goto repeat;
        case ' ': flags |= SPACE; goto repeat;
        case '#': flags |= SPECIAL; goto repeat;
        case '0': flags |= ZEROPAD; goto repeat;
        }

        /* get field width */
        field_width = -1;
        if (isdigit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
            ++fmt;
            /* it's the next argument */
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */
        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (isdigit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                ++fmt;
                          /* it's the next argument */
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }

        /* get the conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt =='Z') {
            qualifier = *fmt;
            ++fmt;
            if (qualifier == 'l' && *fmt == 'l') {
                qualifier = 'L';
                ++fmt;
            }
        }
        if (*fmt == 'q') {
            qualifier = 'L';
            ++fmt;
        }

        /* default base */
        base = 10;

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT)) {
                while (--field_width > 0) {
                    if (str <= end)
                        *str = ' ';
                    ++str;
                }
            }
            c = (uint8_t) va_arg(args, int);
            if (str <= end)
                *str = c;
            ++str;
            while (--field_width > 0) {
                if (str <= end)
                    *str = ' ';
                ++str;
            }
            continue;

        case 's':
            s = va_arg(args, char *);
            if (!s)
                s = "<NULL>";
            //len = strlen(s, precision);

            len = ut_strlen((uint8_t *)s);

            if (!(flags & LEFT)) {
                while (len < field_width--) {
                    if (str <= end)
                        *str = ' ';
                    ++str;
                }
            }
            for (i = 0; i < len; ++i) {
                if (str <= end)
                    *str = *s;
                ++str; ++s;
            }
            while (len < field_width--) {
                if (str <= end)
                    *str = ' ';
                ++str;
            }
            continue;

        case 'p':
            if (field_width == -1) {
                field_width = 2*sizeof(void *);
                flags |= ZEROPAD;
            }
            str = number(str, end,
                         (unsigned long) va_arg(args, void *),
                         16, field_width, precision, flags);
            continue;


        case 'n':
            if (qualifier == 'l') {
                long * ip = va_arg(args, long *);
                *ip = (str - buf);
            } else if (qualifier == 'Z') {
                size_t * ip = va_arg(args, size_t *);
                *ip = (str - buf);
            } else {
                int * ip = va_arg(args, int *);
                *ip = (str - buf);
            }
            continue;

        case '%':
            if (str <= end)
                *str = '%';
            ++str;
            continue;

            /* integer number formats - set up the flags and "break" */
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LARGE;
        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            break;

        default:
            if (str <= end)
                *str = '%';
            ++str;
            if (*fmt) {
                if (str <= end)
                    *str = *fmt;
                ++str;
            } else {
                --fmt;
            }
            continue;
        }
        if (qualifier == 'L')
            num = va_arg(args, long long);
        else if (qualifier == 'l') {
            num = va_arg(args, unsigned long);
            if (flags & SIGN)
                num = (signed long) num;
        } else if (qualifier == 'Z') {
            num = va_arg(args, size_t);
        } else if (qualifier == 'h') {
            num = (unsigned short) va_arg(args, int);
            if (flags & SIGN)
                num = (signed short) num;
        } else {
            num = va_arg(args, unsigned int);
            if (flags & SIGN)
                num = (signed int) num;
        }

        str = number(str, end, num, base,
                     field_width, precision, flags);
    }
    if (str <= end)
        *str = '\0';
    else if (size > 0)
        /* don't write out a null byte if the buf size is zero */
        *end = '\0';
    /* the trailing null byte doesn't count towards the total
     * ++str;
     */
    return str-buf;
}

/**
 * snprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 */
int ut_snprintf(uint8_t * buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i=vsnprintf(buf,size,fmt,args);
    va_end(args);
    return i;
}

/**
 * vsprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want sprintf instead.
 */
static int vsprintf(char *buf, const uint8_t *fmt, va_list args)
{
    return vsnprintf(buf, 0xFFFFFFFFUL, fmt, args);
}


/**
 * sprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 */
int ut_sprintf(uint8_t * buf, const uint8_t *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i=vsprintf(buf,fmt,args);
    va_end(args);
    return i;
}

#define _U      0x01    /* upper */
#define _L      0x02    /* lower */
#define _D      0x04    /* digit */
#define _C      0x08    /* cntrl */
#define _P      0x10    /* punct */
#define _S      0x20    /* white space (space/lf/tab) */
#define _X      0x40    /* hex digit */
#define _SP     0x80    /* hard space (0x20) */

uint8_t _ctype[] = {
_C,_C,_C,_C,_C,_C,_C,_C,                        /* 0-7 */
_C,_C|_S,_C|_S,_C|_S,_C|_S,_C|_S,_C,_C,         /* 8-15 */
_C,_C,_C,_C,_C,_C,_C,_C,                        /* 16-23 */
_C,_C,_C,_C,_C,_C,_C,_C,                        /* 24-31 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,                    /* 32-39 */
_P,_P,_P,_P,_P,_P,_P,_P,                        /* 40-47 */
_D,_D,_D,_D,_D,_D,_D,_D,                        /* 48-55 */
_D,_D,_P,_P,_P,_P,_P,_P,                        /* 56-63 */
_P,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U,      /* 64-71 */
_U,_U,_U,_U,_U,_U,_U,_U,                        /* 72-79 */
_U,_U,_U,_U,_U,_U,_U,_U,                        /* 80-87 */
_U,_U,_U,_P,_P,_P,_P,_P,                        /* 88-95 */
_P,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L,      /* 96-103 */
_L,_L,_L,_L,_L,_L,_L,_L,                        /* 104-111 */
_L,_L,_L,_L,_L,_L,_L,_L,                        /* 112-119 */
_L,_L,_L,_P,_P,_P,_P,_C,                        /* 120-127 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                /* 128-143 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                /* 144-159 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,   /* 160-175 */
_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,       /* 176-191 */
_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,       /* 192-207 */
_U,_U,_U,_U,_U,_U,_U,_P,_U,_U,_U,_U,_U,_U,_U,_L,       /* 208-223 */
_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,       /* 224-239 */
_L,_L,_L,_L,_L,_L,_L,_P,_L,_L,_L,_L,_L,_L,_L,_L};      /* 240-255 */

#define __ismask(x) (_ctype[(int)(uint8_t)(x)])

#define isalnum(c)      ((__ismask(c)&(_U|_L|_D)) != 0)
#define isalpha(c)      ((__ismask(c)&(_U|_L)) != 0)
#define iscntrl(c)      ((__ismask(c)&(_C)) != 0)
#define isdigit(c)      ((__ismask(c)&(_D)) != 0)
#define isgraph(c)      ((__ismask(c)&(_P|_U|_L|_D)) != 0)
#define islower(c)      ((__ismask(c)&(_L)) != 0)
#define isprint(c)      ((__ismask(c)&(_P|_U|_L|_D|_SP)) != 0)
#define ispunct(c)      ((__ismask(c)&(_P)) != 0)
#define isspace(c)      ((__ismask(c)&(_S)) != 0)
#define isupper(c)      ((__ismask(c)&(_U)) != 0)
#define isxdigit(c)     ((__ismask(c)&(_D|_X)) != 0)
static inline uint8_t __tolower(uint8_t c)
{
        if (isupper(c))
                c -= 'A'-'a';
        return c;
}

static inline uint8_t __toupper(uint8_t c)
{
        if (islower(c))
                c -= 'a'-'A';
        return c;
}

#define tolower(c) __tolower(c)
#define toupper(c) __toupper(c)

unsigned long simple_strtoul(const uint8_t *cp,uint8_t **endp,unsigned int base)
{
    unsigned long result = 0,value;

    if (!base) {
        base = 10;
        if (*cp == '0') {
            base = 8;
            cp++;
            if ((*cp == 'x') && isxdigit(cp[1])) {
                cp++;
                base = 16;
            }
        }
    }
    while (isxdigit(*cp) &&
           (value = isdigit(*cp) ? *cp-'0' : toupper(*cp)-'A'+10) < base) {
        result = result*base + value;
        cp++;
    }
    if (endp)
        *endp = (char *)cp;
    return result;
}
static long simple_strtol(const uint8_t *cp,uint8_t **endp,unsigned int base)
{
    if(*cp=='-')
        return -simple_strtoul(cp+1,endp,base);
    return simple_strtoul(cp,endp,base);
}
static unsigned long long simple_strtoull(const uint8_t *cp,uint8_t **endp,unsigned int base)
{
    unsigned long long result = 0,value;

    if (!base) {
        base = 10;
        if (*cp == '0') {
            base = 8;
            cp++;
            if ((*cp == 'x') && isxdigit(cp[1])) {
                cp++;
                base = 16;
            }
        }
    }
    while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
                                                               ? toupper(*cp) : *cp)-'A'+10) < base) {
        result = result*base + value;
        cp++;
    }
    if (endp)
        *endp = (char *)cp;
    return result;
}

static long long simple_strtoll(const uint8_t *cp,uint8_t **endp,unsigned int base)
{
    if(*cp=='-')
        return -simple_strtoull(cp+1,endp,base);
    return simple_strtoull(cp,endp,base);
}


#define INT_MAX         0x7fffffff
int vsscanf(const uint8_t * buf, const uint8_t * fmt, va_list args)
{
        const uint8_t *str = buf;
        char *next;
        char digit;
        int num = 0;
        int qualifier;
        int base;
        int field_width;
        int is_sign = 0;

        while(*fmt && *str) {
                /* skip any white space in format */
                /* white space in format matchs any amount of
                 * white space, including none, in the input.
                 */
                if (isspace(*fmt)) {
                        while (isspace(*fmt))
                                ++fmt;
                        while (isspace(*str))
                                ++str;
                }
                /* anything that is not a conversion must match exactly */
                 if (*fmt != '%' && *fmt) {
                         if (*fmt++ != *str++)
                                 break;
                         continue;
                 }

                 if (!*fmt)
                         break;
                 ++fmt;

                 /* skip this conversion.
                  * advance both strings to next white space
                  */
                 if (*fmt == '*') {
                         while (!isspace(*fmt) && *fmt)
                                 fmt++;
                         while (!isspace(*str) && *str)
                                 str++;
                         continue;
                 }

                 /* get field width */
                 field_width = -1;
                 if (isdigit(*fmt))
                         field_width = skip_atoi(&fmt);

                 /* get conversion qualifier */
                 qualifier = -1;
                 if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' ||
                     *fmt == 'Z' || *fmt == 'z') {
                         qualifier = *fmt++;
                         if (unlikely(qualifier == *fmt)) {
                                 if (qualifier == 'h') {
                                         qualifier = 'H';
                                         fmt++;
                                 } else if (qualifier == 'l') {
                                         qualifier = 'L';
                                         fmt++;
                                 }
                         }
                 }
                 base = 10;
                 is_sign = 0;

                 if (!*fmt || !*str)
                         break;

                 switch(*fmt++) {
                 case 'c':
                 {
                         char *s = (char *) va_arg(args,char*);
                         if (field_width == -1)
                                 field_width = 1;
                         do {
                                 *s++ = *str++;
                         } while (--field_width > 0 && *str);
                         num++;
                 }
                 continue;
                 case 's':
                 {
                         char *s = (char *) va_arg(args, char *);
                         if(field_width == -1)
                                 field_width = INT_MAX;
                         /* first, skip leading white space in buffer */
                         while (isspace(*str))
                                 str++;

                         /* now copy until next white space */
                         while (*str && !isspace(*str) && field_width--) {
                                 *s++ = *str++;
                         }
                         *s = '\0';
                         num++;
                 }
                 continue;
                 case 'n':
                          /* return number of characters read so far */
                  {
                          int *i = (int *)va_arg(args,int*);
                          *i = str - buf;
                  }
                  continue;
                  case 'o':
                          base = 8;
                          break;
                  case 'x':
                  case 'X':
                          base = 16;
                          break;
                  case 'i':
                          base = 0;
                  case 'd':
                          is_sign = 1;
                  case 'u':
                          break;
                  case '%':
                          /* looking for '%' in str */
                          if (*str++ != '%')
                                  return num;
                          continue;
                  default:
                          /* invalid format; stop here */
                          return num;
                  }

                  /* have some sort of integer conversion.
                   * first, skip white space in buffer.
                   */
                  while (isspace(*str))
                          str++;

                  digit = *str;
                  if (is_sign && digit == '-')
                          digit = *(str + 1);

                  if (!digit
                      || (base == 16 && !isxdigit(digit))
                      || (base == 10 && !isdigit(digit))
                      || (base == 8 && (!isdigit(digit) || digit > '7'))
                      || (base == 0 && !isdigit(digit)))
                                  break;

                  switch(qualifier) {
                  case 'H':       /* that's 'hh' in format */
                          if (is_sign) {
                                  signed char *s = (signed char *) va_arg(args,signed char *);
                                  *s = (signed char) simple_strtol(str,&next,base);
                          } else {
                                  uint8_t *s = (uint8_t *) va_arg(args, uint8_t *);
                                  *s = (uint8_t) simple_strtoul(str, &next, base);
                          }
                          break;
                  case 'h':
                          if (is_sign) {
                                  short *s = (short *) va_arg(args,short *);
                                  *s = (short) simple_strtol(str,&next,base);
                          } else {
                                  unsigned short *s = (unsigned short *) va_arg(args, unsigned short *);
                                  *s = (unsigned short) simple_strtoul(str, &next, base);
                          }
                          break;
                  case 'l':
                          if (is_sign) {
                                  long *l = (long *) va_arg(args,long *);
                                  *l = simple_strtol(str,&next,base);
                          } else {
                                  unsigned long *l = (unsigned long*) va_arg(args,unsigned long*);
                                  *l = simple_strtoul(str,&next,base);
                          }
                          break;
                  case 'L':
                          if (is_sign) {
                                  long long *l = (long long*) va_arg(args,long long *);
                                  *l = simple_strtoll(str,&next,base);
                          } else {
                                  unsigned long long *l = (unsigned long long*) va_arg(args,unsigned long long*);
                                  *l = simple_strtoull(str,&next,base);
                          }
                          break;
                  case 'Z':
                  case 'z':
                  {
                          size_t *s = (size_t*) va_arg(args,size_t*);
                          *s = (size_t) simple_strtoul(str,&next,base);
                  }
                  break;
                  default:
                          if (is_sign) {
                                  int *i = (int *) va_arg(args, int*);
                                  *i = (int) simple_strtol(str,&next,base);
                          } else {
                                  unsigned int *i = (unsigned int*) va_arg(args, unsigned int*);
                                  *i = (unsigned int) simple_strtoul(str,&next,base);
                          }
                          break;
                  }
                  num++;

                  if (!next)
                          break;
                  str = next;
          }
          return num;
  }

/**
 * sscanf - Unformat a buffer into a list of arguments
 * @buf:        input buffer
 * @fmt:        formatting of buffer
 * @...:        resulting arguments
 */
int sscanf(const uint8_t * buf, const uint8_t * fmt, ...)
{
        va_list args;
        int i;

        va_start(args,fmt);
        i = vsscanf(buf,fmt,args);
        va_end(args);
        return i;
}

