/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   include/libc_highpriority.h
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 */
#define socket SYS_socket
#define sendto SYS_sendto
#define gettimeofday SYS_gettimeofday
#define bind SYS_bind
#define recvfrom SYS_recvfrom
#define printf ut_printf
#define close SYS_fs_close
#define write SYS_fs_write
#define read SYS_fs_read
#define open SYS_fs_open
#define exit SYS_sc_exit
static void init_module(unsigned char *arg1, unsigned char *arg2){
	ut_printf(" Starting the module\n");
	create_thread(file_test);
}
static void clean_module(){
	ut_printf(" Stopping the module \n");
}

extern void Jcmd_lsmod(unsigned char *arg1, unsigned char *arg2);
