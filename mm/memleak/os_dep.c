#include "os_dep.h"
 void *code_region_start=0x0000000040000000;
 void *data_region_start=&placement_address;
 void *data_region_end=&g_idle_tasks;
 int sysctl_memleak_scan=0;
