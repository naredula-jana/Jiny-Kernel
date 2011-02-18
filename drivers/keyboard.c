#include "keyboard.h"
#include "common.h"
#include "task.h"
char codes[128] = {
        /*0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F*/  
  /*0*/   0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=','\b', '\t',
  /*1*/ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  10,   0, 'a', 's',
  /*2*/ 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';','\'', '`',   0, '\\', 'z', 'x', 'c', 'v',
  /*3*/ 'b', 'n', 'm', ',', '.', '/',   0,   0,   0, ' ',   0,   0,   0,   0,   0,   0,
  /*4*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /*5*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /*6*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /*7*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

char s_codes[128] = {
        /*0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F*/  
  /*0*/   0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+','\b', '\t',
  /*1*/ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',  10,   0, 'A', 'S',
  /*2*/ 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, '|', 'Z', 'X', 'C', 'V',
  /*3*/ 'B', 'N', 'M', '<', '>', '?',   0,   0,   0, ' ',   0,   0,   0,   0,   0,   0,
  /*4*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /*5*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /*6*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /*7*/   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static int en_scan_to_ascii(unsigned char *buf, uint8_t scancode, uint8_t comb_flags, uint8_t state_flags)
{

	if(codes[scancode]) {
		*buf = (comb_flags & SHIFT_MASK) ? s_codes[scancode] : codes[scancode];
		/*  if(state_flags & CAPS_MASK && isalpha(*buf)) {
		    if(isupper(*buf)) *buf = tolower(*buf);
		    else *buf = toupper(*buf);
		    } */
		return 1;
	}
	return 0;
}
#define MAX_BUF 100
static unsigned char kb_buffer[MAX_BUF+1];
static int current_pos=0;
static int read_pos=0;
struct task_struct *waiting_task=0;
int keyboard_int=0;
void dr_keyBoardBH() /* bottom half */
{
	//keyboard_int=0;
	if (waiting_task != 0)
	{
		sc_wakeUpProcess(waiting_task);
		waiting_task=0;
	}
}
unsigned char dr_kbGetchar()
{
	unsigned char c;

	//if (waiting_task !=0) return 0;
	while(current_pos ==0) 
	{
		waiting_task=g_current_task;
	/*	current_task->state=TASK_INTERRUPTIBLE;
		sc_schedule(); */
		sc_sleep(1000); 
	}
	if (read_pos < current_pos)
	{
		c=kb_buffer[read_pos];
		read_pos++;
		if (read_pos == current_pos) 
		{
			current_pos=0;
			read_pos=0;
		}
		//printf(" got key :%x:\n",c);
		return c;
	} 
	return 0;
}

static void keyboard_handler(registers_t regs)
{
	unsigned char control_kbd,LastKey;
	int ret;

	LastKey = inb(0x60);
//	printf(" Key pressed : %x curr_pos:%x read:%x\n",LastKey,current_pos,read_pos);
	if (current_pos < MAX_BUF && LastKey<128)
	{
		ret=en_scan_to_ascii(&kb_buffer[current_pos],LastKey,0,0);
		//     printf(" character :%c: ret:%d \n",kb_buffer[current_pos],ret);
		if (ret == 1) current_pos++;
	}
	/* Tell the keyboard hat the key is processed */
	control_kbd = inb(0x61);
	outb(0x61, control_kbd | 0x80);  /* set the "enable kbd" bit */
	outb(0x61, control_kbd);  /* write back the original control value */
	keyboard_int=1;
	dr_keyBoardBH();

}
int init_driver_keyboard()
{
	ar_registerInterrupt(33,keyboard_handler);
	return 1;
}
