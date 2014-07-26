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
#define SERIAL_PORT1 0x3F8  /* com1 */
#define SERIAL_PORT2 0x2F8  /* com2 */

static int serial_input_handler1(void *unused_private_data) {
	unsigned char c;
	c = inb(DATA_REG(SERIAL_PORT1));
	ar_addInputKey(DEVICE_SERIAL1, c);
	//ut_printf(" Received the char from serial %x %c \n",c,c);
	outb(INT_ENABLE_REG(SERIAL_PORT1), 0x01); // Issue an interrupt when input buffer is full.
	return 0;
}
static int serial_input_handler2(void *unused_private_data) {
	unsigned char c;
	c = inb(DATA_REG(SERIAL_PORT2));
	ar_addInputKey(DEVICE_SERIAL2, c);
	//ut_printf(" Received the char from serial %x %c \n",c,c);
	outb(INT_ENABLE_REG(SERIAL_PORT1), 0x01); // Issue an interrupt when input buffer is full.
	return 0;
}
int init_serial(unsigned long unsed_arg) {
	int portno = SERIAL_PORT1;
	int reg;
	int i;

	for (i = 0; i < 2; i++) {
		if (i==1) portno = SERIAL_PORT2;
		outb(INT_ENABLE_REG(portno), 0x00); // Disable all interrupts
		outb(LINE_CTRL_REG(portno), 0x80); // Enable DLAB (set baud rate divisor)

		outb(DIVISOR_LO_REG(portno), 1 & 0xff); // Set divisor to (lo byte)
		outb(DIVISOR_HI_REG(portno), 1 >> 8); //                (hi byte)

		// calculate line control register
		reg = (DATA_BITS_8 & 3) | (STOP_BITS_1 ? 4 : 0) | (0 ? 8 : 0) | (0 ? 16 : 0) | (0 ? 32 : 0) | (0 ? 64 : 0);

		outb(LINE_CTRL_REG(portno), reg);

		outb(FIFO_CTRL_REG(portno), 0x47); // Enable FIFO, clear them, with 4-byte threshold
		outb(MODEM_CTRL_REG(portno), 0x0B); // No modem support
		outb(INT_ENABLE_REG(portno), 0x01); // Issue an interrupt when input buffer is full.
		if (i==0){
			ar_registerInterrupt(36, serial_input_handler1, "serial-1", NULL);
		}else{
			ar_registerInterrupt(35, serial_input_handler2, "serial-2", NULL);
		}
	}
	return JSUCCESS;
}
static spinlock_t serial_lock = SPIN_LOCK_UNLOCKED((unsigned char *)"serial");
static int dr_serialWrite(char *buf, int len) {
	int i;
	unsigned long flags;

	spin_lock_irqsave(&serial_lock, flags);
	for (i = 0; i < len; i++) {
		if (buf[i] >= 0x20 || buf[i] == 0xa || buf[i] == 0xd || buf[i] == 9
				|| buf[i] == 0x1b){
			outb(DATA_REG(SERIAL_PORT1), buf[i]);
			outb(DATA_REG(SERIAL_PORT2), buf[i]); /* port-2 can used for logging of the port-1 */
		}else {
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
	int read(unsigned char *buf, int len, int flags);
	int write(unsigned char *buf, int len, int flags);
	int print_stats();
	int ioctl(unsigned long arg1, unsigned long arg2);
	int serial_device_no;
};
static serial_jdriver *serial_driver;
int serial_jdriver::probe_device(class jdevice *jdev) {

	if (ut_strcmp(jdev->name, (unsigned char *) "/dev/serial1") == 0  || ut_strcmp(jdev->name, (unsigned char *) "/dev/serial2") == 0) {
		return JSUCCESS;
	}
	return JFAIL;
}
jdriver *serial_jdriver::attach_device(class jdevice *jdev) {
	if (ut_strcmp(jdev->name, (unsigned char *) "/dev/serial1") == 0){
		serial_driver->serial_device_no = DEVICE_SERIAL1;
	}else{
		serial_driver->serial_device_no = DEVICE_SERIAL2;
	}
	COPY_OBJ(serial_jdriver, serial_driver, new_obj, jdev);

	return (jdriver *) new_obj;
}
int serial_jdriver::dettach_device(class jdevice *jdev) {
	return JSUCCESS;
}
int serial_jdriver::read(unsigned char *buff, int len, int read_flags) {
	int ret = 0;

	if (len > 0 && buff != 0) {
		buff[0] = dr_kbGetchar(serial_device_no);
		stat_recvs++;
		ret = 1;

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

int serial_jdriver::write(unsigned char *buf, int len, int wr_flags) {
	int i;
	int ret = 0;
	for (i = 0; i < len; i++) {
		//ut_putchar_ondevice(buf[i], serial_device_no);

		char temp_buf[5];
		if (buf[i] == '\n') {
			temp_buf[0] = '\r';
			temp_buf[1] = '\0';
			dr_serialWrite(temp_buf, 1);
			temp_buf[0] = '\n';
			temp_buf[1] = '\0';
			dr_serialWrite(temp_buf, 1);
		} else {
			temp_buf[0] = (char) buf[i];
			temp_buf[1] = '\0';
			dr_serialWrite(temp_buf, 1);
		}
		ret++;
	}
	stat_sends = stat_sends + ret;
	return ret;
}
int serial_jdriver::print_stats() {
	ut_printf(" sends:%d recvs :%d", stat_sends, stat_recvs);
	return JSUCCESS;
}
int serial_jdriver::ioctl(unsigned long arg1, unsigned long arg2) {
	if (arg1 == 0) {
		return serial_device_no;
	}
	return serial_device_no;
}
/*************************************************************************************************/

extern "C" {

void init_serial_jdriver() {

	serial_driver = jnew_obj(serial_jdriver);
	serial_driver->name = (unsigned char *) "serial_driver";
	register_jdriver(serial_driver);
}

}
