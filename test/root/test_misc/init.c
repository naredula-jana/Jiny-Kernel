#include <stdio.h>
#include <linux/sysctl.h>

unsigned char buf[10001];
#define MAX_TOKENS 10
int execute_cmd(char *p){
	int name[MAX_TOKENS];
	struct __sysctl_args args;
	int j,k,start;
	k = 0;
	start = 0;

	if (p[0]=='#') return 1;
	for (j=0; j<MAX_TOKENS; j++){
		name[j]=0;
	}
	for (j = 0;  j < 1024 && k < MAX_TOKENS; j++) {
		if (p[j] == ' ' || p[j] == '\n' || p[j] == '\0') {

			name[k] = p+start;
			k++;
			if (p[j] == '\0'){
				break;
			}

			p[j] = '\0';

			start = j + 1;
		}
	}

	args.name=&name[0];
	args.nlen = k;
	int ret=sysctl(&args);
    printf("Ret sysctl :%d args:%x name0:%x name1:%x\n",ret,&args,name[0],name[1]);
    return 1;
}
/*
 * file format:
 * set syscall_debug 1
 * cmd
 *
 */
int read_config(){
    int fp;
    unsigned long ret;
    int i;

    fp=open("jiny.conf",0);

    if (fp != 0)
    {
        ret=read(fp,buf,1024);
        printf("New Bytes read from jiny.conf file : %d \n ",ret);
        if (ret > 0)
        {
        	int start=0;
        	for (i=0; i<ret; i++){
        		if (buf[i]=='\n' || buf[i]=='\r'){
        			buf[i]='\0';
        			execute_cmd(&buf[start]);
        			start=i+1;
        		}
        	}
        }
    }
    close(fp);
    return 1;
}

main(){
read_config();

}


