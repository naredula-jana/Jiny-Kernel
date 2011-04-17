

unsigned long ut_printf (const char *format, ...);
unsigned char buf[2048];
unsigned long *g;
main(int argc ,char *argv[])
{
	void *fp,*wp;
	int i;
	unsigned long ret=0;
	i=1;
	ut_printf(" address argc: %x argv:%x \n",&argc,argv);
	ut_printf(" first arg%x second arg:%x \n",argv[0],argv[1]);
	ut_printf(" argc:%x   first  argv:%s  %x  argv:%x\n",argc,argv[0],argv[0],argv);
	ut_printf(" argc:%x   second argv:%s  %x \n",argc,argv[1],argv[1]);
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
