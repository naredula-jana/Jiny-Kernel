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
#include "common.h"
#include "interface.h"
extern unsigned long stack,placement_address;
static void print_symbol(addr_t addr)
{
	int i;
        for (i=0; i< g_total_symbols; i++)
        {
		if ((addr>=g_symbol_table[i].address) && (addr<g_symbol_table[i+1].address))
		{
			ut_printf("   :%s + %x\n",g_symbol_table[i].name,(addr-g_symbol_table[i].address));
			return ;
		}
	}
	ut_printf("   :%x \n",addr);
	return;
}
void ut_showTrace(unsigned long *stack_top)
{
	unsigned long addr;
	unsigned long  sz,stack_end,code_end;
	int i;

	sz=(long)stack_top;
	sz=sz/4;
	sz=sz*4;
	stack_top=(unsigned char *)sz;
	i = 0;
	sz=(STACK_SIZE-1);
	sz=~sz;
	stack_end = (unsigned long)stack_top & (sz);
	stack_end = stack_end+STACK_SIZE-10;
	code_end = &placement_address;
//	cls();
	ut_printf("\nCALL Trace: %x  code_end:%x  %x :%x  \n",stack,code_end,stack_top,stack_end);
	if (stack_end) {
		while ((stack_top < stack_end) && i<12) {
			addr = *stack_top;
			stack_top=(unsigned char *)stack_top+4;
			if ((addr > 0x103000) && (addr<code_end)) {
				print_symbol(addr);
				i++;
			}
		}
	}
}
unsigned long ut_atol(char *p)
{
        unsigned long a;
        int i,m,k;
a=0;
m=0;
        for (i=0; p[i]!='\0'; i++)
        {
                if (p[i] == '0' && m==0) continue;
                m++;
                if (p[i]<='9' && p[i]>='0') k=p[i]-'0';
                else k=p[i]-'a'+0xa;
                if (m>1) a=a*0x10;
                a=a+k;
        }
return a;
}
unsigned int ut_atoi(char *p)
{         
        unsigned int a;
        int i,m,k;
a=0;
m=0;         
        for (i=0; p[i]!='\0'; i++)
        {
                if (p[i] == '0' && m==0) continue;
                m++;
                if (p[i]<='9' && p[i]>='0') k=p[i]-'0';
                else k=p[i]-'a'+0xa;
                if (m>1) a=a*0x10;
                a=a+k;                                                 
        }
return a;
}
// Copy len bytes from src to dest.
uint8_t *g_dest=0;
unsigned long *g_src=0;
//uint8_t *g_sp=0;
unsigned long g_len=0;
//unsigned long g_ddd=0x123;
void ut_memcpy(uint8_t *dest, uint8_t *src, long len)
{
	uint8_t *sp = (const uint8_t *)src;
	uint8_t *dp = (uint8_t *)dest;
	long i=0;

	g_dest=dp;
	g_len=len;
//	g_sp=&k;
//g_ddd=0x123;
ut_printf(" src:%x dest:%x len:%x \n",src,dest,len);
	for(i=len; i>0; i--) 
	{
		*dp = *sp;
	//	g_ddd++;
		dp++;
		sp++;
	}
//g_ddd=0x123;
	//ut_printf(" memcpy Len :%x k:%x \n",len,k);
}
uint8_t *tem1=0;
uint8_t tem2=0;
long tem3=0;
// Write len copies of val into dest.
void ut_memset(uint8_t *dest, uint8_t val, long len)
{
	uint8_t *temp = (uint8_t *)dest;
	long i;
#if 0
tem1=&i; /* TODO : if the above two lines removed then the code hangs */
tem2=val;
tem3=len;
#endif
	ut_printf("memset NEW dest :%x val :%x LEN addr:%x temp:%x \n",dest,val,&len,&temp);/* TODO */
	for ( i=len; i != 0; i--) *temp++ = val;
	return ;
}

// Compare two strings. Should return -1 if 
// str1 < str2, 0 if they are equal or 1 otherwise.
int ut_strcmp(char *str1, char *str2)
{
	int i = 0;
	int failed = 0;
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

// Copy the NULL-terminated string src into dest, and
// return dest.
char *ut_strcpy(char *dest, const char *src)
{
	do
	{
		*dest++ = *src++;
	}
	while (*src != 0);
	*dest=0;
	return dest;
}

// Concatenate the NULL-terminated string src onto
// the end of dest, and return dest.
char *ut_strcat(char *dest, const char *src)
{
	while (*dest != 0)
	{
		*dest = *dest++;
	}

	do
	{
		*dest++ = *src++;
	}
	while (*src != 0);
	return dest;
}

int ut_strlen(const char * s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}
