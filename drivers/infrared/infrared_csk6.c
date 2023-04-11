/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_ir

#include "hal/Driver_IR.h"
#include "hal/ir.h"

#include <drivers/infrared.h>

#include <logging/log.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr.h>


LOG_MODULE_REGISTER(csk6_infrared, CONFIG_LOG_DEFAULT_LEVEL);

typedef struct ir_csk_config {
	IR_RegDef *base;
	void (*irq_config_func)(const struct device *dev);
	const struct pinctrl_dev_config *pcfg;
} ir_csk_config_t;

typedef struct ir_csk_data {
	void *handle;
	struct k_event *evt;
	ir_config_t cfg;
} ir_csk_data_t;

#define DEV_CFG(dev)	 ((ir_csk_config_t *const)(dev)->config)
#define DEV_DATA(dev)	 ((ir_csk_data_t *const)(dev)->data)
#define DEV_BASE(dev)	 ((IR_RegDef *)(DEV_CFG(dev))->base)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))

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

static int ir_csk_runtime_configure(const struct device *dev, ir_config_t *config)
{
	ir_csk_data_t *dev_data = DEV_DATA(dev);
	if (config->protocol != IR_PROTOCOL_CUSTOM) {
		dev_data->cfg.protocol = config->protocol;
	} else {
		LOG_ERR("CSK6 IR module currently does not support custom infrared protocol.");
		return -IR_ERR_INVALID_PARAM;
	}
	return 0;
}

static int ir_csk_send(const struct device *dev, ir_data_t *data)
{
	void *IR0_Handler = DEV_HANDLER(dev);
	ir_csk_data_t *dev_data = DEV_DATA(dev);
	if (dev_data->cfg.protocol != IR_PROTOCOL_CUSTOM) {
		uint8_t addr_hbyte = data->addr >> 8;
		uint8_t addr_lbyte = ~(data->addr & 0xff);
		if (addr_hbyte != addr_lbyte) {
			LOG_WRN("The address code and its complement must be equal, please check and re-enter.");
			return -IR_ERR_INVALID_PARAM;
		}
		uint8_t cmd_hbyte = data->cmd >> 8;
		uint8_t cmd_lbyte = ~(data->cmd & 0xff);
		if (cmd_hbyte != cmd_lbyte) {
			LOG_WRN("The command code and its complement must be equal, please check and re-enter.");
			return -IR_ERR_INVALID_PARAM;
		}
	}
	return IR_HW_Send(IR0_Handler, dev_data->cfg.protocol, data->addr, data->cmd);
}

static int ir_csk_recv_timeout(const struct device *dev, ir_data_t *data, k_timeout_t timeout)
{
	int ret = 0;
	uint32_t evt = 0;
	void *IR0_Handler = DEV_HANDLER(dev);
	ir_csk_data_t *dev_data = DEV_DATA(dev);

	ret = IR_HW_Receive(IR0_Handler, dev_data->cfg.protocol, &data->addr, &data->cmd);
	evt = k_event_wait(dev_data->evt, CSK_IR_EVENT_HW_RECEIVE_COMPLETE | CSK_IR_EVENT_HW_RX_REPEAT_TRIGGER,
				true, timeout);

	if (evt & CSK_IR_EVENT_HW_RECEIVE_COMPLETE) {
		// return IR_EVENT_HW_RECEIVE_COMPLETE;
		return 0;
	} else if (evt & CSK_IR_EVENT_HW_RX_REPEAT_TRIGGER) {
		IR_Control(IR0_Handler, CSK_IR_CONTROL_CLEAR_RX_FIFO, 0);
		// return IR_EVENT_HW_RX_REPEAT_TRIGGER;
		return 0;
	} else {
		IR_Control(IR0_Handler, CSK_IR_CONTROL_CLEAR_RX_FIFO, 0);
		return -IR_ERR_TIMEOUT;
	}

	return ret;
}

static int ir_csk_recv(const struct device *dev, ir_data_t *data)
{
	return ir_csk_recv_timeout(dev, data, K_FOREVER);
}

static void ir_csk_event_cb(uint32_t event, void *user_arg)
{
	ir_csk_data_t *data = (ir_csk_data_t *)user_arg;
	k_event_post(data->evt, event);
	return;
}

static void ir_csk_isr(const struct device *dev)
{
	void *IR_Handler = DEV_HANDLER(dev);

	extern void IR_IRQ_Handler(IR_RESOURCES * ir);
	IR_IRQ_Handler(IR_Handler);
}

static int ir_csk_init(const struct device *dev)
{
	const struct ir_csk_config *config = DEV_CFG(dev);
	struct ir_csk_data *dev_data = DEV_DATA(dev);

	dev_data->handle = ir_csk_get_context(dev);

	void *IR0_Handler = DEV_HANDLER(dev);
	dev_data->cfg.protocol = IR_PROTOCOL_DEFAULT;//default protocol: NEC
	pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);

	IR_Initialize(IR0_Handler, ir_csk_event_cb, dev_data);
	IR_PowerControl(IR0_Handler, CSK_POWER_FULL);

	return 0;
}

static const struct infrared_driver_api ir_csk_driver_api = {
	.configure = ir_csk_runtime_configure,
	.send = ir_csk_send,
	.recv = ir_csk_recv,
	.recv_timeout = ir_csk_recv_timeout,
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
