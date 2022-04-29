#include <random/rand32.h>
#include <lisa_time.h>
#include <lisa_log.h>

#define TAG "lisa_time"
uint32_t lisa_os_get_ticks(void)
{
	/*
	  Returns the elapsed time since the system booted, 
	  if want get the current system tick ,use sys_clock_tick_get_32();
	*/
	return k_uptime_get();
}

uint32_t lisa_os_set_ticks(uint32_t tick)
{
	__ASSERT(0, "%s is not supportd", __FUNCTION__);
	return 0;
}

uint32_t lisa_os_get_time(void)
{
	return (k_uptime_get() / 1000);
}

uint32_t lisa_rand32(void)
{
	return sys_rand32_get();
}