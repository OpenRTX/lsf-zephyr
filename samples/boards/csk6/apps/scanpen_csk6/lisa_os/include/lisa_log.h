#ifndef __LISA_RTT_LOG__
#define __LISA_RTT_LOG__

#include <zephyr.h>
#include <sys/printk.h>
#include "lisa_err.h"

#define LISA_LOG_LEVEL_NONE (0)
#define LISA_LOG_LEVEL_ERROR (1)
#define LISA_LOG_LEVEL_WARN (2)
#define LISA_LOG_LEVEL_INFO (3)
#define LISA_LOG_LEVEL_DEBUG (4)
#define LISA_LOG_LEVEL_VERBOSE (5)

#define log_level LISA_LOG_LEVEL_VERBOSE

#define LISA_LOGV(tag, format, ...)                                                                \
	do {                                                                                       \
		if (log_level >= LISA_LOG_LEVEL_VERBOSE) {                                              \
			printk("[V]][" tag "]" format "\n", ##__VA_ARGS__);                        \
		}                                                                                  \
	} while (0)

#define LISA_LOGD(tag, format, ...)                                                                \
	do {                                                                                       \
		if (log_level >= LISA_LOG_LEVEL_DEBUG) {                                                \
			printk("[D][" tag "]" format "\n", ##__VA_ARGS__);                         \
		}                                                                                  \
	} while (0)

#define LISA_LOGI(tag, format, ...)                                                                \
	do {                                                                                       \
		if (log_level >= LISA_LOG_LEVEL_INFO) {                                                 \
			printk("[I][" tag "]" format "\n", ##__VA_ARGS__);                         \
		}                                                                                  \
	} while (0)

#define LISA_LOGW(tag, format, ...)                                                                \
	do {                                                                                       \
		if (log_level >= LISA_LOG_LEVEL_WARN) {                                                 \
			printk("[W][" tag "]" format "\n", ##__VA_ARGS__);                         \
		}                                                                                  \
	} while (0)

#define LISA_LOGE(tag, format, ...)                                                                \
	do {                                                                                       \
		if (log_level >= LISA_LOG_LEVEL_ERROR) {                                                \
			printk("[E][" tag "]" format "\n", ##__VA_ARGS__);                         \
		}                                                                                  \
	} while (0)

#if 0
#define LISA_LOGD(tag, format, ...) printk("[D][" tag "]" format "\n", ##__VA_ARGS__)
#define LISA_LOGI(tag, format, ...) printk("[I][" tag "]" format "\n", ##__VA_ARGS__)
#define LISA_LOGW(tag, format, ...) printk("[W][" tag "]" format "\n", ##__VA_ARGS__)
#define LISA_LOGE(tag, format, ...) printk("[E][" tag "]" format "\n", ##__VA_ARGS__)
#endif

#endif // __LISA_RTT_LOG__
