#include <stdio.h>
       #include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>
unsigned char buf[2048];

main(int argc, char **argv) {
	int fp, wp, j;
	unsigned long ret;

	fp = open(argv[1], O_CREAT);
    j=atoi(argv[2]);
printf(" append :%x  wronly :%x creat:%x \n",O_APPEND,O_WRONLY,O_CREAT);
	if (fp > 0) {
		int i;
		for (i = 0; i < j; i++)
			if (1) {
				snprintf(buf,10,"\n%d:testabcdefgaaaaa",i);
				write(fp, buf, 10);
			}
	}
fdatasync(fp);
	close(fp);

}
