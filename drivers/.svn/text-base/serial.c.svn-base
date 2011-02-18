#include "common.h"
//// io ports
#define DATA_BITS_8 3
#define STOP_BITS_1 0

#define DATA_REG(n) (n)
#define INT_ENABLE_REG(n) ((n) + 1)
#define FIFO_CTRL_REG(n) ((n) + 2)
#define DIVISOR_LO_REG(n) (n)
#define DIVISOR_HI_REG(n) ((n) + 1)
#define LINE_CTRL_REG(n) ((n) + 3)
#define MODEM_CTRL_REG(n) ((n) + 4)
#define LINE_STAT_REG(n) ((n) + 5)
#define MODEM_STAT_REG(n) ((n) + 6)
#define SCRATCH_REG(n) ((n) + 7)
#define SERIAL_PORT 0x3F8
void init_serial()
{
  int portno = SERIAL_PORT ;
  int reg;


  outb(INT_ENABLE_REG(portno), 0x00);              // Disable all interrupts
  outb(LINE_CTRL_REG(portno), 0x80);              // Enable DLAB (set baud rate divisor)

  outb(DIVISOR_LO_REG(portno), 1 & 0xff);    // Set divisor to (lo byte)
  outb(DIVISOR_HI_REG(portno), 1 >> 8);      //                (hi byte)

  // calculate line control register
  reg =
    (DATA_BITS_8 & 3) |
    (STOP_BITS_1 ? 4 : 0) |
    (0 ? 8 : 0) |
    (0 ? 16 : 0) |
    (0 ? 32 : 0) |
    (0 ? 64 : 0);

  outb(LINE_CTRL_REG(portno), reg);

  outb(FIFO_CTRL_REG(portno), 0x47);             // Enable FIFO, clear them, with 4-byte threshold 
  outb(MODEM_CTRL_REG(portno), 0x0B);             // No modem support
  outb(INT_ENABLE_REG(portno), 0x01);             // Issue an interrupt when input buffer is full.
}
static spinlock_t serial_lock  = SPIN_LOCK_UNLOCKED;
int dr_serialWrite( char *buf , int len)
{
	int i;
        unsigned long  flags;

	spin_lock_irqsave(&serial_lock, flags);
	for (i=0; i<len; i++)
	{
		outb(DATA_REG(SERIAL_PORT), buf[i]);
	}
	spin_unlock_irqrestore(&serial_lock, flags);
}


