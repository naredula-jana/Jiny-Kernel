/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   driver_keyserial.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/

extern "C" {
#include "common.h"
#include "mach_dep.h"
#include "interface.h"
extern int dr_serialWrite(char *buf, int len);

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

static int serial_input_handler(void *unused_private_data) {
	unsigned char c;
	c = inb(DATA_REG(SERIAL_PORT));
	ar_addInputKey(DEVICE_SERIAL, c);
	//ut_printf(" Received the char from serial %x %c \n",c,c);
	outb(INT_ENABLE_REG(SERIAL_PORT), 0x01); // Issue an interrupt when input buffer is full.
	return 0;
}
int init_serial(unsigned long unsed_arg) {
	int portno = SERIAL_PORT;
	int reg;

	outb(INT_ENABLE_REG(portno), 0x00); // Disable all interrupts
	outb(LINE_CTRL_REG(portno), 0x80); // Enable DLAB (set baud rate divisor)

	outb(DIVISOR_LO_REG(portno), 1 & 0xff); // Set divisor to (lo byte)
	outb(DIVISOR_HI_REG(portno), 1 >> 8); //                (hi byte)

	// calculate line control register
	reg = (DATA_BITS_8 & 3) | (STOP_BITS_1 ? 4 : 0) | (0 ? 8 : 0) | (0 ? 16 : 0)
			| (0 ? 32 : 0) | (0 ? 64 : 0);

	outb(LINE_CTRL_REG(portno), reg);

	outb(FIFO_CTRL_REG(portno), 0x47); // Enable FIFO, clear them, with 4-byte threshold
	outb(MODEM_CTRL_REG(portno), 0x0B); // No modem support
	outb(INT_ENABLE_REG(portno), 0x01); // Issue an interrupt when input buffer is full.
	ar_registerInterrupt(36, serial_input_handler, "serial", NULL);
	return JSUCCESS;
}
static spinlock_t serial_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"serial");
int dr_serialWrite(char *buf, int len) {
	int i;
	unsigned long flags;

	spin_lock_irqsave(&serial_lock, flags);
	for (i = 0; i < len; i++) {
		if (buf[i] >= 0x20 || buf[i] == 0xa || buf[i] == 0xd || buf[i] == 9
				|| buf[i] == 0x1b)
			outb(DATA_REG(SERIAL_PORT), buf[i]);
		else {
			//ut_log(" Special character eaten Up :%x: \n",buf[i]);
		}
	}
	spin_unlock_irqrestore(&serial_lock, flags);
}
}
#include "jdevice.h"

/*************************************************************************************/
class serial_jdriver: public jdriver {
public:
	int probe_device(jdevice *dev);
	jdriver *attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);
	int print_stats();
	int ioctl(unsigned long arg1, unsigned long arg2);
};
serial_jdriver serial_driver;
int serial_jdriver::probe_device(class jdevice *jdev) {

	if (ut_strcmp(jdev->name, (unsigned char *) "/dev/serial_in") == 0
			|| ut_strcmp(jdev->name, (unsigned char *) "/dev/serial_out")
					== 0) {
		return JSUCCESS;
	}
	return JFAIL;
}
jdriver *serial_jdriver::attach_device(class jdevice *jdev) {
	COPY_OBJ(serial_jdriver, &serial_driver, new_obj, jdev);
	return (jdriver *) new_obj;
}
int serial_jdriver::dettach_device(class jdevice *jdev) {
	return JSUCCESS;
}
int serial_jdriver::read(unsigned char *buff, int len) {
	int ret = 0;

	if (len > 0 && buff != 0) {
		buff[0] = dr_kbGetchar(DEVICE_SERIAL);
		stat_recvs++;
		ret = 1;
#if 0
		if (buff[0] == CTRL_D || buff[0] == CTRL_C) {
			ut_log(" RECVIED special character :%x thread name :%s:\n", buff[0],
					g_current_task->name);
		}
#endif

		if (buff[0] == CTRL_D || buff[0] == CTRL_C) {/* for CTRL-D */
			return 0;
		}
		if (buff[0] == 0xd)
			buff[0] = '\n';
		buff[1] = '\0';
		ut_printf("%s", buff);
	}
	return ret;
}

int serial_jdriver::write(unsigned char *buf, int len) {
	int i;
	int ret = 0;
	for (i = 0; i < len; i++) {
		ut_putchar_ondevice(buf[i], DEVICE_SERIAL);
		ret++;
	}
	stat_sends = stat_sends + ret;
	return ret;
}
int serial_jdriver::print_stats() {
	ut_printf(" sends:%d recvs:%d", stat_sends, stat_recvs);
	return JSUCCESS;
}
int serial_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	if (arg1 == 0) {
		return DEVICE_SERIAL;
	}
	return DEVICE_SERIAL;
}
/*************************************************************************************************/

extern "C" {

static void *vptr_serial[8] = { (void *) &serial_jdriver::probe_device,
		(void *) &serial_jdriver::attach_device,
		(void *) &serial_jdriver::dettach_device,
		(void *) &serial_jdriver::read, (void *) &serial_jdriver::write,
		(void *) &serial_jdriver::print_stats, (void *) &serial_jdriver::ioctl,
		0 };
void init_serial_jdriver() {
	void **p = (void **) &serial_driver;
	*p = &vptr_serial[0];

	serial_driver.name = (unsigned char *) "serial_driver";
	register_jdriver(&serial_driver);
}

}
