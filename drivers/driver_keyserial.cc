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
#include "interface.h"
extern int dr_serialWrite( char *buf , int len);
}
#include "jdevice.h"

class keyboard_jdriver: public jdriver {
public:
	int probe_device(jdevice *dev);
	int attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);
	int print_stats();
};

int keyboard_jdriver::probe_device(class jdevice *jdev) {

	if (ut_strcmp(jdev->name , (unsigned char *)"/dev/keyboard") == 0){
		return JSUCCESS;
	}
	return JFAIL;
}
int keyboard_jdriver::attach_device(class jdevice *jdev) {
	return JSUCCESS;
}
int keyboard_jdriver::dettach_device(class jdevice *jdev) {
	return JSUCCESS;
}
int keyboard_jdriver::read(unsigned char *buf, int len){
	int ret;

	if (len >0 && buf!=0){
		ret = dr_kbGetchar(-1);
		if (ret != -1){
			buf[0]=ret;
			stat_recvs++;
			return 1;
		}
	}
	return 0;
}
int keyboard_jdriver::write(unsigned char *buf, int len){
	return 0;
}
int keyboard_jdriver::print_stats(){

	return JSUCCESS;
}
/*************************************************************************************/
class serial_jdriver: public jdriver {
public:
	int probe_device(jdevice *dev);
	int attach_device(jdevice *dev);
	int dettach_device(jdevice *dev);
	int read(unsigned char *buf, int len);
	int write(unsigned char *buf, int len);
	int print_stats();
};

int serial_jdriver::probe_device(class jdevice *jdev) {

	if (ut_strcmp(jdev->name , (unsigned char *)"/dev/serial") == 0){
		return JSUCCESS;
	}
	return JFAIL;
}
int serial_jdriver::attach_device(class jdevice *jdev) {
	return JSUCCESS;
}
int serial_jdriver::dettach_device(class jdevice *jdev) {
	return JSUCCESS;
}
int serial_jdriver::read(unsigned char *buf, int len){


	if (len >0 && buf!=0){

	}
	return 0;

}

int serial_jdriver::write(unsigned char *buf, int len){
	int ret;

	ret = dr_serialWrite((char *)buf,len);
	stat_sends = stat_sends+ret;
	return  ret;
}
int serial_jdriver::print_stats(){

	return JSUCCESS;
}
/*************************************************************************************************/
keyboard_jdriver keyboard_driver;
serial_jdriver serial_driver;

extern "C" {
void *vptr_keyboard[7]={(void *)&keyboard_jdriver::probe_device,(void *)&keyboard_jdriver::attach_device,(void *)&keyboard_jdriver::dettach_device,(void *)&keyboard_jdriver::read,(void *)&keyboard_jdriver::write,(void *)&keyboard_jdriver::print_stats,0};
void init_keyboard_jdriver() {
	void **p=(void **)&keyboard_driver;
	*p=&vptr_keyboard[0];

	keyboard_driver.name = (unsigned char *)"keyboard_driver";
	register_jdriver(&keyboard_driver);
}
void *vptr_serial[7]={(void *)&serial_jdriver::probe_device,(void *)&serial_jdriver::attach_device,(void *)&serial_jdriver::dettach_device,(void *)&serial_jdriver::read,(void *)&serial_jdriver::write,(void *)&serial_jdriver::print_stats,0};
void init_serial_jdriver() {
	void **p=(void **)&serial_driver;
	*p=&vptr_serial[0];

	serial_driver.name = (unsigned char *)"serial_driver";
	register_jdriver(&serial_driver);
}

}
