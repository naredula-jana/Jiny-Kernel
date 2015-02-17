/* prodcons2.c */

/*
 * A solution to the producer consumer using semaphores
 *
 * This solution to the producer consumer problem illustrates the
 * use of shared memory, semaphores and "forked" processes.
 */

#include <stdio.h>
#include <sys/wait.h>
#include "memory.h"


#define N     10    /* size of the buffer                           */

#define MUTEX 2     /* use the first semaphore in the set for Mutex */
#define EMPTY 4     /* next semaphore will be empty                 */
#define FULL  6     /* and the one after that is for full           */

#include <sys/time.h>
#include <stdio.h>

#include <errno.h>
#include <semaphore.h>  /* Semaphore */


int b; /* buffer size = 1; */

unsigned char stack[4001];
int out_max, in_max;
void *consumer();
void *producer();
#define LOOP_SIZE 200000
void cpu_eat(int count) {
	unsigned long loop;
	int k;
	loop = 1;
	for (k = 0; k < count; k++) {
		loop = loop + 3;
	}
}
sem_t prod_sem,cons_sem;
void main(int argc, char *argv[]) {
	int rc, i;
	pthread_t t[2];

	if (argc != 3) {
		printf("./sem <out_loop> <in_loop> \n");
		return 1;
	}
	out_max = atoi(argv[1]);
	in_max = atoi(argv[2]);
	if (in_max == 0) {
		in_max = LOOP_SIZE;
	}

	 sem_init(&prod_sem, 0, 0);
	 sem_init(&cons_sem, 0, 1);
	clone((void *) &consumer, &stack[4000], 9000, 0, 0);
	producer();
}

void add_buffer(int i) {
	cpu_eat(in_max);
	b = i;
}
int get_buffer() {
	cpu_eat(in_max);
	return b;
}
int exit_done = 0;
void *producer() {
	int i = 0;
	printf("producer starting\n");
	while (1) {
		sem_post(&prod_sem);
		add_buffer(i);
		i = i + 1;
		if (exit_done > 0) {
			break;
		}
		sem_wait(&cons_sem);
		cpu_eat(in_max);
	}
	printf(" Producer completed .. : %d \n", i);
	fflush(stdout);
	while (1) {
		if (exit_done == 1) {
			exit_done = 2;
			return;
		}
	}
	/* wait till consumer is exited */
}
void *consumer() {
	int i, v;
	int max_loop = out_max;
	printf("consumer started\n");
	for (i = 0; i < max_loop; i++) {
		 sem_wait(&prod_sem);
		v = get_buffer();
		sem_post(&cons_sem);
	}

	printf(" Consumer light completed.. : %d \n", max_loop);
	fflush(stdout);
	exit_done = 1;
	sem_post(&cons_sem);
	while (1) {
		if (exit_done == 2) {
			return;
		}
	}
	return;
}
