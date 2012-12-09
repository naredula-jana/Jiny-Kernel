#include "os_dep.h"
 void *code_region_start=(void *)0x0000000040000000;
 void *data_region_start=&end; //TODO: need to cross the value
 void *data_region_end=&g_idle_tasks;
 int sysctl_memleak_scan=0;
