

unsigned long printf (const char *format, ...);
unsigned char buf[2048];
unsigned char stack[8000];
unsigned long *g;
child_main()
{
        int k;
          printf(" CHILD new version  STARTED  before xor  \n");
        for(k=0; k<5; k++)
        {
                printf(" CHILD New version loop:%d \n",k);
        }
        printf("before  CHILD Exiting from test code \n");
        exit(1);
        printf("after CHILD Exiting from test code \n");
}
main(int argc ,char *argv[])
{
	void *fp,*wp;
	int i;
	int pid;
	unsigned long stack_addr;
	unsigned long ret=0;
	i=1;

	printf("PARENT argc:%x   first  argv:%s  %x  argv:%x\n",argc,argv[0],argv[0],argv);
	printf("PARENT   argc:%x   second argv:%s  %x \n",argc,argv[1],argv[1]);

	stack_addr=&stack[0];
	stack_addr=stack_addr+4000;
printf(" FNN :%x stack:%x \n",child_main,stack_addr);
//pid=clone(child_main,0,1,0);
pid=fork();
if (pid ==0){
child_main();
printf(" CHILD FORK :\n");
}else{
	printf("clone FORK  return values :%d \n",pid);
}
	printf("after Exiting from test code:%d  \n",pid);
	exit(1);
}
/*__attribute__((destructor)) static void mydestructor(void) {
        printf("destructor\n");
} */

