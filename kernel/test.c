#include "common.h"
#include "task.h"
addr_t g_before_i=0;
addr_t g_after_i=0;
addr_t g_error_i=0;
extern addr_t g_debug_level;
extern int g_test_exit;
void test_proc()
{
	long i;
	char c;
	addr_t a ;
	unsigned char d[1024];
	addr_t b ;
	int k,m;
	for (k=0; k<1024; k++) d[k]=0xcc;
	a=0xaaaaaaaaaaaaaaaa ;
	b=0xbbbbbbbbbbbbbbbb ;
	i=0;
	while(1)
	{
		i++;
		for (m=0; m<1024;m++)
			for (k=0; k<1024; k++)
			{
				g_before_i=k;
				if (d[k]!=0xcc)
				{
					g_error_i=1;
					ut_printf("  ERROR in :%x :%x :%x :%x \n",g_current_task,i,k,d[k]);
					ut_printf("  ERROR after :%x :%x : \n",g_before_i,g_after_i);
					while(1) ;
				}
			}
		//print_irq_stat();
	/*	if ((i%200)==0) {
			ut_printf("SCHEDULE after big loop ......................... \n");
			sc_schedule();
		} */
		if (g_test_exit==1)
		{
			ut_printf(" EXITING app :%d \n",g_current_task->pid);
			sc_exit();
		}

		if (g_debug_level ==1)
			ut_printf("Proc p:%d laddr:%x c:%x  sp:%x slen:%d loop:%d KEY\n",g_current_task->pid,g_current_task,&c,g_current_task->thread.sp,((void *)&c-(void*)g_current_task),i);
	}
}
