/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/**
 * Should be included from sof/schedule/task.h
 * but triggers include chain issue
 * FIXME
 */
int sof_main(int argc, char *argv[]);

int __wrap_printf(const char * fmt, ...)
{
	static char buf[4*1024] = { 0 };
	va_list ap;

	va_start(ap, fmt);
	uint32_t n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if(n > 0) LOG_INF("[luna_err]%s", buf);

	return n;
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
	return memset(k_malloc(nmemb * size), 0, nmemb * size);
}

void * __wrap_malloc(int size)
{
	return k_malloc(size);
}

void __wrap_free(void *ptr)
{
    k_free(ptr);
}

/**
 * TODO: Here comes SOF initialization
 */

void main(void)
{
	int ret;

	LOG_INF("SOF on %s", CONFIG_BOARD);

	/* sof_main is actually SOF initialization */
	ret = sof_main(0, NULL);
	if (ret) {
		LOG_ERR("SOF initialization failed");
	}

	LOG_INF("SOF initialized");
}
