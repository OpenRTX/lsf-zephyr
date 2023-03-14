/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "venus_ap.h"

#include <string.h>

#include <zephyr/drivers/onewire/infrared/infrared_csk6.h>
#include <zephyr/init.h>

#include <logging/log.h>

#define DT_DRV_COMPAT listenai_csk_ir

LOG_MODULE_REGISTER(csk6_infrared, CONFIG_LOG_DEFAULT_LEVEL);

/* CSK6 infrared driver */

//     int (*init)(struct device *dev, ir_config_t *config);
//     int (*send)(struct device *dev, ir_data_t *data);
//     int (*recv)(struct device *dev, ir_data_t *data);
//     int (*set_mode)(struct device *dev, enum ir_protoclo_mode mode);
//     int (*set_freq)(struct device *dev, uint32_t freq);

static const struct infrared_driver_api {
	.init = ir_csk_init,
	.send = ir_csk_send,
	.recv = ir_csk_recv,
	.set_mode = ir_csk_set_mode,
	.set_freq = ir_csk_set_freq,
};


#define IR_DEVICE_INIT_CSK(n)                                                                      \
	static void ir_csk_config_func_##n(const struct device *dev);                              \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
                                                                                                   \
	static const struct ir_csk_config ir_csk_config_##n = {                                    \
		.base = (ir_RegDef *)DT_INST_REG_ADDR(n),                                          \
		.irq_config_func = ir_csk_config_func_##n,                                         \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                         \
	};                                                                                         \
                                                                                                   \
	K_EVENT_DEFINE(csk6_ir_event_##n);                                                         \
	static struct ir_csk_data ir_csk_data_##n = {.evt = &csk6_ir_event_##n};                   \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &ir_csk_init, NULL, &ir_csk_data_##n, &ir_csk_config_##n,         \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                     \
			      &ir_csk_driver_api);                                                 \
                                                                                                   \
	static void ir_csk_config_func_##n(const struct device *dev)                               \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), ir_csk_isr,                 \
			    DEVICE_DT_INST_GET(n), 0);                                             \
                                                                                                   \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}

DT_INST_FOREACH_STATUS_OKAY(IR_DEVICE_INIT_CSK)
