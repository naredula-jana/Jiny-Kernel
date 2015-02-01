/**
  @file producer_consumer.c

  @info A sample program to demonstrate the classic consumer/producer problem
  using pthreads.
*/

#include <stdio.h>
#include <pthread.h>

#define MAX 10                                  /* maximum iterations */

int number;                                     /* the resource */

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

/**
  @func consumer
  This function is responsible for consuming (printing) the incremented
  number and signalling the producer.
*/
void *consumer(void *dummy)
{
  int printed= 0;

  printf("Consumer : \"Hello I am consumer #%ld. Ready to consume numbers"
         " now\"\n", pthread_self());

  while (1)
  {
    pthread_mutex_lock(&mu);
    /* Signal the producer that the consumer is ready. */
    pthread_cond_signal(&sig_producer);
    /* Wait for a new number. */
    pthread_cond_wait(&sig_consumer, &mu);
    /* Consume (print) the number. */
    printf("Consumer : %d\n", number);
    /* Unlock the mutex. */
    pthread_mutex_unlock(&mu);

    /*
      If the MAX number was the last consumed number, the consumer should
      stop.
    */
    if (number == MAX)
    {
      printf("Consumer done.. !!\n");
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

  while (1)
  {
    pthread_mutex_lock(&mu);
    number ++;
    printf("Producer : %d\n", number);
    /*
      Signal the consumer that a new number has been generated for its
      consumption.
    */
    pthread_cond_signal(&sig_consumer);
    /*
      Now wait for consumer to confirm. Note, expect no confirmation for
      consumption of MAX from consumer.
    */
    if (number != MAX)
      pthread_cond_wait(&sig_producer, &mu);

    /* Unlock the mutex. */
    pthread_mutex_unlock(&mu);

    /* Stop if MAX has been produced. */
    if (number == MAX)
    {
      printf("Producer done.. !!\n");
      break;
    }
  }
}

void main()
{
  int rc, i;
  pthread_t t[2];

  number= 0;

  /* Create consumer & producer threads. */
  if ((rc= pthread_create(&t[0], NULL, consumer, NULL)))
    printf("Error creating the consumer thread..\n");
  if ((rc= pthread_create(&t[1], NULL, producer, NULL)))
    printf("Error creating the producer thread..\n");

  /* Wait for consumer/producer to exit. */
  for (i= 0; i < 2; i ++)
    pthread_join(t[i], NULL);

  printf("Done..\n");
}
