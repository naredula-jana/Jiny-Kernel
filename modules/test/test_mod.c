

int test_abc=230;
int test2;
char a[100][20];
extern void ut_printf(const char *format, ...);
static dummy_cal(){
	test_abc=22;
}
void init_module(){
	ut_printf(" Init: %d \n",test_abc);
    ut_printf("Large memory  starting th test module :%d \n",test_abc);
    test_abc++;
}
void clean_module(){
    ut_printf(" clean test module\n");
}
