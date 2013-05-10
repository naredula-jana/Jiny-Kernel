#include <stdio.h>
unsigned char buf[2048];
#define FADV_DONTNEED 4
main(int argc , char **argv)
{
	int fp,wp,j;
	unsigned long ret;
//    printf("PARENT argc:%x   first  argv[0]: %s argv[1]:%s  argv:%x\n",argc,argv[0],argv[1],argv);
    for (j = 0; j < 100000; j++) {
		if (argv[1] == 0)
			fp = open("input", 0);
		else
			fp = open(argv[1], 0);

		//wp=open("output",1);
//		printf("%d NEW Read file :%x outfile: %x \n", j,fp, wp);
		if (fp > 0) {
			int i;
			for (i = 0; i < 10; i++)
				ret = read(fp, buf, 1024);
		    //	printf(" Bytes read from file : %d ", ret);
#if 0
			if (ret > 0)
			{
				write(wp,buf,ret);
			}
#endif
		}
		posix_fadvise64(fp, 0, 0, FADV_DONTNEED);
		close(fp);
	}

//	fdatasync(wp);
	//close(wp);
}
