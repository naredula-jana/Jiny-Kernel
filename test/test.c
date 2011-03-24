

unsigned long ut_printf (const char *format, ...);
unsigned char buf[2048];

main()
{
	void *fp,*wp;
	unsigned long ret;

	fp=open("/home/njana/test",0);
	wp=open("/home/njana/ooo",0);
	ut_printf(" Read file :%x outfile: %x \n",fp,wp);
	if (fp != 0)
	{
		ret=read(fp,buf,1024);	
		ut_printf(" Bytes read from file : %d ",ret);
		if (ret > 0)
		{
			write(wp,buf,ret);
		}
	}		
	close(fp);
	fdatasync(wp);
	close(wp);
}
