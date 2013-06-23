#include "os_dep.h"
 void *code_region_start=(void *)0x0000000040000000;
 //extern void *tcp_state_str;
 extern void *device_list;
 void *data_region_start=0x40200000;
 void *data_region_end=&end; //TODO hardcode to get the end of data
 int sysctl_memleak_scan=0;
