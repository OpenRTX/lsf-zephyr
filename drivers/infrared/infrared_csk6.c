/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_ir
// #include "venus_ap.h"

// #include <string.h>
#include "hal/Driver_IR.h"
#include "hal/ir.h"

#include <drivers/infrared.h>
// #include <zephyr/init.h>

#include <logging/log.h>
#include <zephyr/drivers/pinctrl.h>


LOG_MODULE_REGISTER(csk6_infrared, CONFIG_LOG_DEFAULT_LEVEL);

#define DEV_CFG(dev)	 ((const struct ir_csk_config *const)(dev)->config)
#define DEV_DATA(dev)	 ((struct ir_csk_data *const)(dev)->data)
#define DEV_BASE(dev)	 ((IR_RegDef *)(DEV_CFG(dev))->base)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))

struct ir_csk_config {
	IR_RegDef *base;
	void (*irq_config_func)(const struct device *dev);
	const struct pinctrl_dev_config *pcfg;
};

struct ir_csk_data {
	void *handle;
	struct k_event *evt;
	uint8_t protocol;
};

static void *ir_csk_get_context(const struct device *dev)
{
	IR_RegDef *base = DEV_BASE(dev);
	if (base == IP_IR) {
		return IR0();
	} else {
		LOG_ERR("ir reg base[%p] err, default: IR0", base);
		return NULL;
	}
}

/* CSK6 infrared driver */

static int ir_csk_runtime_configure(struct device *dev, ir_config_t *config)
{
	
	return 0;
}

static int ir_csk_send(struct device *dev, ir_data_t *data)
{
	void *IR0_Handler = DEV_HANDLER(dev);
	IR_HW_Send(IR0_Handler, IR_PROTOCOL_NEC, data->addr, data->cmd);
	return 0;
}

static int ir_csk_recv(struct device *dev, ir_data_t *data)
{
	void *IR0_Handler = DEV_HANDLER(dev);
	IR_HW_Receive(IR0_Handler, IR_PROTOCOL_NEC, &data->addr, &data->cmd);
	return 0;
}

static void ir_csk_event_cb(uint32_t event, void *user_arg)
{
	// struct i2c_csk_data *data = (struct i2c_csk_data *)user_arg;
	// k_event_post(data->evt, event);
	return;
}

static int ir_csk_init(const struct device *dev)
{
	const struct ir_csk_config *config = DEV_CFG(dev);
	struct ir_csk_data *data = DEV_DATA(dev);

	data->handle = ir_csk_get_context(dev);
	
	void *IR0_Handler = DEV_HANDLER(dev);

	IR_Initialize(IR0_Handler, ir_csk_event_cb, NULL);
	IR_PowerControl(IR0_Handler, CSK_POWER_FULL);

	pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);

	return 0;
}

static void ir_csk_isr(const struct device *dev)
{
	extern void IR_IRQ_Handler(IR_RESOURCES * ir);

	void *IR_Handler = DEV_HANDLER(dev);
	IR_IRQ_Handler(IR_Handler);
}

static const struct infrared_driver_api ir_csk_driver_api = {
	.configure = ir_csk_runtime_configure,
	.send = ir_csk_send,
	.recv = ir_csk_recv,
	.recv_timeout = NULL,
};


#define IR_DEVICE_INIT_CSK(n)                                                                      \
	static void ir_csk_config_func_##n(const struct device *dev);                              \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
                                                                                                   \
	static const struct ir_csk_config ir_csk_config_##n = {                                    \
		.base = (IR_RegDef *)DT_INST_REG_ADDR(n),                                          \
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
