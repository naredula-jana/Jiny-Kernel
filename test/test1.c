

unsigned long printf (const char *format, ...);
unsigned char buf[2048];

main()
{
	int fp,wp;
	unsigned long ret;

	fp=open("input",0);
	wp=open("output",1);
	printf(" NEW Read file :%x outfile: %x \n",fp,wp);
	if (fp != 0)
	{
		ret=read(fp,buf,1024);	
		printf(" Bytes read from file : %d \n ",ret);
		if (ret > 0)
		{
			write(wp,buf,ret);
		}
	}		
	close(fp);
	fdatasync(wp);
	close(wp);
}
