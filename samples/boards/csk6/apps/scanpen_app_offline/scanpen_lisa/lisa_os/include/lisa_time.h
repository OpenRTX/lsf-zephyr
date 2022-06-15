#ifndef __LISA_OS_TIME__
#define __LISA_OS_TIME__

#include <zephyr.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <stdint.h>

#define LISA_WAIT_FOREVER 0xffffffffU /* Wait forever timeout value */
#define LISA_NO_WAIT 0 /* wait 0 second timeout value */

uint64_t lisa_os_get_ticks(void);
uint64_t lisa_os_set_ticks(uint64_t tick);

// get second
uint64_t lisa_os_get_time(void);
uint32_t lisa_rand32(void);

#endif //__LISA_OS_TIME__