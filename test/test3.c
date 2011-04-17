

unsigned long ut_printf (const char *format, ...);
unsigned char buf[2048];
unsigned char stack[8000];
unsigned long *g;
void child_main();
main(int argc ,char *argv[])
{
	void *fp,*wp;
	int i;
	unsigned long stack_addr;
	unsigned long ret=0;
	i=1;

	ut_printf("PARENT argc:%x   first  argv:%s  %x  argv:%x\n",argc,argv[0],argv[0],argv);
	ut_printf("PARENT   argc:%x   second argv:%s  %x \n",argc,argv[1],argv[1]);

	stack_addr=&stack[0];
	stack_addr=stack_addr+7000;
clone(child_main,stack_addr,1,0);
 	while (i<5)
	{
		ut_printf("NEW   SYSCALLs loop count: %x stackaddr:%x \n",i,&ret);
		i++;
	}
/*	g=0x40115f00;
	ret=*g; */
	ut_printf("before Exiting from test code \n");
	exit(1);
	ut_printf("after Exiting from test code \n");
}
child_main()
{
        int k;
        for(k=0; k<5; k++)
        {
                ut_printf(" CHILD loop:%d \n",k);
        }
        ut_printf("before  CHILD Exiting from test code \n");
        exit(1);
        ut_printf("after CHILD Exiting from test code \n");
}
