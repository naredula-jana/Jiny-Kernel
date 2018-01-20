
typedef struct {
	unsigned long tv_sec;
	unsigned long tv_usec;
}time_t;

unsigned long g_system_times_ns __attribute__ ((section ("vsysdata"))) ;
unsigned long g_system_start_time __attribute__ ((section ("vsysdata"))) ;
unsigned long vsys_sec __attribute__ ((section ("vsysdata")))  =0x123;

unsigned long VSYS_gettimeofday(time_t *tv, struct timezone *unused_arg_tz)  {
	if (tv != 0){
		tv->tv_sec = (g_system_times_ns / 1000000000) + g_system_start_time;
		tv->tv_usec = (g_system_times_ns / 1000) % 1000000;
	}
	return 0;
}

unsigned long __attribute__((aligned(1024)))  VSYS_time(unsigned long *time) {
	if (time != 0){
		*time= (g_system_times_ns / 1000000000) + g_system_start_time;
		return *time;
	}
	return 0;
}

unsigned long __attribute__((aligned(1024)))  VSYS_getcpu() { /* TODO: not implemented */
	return 0;
}
