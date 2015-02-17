

#include <sys/time.h>
  #include <stdio.h>
  #include <pthread.h>
  #include <errno.h>

  pthread_mutex_t produce_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t consume_mutex = PTHREAD_MUTEX_INITIALIZER;

  int b;  /* buffer size = 1; */
/*
 main()
 {
   pthread_t producer_thread;
   pthread_t consumer_thread;
   void *producer();
   void *consumer();

     pthread_mutex_lock(&consume_mutex);
     pthread_create(&consumer_thread,NULL,consumer,NULL);
     pthread_create(&producer_thread,NULL,producer,NULL);
     pthread_join(consumer_thread,NULL);
 }
*/
 unsigned char stack[4001];
 int out_max,in_max;
 void *consumer();
 void *producer();
#define LOOP_SIZE 200000
void cpu_eat(int count){
	  unsigned long loop;
	  int k;
	  loop=1;
	    for (k=0; k<count; k++){
	    	loop = loop+3;
	    }
}
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
 	 clone((void *) &consumer, &stack[4000], 9000, 0, 0);
 	  producer();
 }

 void add_buffer(int i){
	 cpu_eat(in_max);
   b = i;
 }
 int get_buffer(){
	 cpu_eat(in_max);
   return b ;
 }
int exit_done=0;
 void *producer()
 {
 int i = 0;
 printf("I'm a producer\n");
 while (1) {
   pthread_mutex_lock(&produce_mutex);
   add_buffer(i);
   pthread_mutex_unlock(&consume_mutex);
   i = i + 1;
   if (exit_done > 0){
	   break;
   }
 }
 printf(" Producer completed : %d \n",i);
 fflush(stdout);
 while(1){
	 if (exit_done==1){
		 exit_done=2;
		 return;
	 }
 }
 /* wait till consumer is exited */
 }
 void *consumer()
 {
 int i,v;
 int max_loop=out_max;
 printf("I'm a consumer\n");
 for (i=0;i<max_loop;i++) {
    pthread_mutex_lock(&consume_mutex);
    v = get_buffer();
    pthread_mutex_unlock(&produce_mutex);
    //printf("got %d  ",v);
 }
 pthread_mutex_unlock(&produce_mutex);
 printf(" Consumer completed : %d \n",max_loop);
 fflush(stdout);
 exit_done=1;
 while(1){
	 if (exit_done==2){
		 return;
	 }
	 pthread_mutex_unlock(&produce_mutex);
 }
 pthread_exit(NULL);
 }
