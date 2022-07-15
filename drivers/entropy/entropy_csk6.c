/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_entropy

#include <string.h>
#include <drivers/entropy.h>
#include "Driver_TRNG.h"
	
#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_entropy);

#ifndef typeof
#define typeof  __typeof__
#endif

#ifndef typecheck
#define typecheck(type,x) \
({	\
	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})
#endif

#define time_after(a,b) \
	(typecheck(unsigned long, a) && \
	typecheck(unsigned long, b) && \
	((long)(b) - (long)(a) < 0))

#define DEV_DATA(dev) ((struct entropy_csk6_data * const)(dev)->data)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))

struct entropy_csk6_data {
    void *handle; 
	struct k_sem sem_lock;
};

static inline bool _ready(const struct device *dev)
{
	return (1 == HAL_TRNG_GetDataReady(DEV_HANDLER(dev))) ? true : false;
}

static int csk6_get_trng(const struct device *dev, uint8_t *buf,
				     uint16_t len)
{
	unsigned long time_cur,time_after;

	/* 这里的超时时间设置需依赖trng硬件特性：  
	 * 目前配置参数为“CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11”
	 * 冷启动随机数获取时间为80ms, 热启动随机数获取时间为2ms
	 * 这里的超时时间设置为100ms是合理的
	 */
	uint32_t timeout = 100;

	while (len > 0) {
		time_after = k_uptime_get_32() + timeout;
		while(!_ready(dev)){
			time_cur = k_uptime_get_32();
			if(time_after(time_cur, time_after)){
				LOG_WRN("trng time out! \n");
				return -ETIMEDOUT;
			}
		};

		uint32_t word = HAL_TRNG_GetData(DEV_HANDLER(dev));

		uint32_t to_copy = MIN(sizeof(word), len);

		memcpy(buf, &word, to_copy);
		buf += to_copy;
		len -= to_copy;
	}

	return 0;
}

static int entropy_csk6_get_entropy(const struct device *dev, uint8_t *buffer,
				     uint16_t length)
{
	__ASSERT_NO_MSG(buffer != NULL);

	struct entropy_csk6_data *data = DEV_DATA(dev);
	int ret;

	k_sem_take(&data->sem_lock, K_FOREVER);
	ret = csk6_get_trng(dev, buffer, length);
	k_sem_give(&data->sem_lock);

	return ret;
}

static int entropy_csk6_get_entropy_isr(const struct device *dev,
				       uint8_t *buffer,
				       uint16_t length, uint32_t flags)
{
	__ASSERT_NO_MSG(buffer != NULL);

	int ret;
	unsigned int key;
	uint16_t cnt = length;

	if ((flags & ENTROPY_BUSYWAIT) == ENTROPY_BUSYWAIT) {

		key = irq_lock();
		ret = csk6_get_trng(dev, buffer, length);
		irq_unlock(key);

		if(ret < 0){
			return ret;
		}

		return cnt;

	} else {
		//not support non-blocking mode
		return 0;
	}
}

static int entropy_csk6_init(const struct device *dev)
{
    struct entropy_csk6_data *data = DEV_DATA(dev);
    data->handle = TRNG();

    HAL_TRNG_Initialize(DEV_HANDLER(dev));
    HAL_TRNG_PowerControl(DEV_HANDLER(dev), CSK_POWER_FULL);
    HAL_TRNG_Control(DEV_HANDLER(dev), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);

	HAL_TRNG_Enable(DEV_HANDLER(dev));

	/* Locking semaphore initialized to 1 (unlocked) */
	k_sem_init(&data->sem_lock, 1, 1);

	return 0;
}

static const struct entropy_driver_api entropy_csk6_api_funcs = {
	.get_entropy = entropy_csk6_get_entropy,
	.get_entropy_isr = entropy_csk6_get_entropy_isr
};

static struct entropy_csk6_data entropy_csk6_data;

DEVICE_DT_INST_DEFINE(0, entropy_csk6_init,
		      NULL,
		      &entropy_csk6_data,
		      NULL, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &entropy_csk6_api_funcs);