#include <stdio.h>

unsigned char buf[2048];

main(int argc , char **argv)
{
	int wp,i;
	unsigned long ret;

	wp=open(argv[1],1);

	printf("LATEST  NEW  outfile: %x :% \n",wp);
	for (i=0; i<1000000000; i++){
	if (wp != 0)
	{
          strcpy(buf,"test12");
		write(wp,buf,3);
	}		
	fdatasync(wp);
	}
	close(wp);
}
