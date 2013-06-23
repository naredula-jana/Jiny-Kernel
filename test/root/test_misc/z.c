#include <stdio.h>
#include <time.h>
main() {
	int i, j;
	char *p;
	struct timespec req;

while(1)
	for (i = 0; i < 10000000; i++) {
		req.tv_sec = 0;
        req.tv_nsec =0;
	//	nanosleep(&req, 0);

		for (j = 0; j < 1024; j++) {
	//	    nanosleep(&req, 0);
			p = malloc(4 * 1024);
			p[5] = 'c';
			free(p);
		}
	//	printf("  New after the sleep :%d  adress:%x\n", i,p);
	}
}
