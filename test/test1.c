

unsigned long ut_printf (const char *format, ...);
unsigned char buf[2048];

main()
{
	void *fp,*wp;
	int i;
	unsigned long ret=0;
	i=1;
 	while (i<12)
	{
		i++;
		ut_printf("  loop countfrom test prog : %x \n",i);
	}
	ut_printf("Exiting from test code \n");
}
