#include <sched.h>

unsigned long printf (const char *format, ...);
unsigned char buf[2048];
unsigned char stack[8000];
unsigned long *g;
child_main()
{
        int k;
          printf(" CHILD new version  STARTED  before xor  \n");
		  exit(200);
//__asm__ __volatile__("pxor   %xmm2,%xmm2");
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
	unsigned long stack_addr;
	unsigned long ret=0;
	i=1;

	printf("PARENT argc:%x   first  argv:%s  %x  argv:%x\n",argc,argv[0],argv[0],argv);
	printf("PARENT   argc:%x   second argv:%s  %x \n",argc,argv[1],argv[1]);

	stack_addr=&stack[0];
	stack_addr=stack_addr+4000;
printf(" FNN :%x stack:%x  clone flag :%x\n",child_main,stack_addr,CLONE_VM);
clone(child_main,stack_addr,1|CLONE_VM,0);
	printf("Newbefore Exiting from test code \n");
	exit(1);
	printf("after Exiting from test code \n");
}
/*__attribute__((destructor)) static void mydestructor(void) {
        printf("destructor\n");
} */

