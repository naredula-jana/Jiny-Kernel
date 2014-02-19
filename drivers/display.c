/* kernel.c - the C part of the kernel */
#include <stdarg.h>
#include "interface.h"
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


/* Variables.  */
/* Save the X position.  */
static int xpos;
/* Save the Y position.  */
static int ypos;
/* Point to the video memory.  */
volatile unsigned char *g_video_ram = 0;
static unsigned char video_buffer_g[LINES][2 * COLUMNS];
/* Forward declarations.  */
void cls(void);
static void itoa(char *buf, int buf_len, int base, unsigned long d);
void ut_printf(const char *format, ...);

extern void *g_print_lock;
/* Clear the screen and initialize VIDEO, XPOS and YPOS.  */
int ut_cls(void) {
	int i;
	unsigned char *ptr;
	ptr = &(video_buffer_g[0][0]);

	if (g_video_ram==0) return 0;
	for (i = 0; i < COLUMNS * LINES * 2; i++) {
		*(g_video_ram + i) = 0;
		*(ptr + i) = 0;
	}

	xpos = 0;
	ypos = 0;
	return 1;
}
static void refresh_screen(void) {
	int i;
	unsigned char *ptr;
	ptr = &(video_buffer_g[0][0]);
	if (g_video_ram==0) return;

	for (i = 1; i < COLUMNS * LINES * 2; i = i + 2) {
		*(g_video_ram + i) = 0x4f;
		*(ptr + i) = 0x4f;
	}

}
/* Convert the integer D to a string and save the string in BUF. If
 BASE is equal to 'd', interpret that D is decimal, and if BASE is
 equal to 'x', interpret that D is hexadecimal.  */
//#define DIVISOR 16
static void itoa(char *buf, int buf_len, int base, unsigned long d) {
	char *p = buf;
	char *p1, *p2;
	unsigned long ud = d;
	int len = buf_len;
	int divisor = 10;

	/* If %d is specified and D is minus, put `-' in the head.  */
	if (base == 'd' && d < 0) {
		*p++ = '-';
		buf++;
		ud = -d;
	} else if (base == 'x')
		divisor = 16;

	/* Divide UD by DIVISOR until UD == 0.  */
	do {
		int remainder = ud % divisor;
		len--;
		if (len < 1) {
			while (1)
				;
		}
		*p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
		if (divisor != 16 && divisor != 10) /* TODO : test purpose */
		{
			cli();
			while (1)
				;
		}
	} while (ud /= divisor);

	/* Terminate BUF.  */
	*p = 0;

	/* Reverse BUF.  */
	p1 = buf;
	p2 = p - 1;
	while (p1 < p2) {
		char tmp = *p1;
		*p1 = *p2;
		*p2 = tmp;
		p1++;
		p2--;
	}
}
static void scroll() {
	int i;
	unsigned char *ptr;

	ptr = &(video_buffer_g[0][0]);
	if (g_video_ram == 0) return;
	for (i = 0; i < COLUMNS * (LINES - 1) * 2; i++) /* copy the data from next line to prev line */
	{
		*(g_video_ram + i) = ptr[i + 2 * COLUMNS];
		*(ptr + i) = ptr[i + 2 * COLUMNS];
	}
	for (i = COLUMNS * (LINES - 1) * 2; i < COLUMNS * (LINES) * 2; i++) /* clear the last line */
	{
		*(g_video_ram + i) = 0;
		*(ptr + i) = 0;
	}
	refresh_screen();
}

unsigned char g_dmesg[MAX_DMESG_LOG+2];
unsigned long g_dmesg_index = 0;

extern int dr_serialWrite( char *buf , int len);
/* Put the character C on the screen.  */
static spinlock_t putchar_lock = SPIN_LOCK_UNLOCKED("putchar");

static int init_log_file_done=0;
static struct file *log_fp;
static int dmesg_file_index=0;

int init_log_file(unsigned long unused){
	int ret=0;

	if (init_log_file_done == 0) {
		log_fp = (struct file *) fs_open((unsigned char *) "jiny.log", O_APPEND, O_RDWR);
		if (log_fp == 0)
			return 0;
		init_log_file_done = 1;
		ret=fs_write(log_fp, g_dmesg, g_dmesg_index);
		dmesg_file_index = g_dmesg_index;
	}
	return ret;
}
int Jcmd_logflush(unsigned char *arg1) {
	int ret = 0;
	if (init_log_file_done == 1) {
		int len = g_dmesg_index - dmesg_file_index; // TODO : need to take wrap arounds where len becomes negative

		if (len > 0) {
			ret = fs_write(log_fp, &g_dmesg[dmesg_file_index], len);
			dmesg_file_index = g_dmesg_index;
		}
		ut_log("flushed to log :%d  ret:%d len:%d\n", dmesg_file_index, ret, len);
	} else {
		ut_log("Failed to flush to log\n");
	}
	return 1;
}
static void log_putchar(unsigned char c){
	unsigned long flags;

	spin_lock_irqsave(&putchar_lock, flags);
	//if (g_boot_completed) mutexLock(g_print_lock);

	g_dmesg[g_dmesg_index % MAX_DMESG_LOG] = (unsigned char) c;
	g_dmesg[(g_dmesg_index+1) % MAX_DMESG_LOG]=0;
	g_dmesg_index++;

	//if (g_boot_completed) mutexUnLock(g_print_lock);
	spin_unlock_irqrestore(&putchar_lock, flags);
}
int Jcmd_dmesg(unsigned char *arg1){
	ut_printf("Size/Max : %d / %d \n",g_dmesg_index,MAX_DMESG_LOG);
	g_dmesg[MAX_DMESG_LOG]=0;
	ut_printf("%s\n",g_dmesg);
	return 0;
}

