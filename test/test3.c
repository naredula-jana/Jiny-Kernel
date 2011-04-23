

unsigned long printf (const char *format, ...);
unsigned char buf[2048];
unsigned char stack[8000];
unsigned long *g;
child_main()
{
        int k;
          printf(" CHILD new version  STARTED  \n");
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
	stack_addr=stack_addr+7000;
clone(child_main,stack_addr,1,0);
 	while (i<2)
	{
		clone(child_main,stack_addr,1,0);
		printf("NEW   SYSCALLs loop count: %x stackaddr:%x \n",i,&ret);
		i++;
	}
//	while(1) ;
	printf("Before CRASH\n");
	g=0x40115f00;
	ret=*g; 
	printf("before Exiting from test code \n");
	exit(1);
	printf("after Exiting from test code \n");
}
/*__attribute__((destructor)) static void mydestructor(void) {
        printf("destructor\n");
} */

