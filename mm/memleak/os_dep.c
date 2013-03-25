#include "os_dep.h"
 void *code_region_start=(void *)0x0000000040000000;
 extern void *VIDEO,*device_list;
 void *data_region_start=&VIDEO; //TODO: need to check the value
 void *data_region_end=&device_list; //TODO hardcode to get the end of data
 int sysctl_memleak_scan=0;
