/* kernel.c - the C part of the kernel */
#include <stdarg.h>
#include "common.h"
/* Macros.  */

/* Check if the bit BIT in FLAGS is set.  */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))

/* Some screen stuff.  */
/* The number of columns.  */
#define COLUMNS			80
/* The number of lines.  */
#define LINES			24
/* The attribute of an character.  */
#define ATTRIBUTE		0x1f
/* The video memory address.  */
#define VIDEO			0xB8000

/* Variables.  */
/* Save the X position.  */
static int xpos;
/* Save the Y position.  */
static int ypos;
/* Point to the video memory.  */
static volatile unsigned char *video_g;
static unsigned char video_buffer_g[LINES][2*COLUMNS];
/* Forward declarations.  */
void cls (void);
static void itoa (char *buf,int buf_len, int base, unsigned long d);
void ut_putchar (int c);
void ut_printf (const char *format, ...);

/* Clear the screen and initialize VIDEO, XPOS and YPOS.  */
void ut_cls (void)
{
	int i;
	unsigned char *ptr;
	ptr=&(video_buffer_g[0][0]);	
	video_g = (unsigned char *) VIDEO;

	for (i = 0; i < COLUMNS * LINES * 2; i++)
	{
		*(video_g + i) = 0;
		*(ptr + i) = 0;
	}

	xpos = 0;
	ypos = 0;
}
static void refresh_screen (void)
{
        int i;
        unsigned char *ptr;
        ptr=&(video_buffer_g[0][0]);
        video_g = (unsigned char *) VIDEO;

        for (i = 1; i < COLUMNS * LINES * 2; i=i+2)
        {
		*(video_g + i) = 0x4f;
		*(ptr + i) = 0x4f;
        }

}
/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal.  */
//#define DIVISOR 16
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
			cli();
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
static void scroll()
{
	int i,x,y;
	unsigned char *ptr;

	ptr=&(video_buffer_g[0][0]);	
	video_g = (unsigned char *) VIDEO;
	for (i = 0; i < COLUMNS * (LINES-1) * 2; i++) /* copy the data from next line to prev line */
	{
		*(video_g + i) = ptr[i+2*COLUMNS]; 
		*(ptr + i) = ptr[i+2*COLUMNS]; 
	}
	for (i = COLUMNS * (LINES-1) * 2; i < COLUMNS * (LINES) * 2; i++) /* clear the last line */
	{
		*(video_g + i) = 0; 
		*(ptr + i) = 0; 
	}
	refresh_screen(); 
}
/* Put the character C on the screen.  */
static spinlock_t putchar_lock  = SPIN_LOCK_UNLOCKED;
void ut_putchar (int c)
{
	unsigned char *ptr;
	unsigned long  flags;

	spin_lock_irqsave(&putchar_lock, flags);	
	ptr=&(video_buffer_g[0][0]);	
	if (c == '\n' || c == '\r')
	{
newline:
		xpos = 0;
		ypos++;
		if (ypos >= LINES)
		{
			if (1)
			{
			scroll();
			ypos--;
			}else
			{
			ypos = 0;
			}
		}
        	spin_unlock_irqrestore(&putchar_lock, flags);	
		return;
	}

	*(video_g + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
	*(video_g + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;
	*(ptr + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
	*(ptr + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

	xpos++;
	if (xpos >= COLUMNS)
		goto newline;
       	spin_unlock_irqrestore(&putchar_lock, flags);	
}
static spinlock_t display_lock  = SPIN_LOCK_UNLOCKED;
void ut_printf (const char *format, ...)
{
	char **arg = (char **) &format;
	int c;
	unsigned long  val,flags;
	int i;
	char buf[40];

spin_lock_irqsave(&display_lock, flags);	
	va_list vl;
	va_start(vl,format);

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
	va_end(vl);
        spin_unlock_irqrestore(&display_lock, flags);	
}
