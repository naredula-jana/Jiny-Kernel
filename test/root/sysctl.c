#include <stdio.h>
#include <linux/sysctl.h>

unsigned char buf[10001];
#define MAX_TOKENS 10

main(int argc, char *argv[]) {
	int name[MAX_TOKENS];
	struct __sysctl_args args;
	int j, k, start;
	k = 0;
	start = 0;

	//if (argc <2){
		//printf(" ERROR : sysctl set/cmd <> \n");
	//}
	for (j=0; j<3; j++)
		name[j]=0;
if (argc > 1)
	name[0]=argv[1];
if (argc > 2)
	name[1]=argv[2];
if (argc > 3)
	name[2]=argv[3];
	name[3]=0;

	args.name = &name[0];

	args.nlen = k;
	int ret = sysctl(&args);
	return 1;
}


