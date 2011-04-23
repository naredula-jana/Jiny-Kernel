#include <stdarg.h>
int errno;
#include "jlibc.h"

char out_buf[1024];
int curr_index=0; 

ut_putchar(int c)
{
	
	out_buf[curr_index]=(char )c;
	out_buf[curr_index+1]='\0';
	curr_index++;
//	my_write(buf);
}
static void itoa (char *buf, int buf_len, int base, unsigned long d)
{
	char *p = buf;
	char *p1, *p2;
	unsigned long ud = d;
	int len=buf_len;
	int divisor = 10;

	/* If %d is specified and D is minus, put `-' in the head.  */
	if (base == 'd' && d < 0)
	{
		*p++ = '-';
		buf++;
		ud = -d;
	}
	else if (base == 'x')
		divisor = 16;

	/* Divide UD by DIVISOR until UD == 0.  */
	do
	{
		int remainder = ud % divisor;
		len--;
		if (len < 1)
		{
			while(1) ;
		}
		*p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
		if (divisor != 16 && divisor !=10) /* TODO : test purpose */
		{
			while(1);
		}
	}
	while (ud /= divisor);
	/* Terminate BUF.  */
	*p = 0;

	/* Reverse BUF.  */
	p1 = buf;
	p2 = p - 1;
	while (p1 < p2)
	{
		char tmp = *p1;
		*p1 = *p2;
		*p2 = tmp;
		p1++;
		p2--;
	}
}
int strlen(const char * s)
{
        const char *sc;

        for (sc = s; *sc != '\0'; ++sc)
                /* nothing */;
        return sc - s;
}
unsigned long printf (const char *format, ...)
{
	char **arg = (char **) &format;
	int c;
	unsigned long  val,flags,ret;
	int i;
	char buf[40];

	va_list vl;
	va_start(vl,format);

	curr_index=0;

	arg++;
	i=0;
	while ((c = *format++) != 0)
	{
		if (c != '%')
			ut_putchar (c);
		else
		{
			char *p;

			c = *format++;
			switch (c)
			{
				case 'd':
				case 'u':
				case 'x':
				case 'i':

					if (c=='i')
					{
						val=va_arg(vl,unsigned int);
						c='x';
					}
					else
						val=va_arg(vl,long);

					itoa (buf,39, c, val);
					i++;
					p = buf;
					goto string;
					break;
				case 's':
					val=va_arg(vl,long);
					p = val;
					if (! p)
						p = "(null)";

string:
					while (*p)
						ut_putchar (*p++);
					break;

				default:
					ut_putchar (*((int *) arg++));
					break;
			}
		}
	}

	ret=write(1,out_buf,strlen(out_buf)+1);
	va_end(vl);
	return ret;
}
