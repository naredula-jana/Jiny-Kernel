

int test1=4;
int test2;
static dummy_cal(){
    test1=22;
}
void init_module(){
    ut_printf(" starting th test module :%d \n",test1);
test1++;
}
void clean_module(){
    ut_printf(" starting th test module\n");
}