void ut_putchar_ondevice(unsigned char c, int device) {
	unsigned char *ptr;
	unsigned long flags;

	//1. Serial output
	if (device == DEVICE_SERIAL){
		char buf[5];
		if (c == '\n') {
			buf[0] = '\r';
			buf[1] = '\0';
			dr_serialWrite(buf, 1);
			buf[0] = '\n';
			buf[1] = '\0';
			dr_serialWrite(buf, 1);
		} else {
			buf[0] = (char) c;
			buf[1] = '\0';
			dr_serialWrite(buf, 1);
		}
		return;
	}

	//2. VGI output
	//log_putchar(c);
	if (g_video_ram == 0) return;

	spin_lock_irqsave(&putchar_lock, flags);
//	if (g_boot_completed) mutexLock(g_print_lock);
	ptr = &(video_buffer_g[0][0]);
	if (c == '\n' || c == '\r') {
		newline: xpos = 0;
		ypos++;
		if (ypos >= LINES) {
			if (1) {
				scroll();
				ypos--;
			} else {
				ypos = 0;
			}
		}
		//if (g_boot_completed) mutexUnLock(g_print_lock);
		spin_unlock_irqrestore(&putchar_lock, flags);
		return;
	}

	*(g_video_ram + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
	*(g_video_ram + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;
	*(ptr + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
	*(ptr + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

	xpos++;
	if (xpos >= COLUMNS)
		goto newline;

//	if (g_boot_completed) mutexUnLock(g_print_lock);
	spin_unlock_irqrestore(&putchar_lock, flags);
}

void ut_putchar(unsigned char c) {
	return ut_putchar_ondevice(c,DEVICE_SERIAL);
}

struct writer_struct{
	int type;
	unsigned char *buf;
	int len;
};
static int str_writer(struct writer_struct *writer,unsigned char c){
	if (writer->type==1){
		log_putchar(c);
		//if (is_kernel_thread)
		//	ut_putchar(c);
		return 1;
	}else if (writer->type==0){
	    ut_putchar(c);
		return 1;
	}else if (writer->buf != 0 && (writer->len > 0)){
		writer->buf[0]=c;
		writer->len--;
		writer->buf++;
		return 1;
	}
	return 0;
}
static void format_string(struct writer_struct *writer, const char *format, va_list vl) {
	char **arg = (char **) &format;
	int c;
	unsigned long val;
	int i;
	char buf[180];
	char *p;

	arg++;
	i = 0;
	while ((c = *format++) != 0) {
		int number_len=0;
		if (c != '%'){
			if (str_writer(writer,c) ==0) return;
		}
		else {
			c = *format++;
			if (c>'0' && c<='9'){
				number_len=(c-'0')*2;
				c = *format++;
			}
			switch (c) {
			case 'd':
			case 'u':
			case 'x':
			case 'i':
				if (c == 'i') {
					val = va_arg(vl,unsigned int);
					c = 'x';
				} else
					val = va_arg(vl,long);

				itoa(buf, 39, c, val);

				i++;
				p = buf;
				goto string;
				break;
			case 's':
				val = va_arg(vl,long);
				p = (char *)val;
				if (!p)
					p = "(null)";

	string:
	            if (number_len > 0) {
					int num_len;
					int end_str=0;

					for (num_len = 0; num_len < number_len && num_len<80; num_len++) {
						if (p[num_len] == '\0' || end_str==1) {
							buf[num_len] = ' ';
							buf[num_len + 1] = '\0';
							end_str=1;
						}else{
							buf[num_len] = p[num_len];
						}
					}
					p=buf;
				}
/* TODO: need to check the validity of virtual address, otherwise it will generate page fault with lock */
				while (ar_check_valid_address(p,1)==JSUCCESS  && *p){
					if (str_writer(writer, *p) ==0) return;
					p++;
				}
	            if (ar_check_valid_address(p,1)==JFAIL){
	            	BUG();
	            	ut_log("Invalid Address tp display : %x \n",p);
	            }
				break;

			default:
				if (str_writer(writer,*arg) ==0) return;
				arg++;
				break;
			}
		}
	}
}

void ut_printf(const char *format, ...) {
	struct writer_struct writer;
	unsigned long flags;
	int locked=0;
	// Cannot be used because idle threads may issue print from isrs, due to this it is best effort to lock in a non-interrupt context
	if (g_boot_completed && g_cpu_state[getcpuid()].intr_nested_level==0 && g_current_task->locks_nonsleepable==0) {
	//	mutexLock(g_print_lock);
		locked=1;
	}
	va_list vl;
	va_start(vl,format);

	writer.type=0;
	format_string(&writer,format,vl);

	if (locked ==1){
		//mutexUnLock(g_print_lock);
	}
}


void ut_log(const char *format, ...){
	struct writer_struct writer;
	unsigned long flags;
	char buf[80];
//	if (g_boot_completed) mutexLock(g_print_lock);

	va_list vl;
	va_start(vl,format);

#if 1
	writer.type=1;
	ut_snprintf(buf,35,"%4d:",g_jiffies);  // print timestamp before line
	format_string(&writer,buf,0);

#endif

	writer.type=1;
	format_string(&writer,format,vl);

//	if (g_boot_completed) mutexUnLock(g_print_lock);
}
