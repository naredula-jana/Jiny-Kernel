/*
 * mem_test.c
 *
 *  Created on: Jun 1, 2015
 *      Author: enarere
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include<fcntl.h>
#include <signal.h>

#define MEM_SIZE (256*1024)
unsigned char mem[MEM_SIZE*1024];


unsigned long mtime() {
	struct timeval td;
	gettimeofday(&td, (struct timezone *) 0);
	return (td.tv_sec * 1000000 + ((double) td.tv_usec));
}

main(int argc, char *argv[]){
	int l,loops;
	int i;
	unsigned char *p;
	int k=0,m;
	unsigned long stime,etime;

	if (argc != 2) {
		printf("  argc :%d \n",argc);
		printf("./mem_test <> \n");
		return 1;
	}
	loops= atoi(argv[1]);


	stime = mtime();
	while (l < loops) {
		p = &mem[0];
		for (i = 0; i < MEM_SIZE-10; i = i + 1, p = p + 1024) {
			k = *p + k;
			//m=*p;
			//*p=m;
			*p=5+k;
		}
		l++;
	}
	etime = mtime();
	printf(" loops:%d time diff :%d memsize:%d M  read and  write\n",loops,(int)(etime-stime),((MEM_SIZE)/(1024)));
}


