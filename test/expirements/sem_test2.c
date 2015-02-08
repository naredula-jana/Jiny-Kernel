/**
  @file producer_consumer.c

  @info A sample program to demonstrate the classic consumer/producer problem
  using pthreads.
*/

#include <stdio.h>
#include <pthread.h>
int out_max,in_max;
#define MAX 10                                  /* maximum iterations */

int prod_number=0;
int cons_number=0;
/* the resource */

/* Mutex to protect the resource. */
pthread_mutex_t mu= PTHREAD_MUTEX_INITIALIZER;  /* to protect the resource*/

/*
  Condition variable to signal consumer that a new number is available for
  consumption.
*/
pthread_cond_t sig_consumer= PTHREAD_COND_INITIALIZER;
/*
  Condition variable to signal the producer that
  (a) the new number has been consumed,
  (b) generate another one.
*/
pthread_cond_t sig_producer= PTHREAD_COND_INITIALIZER;
#define LOOP_SIZE 200000
void cpu_eat(){
	  unsigned long loop;
	  int k;
	  loop=1;
	    for (k=0; k<in_max; k++){
loop = loop+3;
	    }
}
long com_counter=10;
/**
  @func consumer
  This function is responsible for consuming (printing) the incremented
  number and signalling the producer.
*/
void *consumer(void *dummy)
{
  int printed= 0;
  int consumes=0;

  printf("Consumer : \"Hello I am consumer #%ld. Ready to consume numbers"
         " now\"\n", pthread_self());

  while (1)
  {
    pthread_mutex_lock(&mu);
    cpu_eat();
    consumes++;
    if (cons_number< prod_number){
    	cons_number++;
    	com_counter = com_counter - 2;
    }
    pthread_mutex_unlock(&mu);

    /*
      If the MAX number was the last consumed number, the consumer should
      stop.
    */
    if (cons_number == out_max)
    {
      printf("Consumer done!  :%d  consumes:%d  cons_number:%d cc:%d\n",out_max,consumes,cons_number,com_counter);
      break;
    }
  }
}

/**
  @func producer
  This function is responsible for incrementing the number and signalling the
  consumer.
*/
void *producer(void *dummy)
{
  printf("Producer : \"Hello I am producer #%ld. Ready to produce numbers"
         " now\"\n", pthread_self());
int number =0;
  while (1)
  {

    pthread_mutex_lock(&mu);
    if (cons_number == prod_number){
    	prod_number++;
    	com_counter = com_counter +2;
    }
    number ++;
cpu_eat();
    pthread_mutex_unlock(&mu);

    /* Stop if MAX has been produced. */
    if (prod_number == out_max)
    {
      printf("Producer done.. :%d  produced:%d  prod_number:%d cc:%d\n",out_max,number,prod_number,com_counter);
      break;
    }
  }
}
unsigned char stack[9000];
void main(int argc, char *argv[] )
{
  int rc, i;
  pthread_t t[2];


	if (argc != 3) {
		printf("./sem <out_loop> <in_loop> \n");
		return 1;
	}
	out_max = atoi(argv[1]);
	in_max = atoi(argv[2]);
	if (in_max ==0){
		in_max = LOOP_SIZE;
	}


#if 0
  /* Create consumer & producer threads. */
  if ((rc= pthread_create(&t[0], NULL, consumer, NULL)))
    printf("Error creating the consumer thread..\n");
  if ((rc= pthread_create(&t[1], NULL, producer, NULL)))
    printf("Error creating the producer thread..\n");
  /* Wait for consumer/producer to exit. */
  for (i= 0; i < 2; i ++)
    pthread_join(t[i], NULL);
#else
  clone((void *) &consumer, &stack[4000], 9000, 0, 0);
  producer(0);
#endif
  printf("Done..\n");
}
