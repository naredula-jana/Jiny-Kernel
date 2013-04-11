#include <stdio.h>
#include <time.h>
main() {
	int i, j;
	char *p;
	struct timespec req;

	for (i = 0; i < 1000; i++) {
		req.tv_sec = 1;
		nanosleep(&req, 0);

		for (j = 0; j < 1024; j++) {
			p = malloc(4 * 1024);
			p[5] = 'c';
		}
		printf(" This is New after the sleep :%d  adress:%x\n", i,p);
	}
}
