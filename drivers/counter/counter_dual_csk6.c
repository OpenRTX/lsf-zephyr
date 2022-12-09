/*
 * Copyright (c) 2022 .
 */

#define DT_DRV_COMPAT listenai_csk_counter

#include "hal/Driver_DUAL_TIMER.h"
#include "hal/dual_timer.h"

#include <drivers/counter.h>
#define DEV_CFG(dev)  ((struct counter_csk_config *)(dev)->config)
#define DEV_DATA(dev) ((struct counter_csk_data *)(dev)->data)

// TIMER_RESOURCES TIMER_RESOURCES_1
struct counter_csk_config {
	struct counter_config_info counter_info;
	void (*irq_config)(void);
	TIMER_RESOURCES TIME_RESOURCES;
};
struct counter_csk_data {
	struct counter_alarm_cfg alarm_cfg;
	uint32_t ticks;
	uint8_t channel;
	void *handle;
};

static void *timer_csk_get_ctx(const struct device *dev)
{
	DUAL_TIMER_RegDef *base = DEV_CFG(dev)->TIME_RESOURCES.reg;
	if (base == VENUS_TIMER0) {
		return TIMER0();
	} else {
		return NULL;
	}
}

static uint32_t csk_get_value(const struct device *dev)
{
	uint32_t now;
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;

	TIMER_ReadTimerCount(DUAL_Hander, channel, &now);

	return now;
}

static void timer_csk_isr(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	extern void TIMER_IRQ_Handler(TIMER_RESOURCES * timer);
	TIMER_RESOURCES *DUAL_Hander = data->handle;
	
	uint8_t channel = data->channel;
	uint32_t now = csk_get_value(dev);
	uint32_t ret0, ret1;
	uint32_t event = 0;
	ret0 = DUAL_Hander->reg->REG_MIS0.all;
	ret1 = DUAL_Hander->reg->REG_MIS1.all;

	if (ret0) {
		// clear interrupt status
		DUAL_Hander->reg->REG_INTCLR0.all = CSK_TIMER_CLR_INT;
		event = CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH0;
		if (data->alarm_cfg.callback != NULL) {
			data->alarm_cfg.callback(dev, channel, now, data->alarm_cfg.user_data);
		}
		while (DUAL_Hander->reg->REG_MIS0.all)
			;
	}

	event = 0;

	if (ret1) {
		DUAL_Hander->reg->REG_INTCLR1.all = CSK_TIMER_CLR_INT;
		event = CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH1;
		if (data->alarm_cfg.callback != NULL) {
			data->alarm_cfg.callback(dev, channel, now, data->alarm_cfg.user_data);
		}
		while (DUAL_Hander->reg->REG_MIS1.all)
			;
	}
}

static void timer_csk_irq_config(void)
{
	IRQ_CONNECT(IRQ_TIMER_VECTOR, 2, timer_csk_isr, DEVICE_DT_GET(DT_NODELABEL(dultim)), 0);
	irq_enable(IRQ_TIMER_VECTOR);
}

static int counter_csk_init(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	struct counter_csk_config *cfg = DEV_CFG(dev);
	/* Get device handle */
	data->handle = timer_csk_get_ctx(dev); // TIMER0();//
	void *DUAL_Hander = data->handle;

	// Setting irq
	TIMER_Initialize(DUAL_Hander);
	TIMER_PowerControl(DUAL_Hander, CSK_POWER_FULL);
	cfg->irq_config();

	return 0;
}

static int counter_csk_start(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;

	TIMER_StartTimer(DUAL_Hander, channel);
	return 0;
}

static int counter_csk_stop(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;
	TIMER_StopTimer(DUAL_Hander, channel);
	return 0;
}

static int counter_csk_get_value(const struct device *dev, uint32_t *ticks)
{
	// *ticks = csk_get_value(dev);
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;
	TIMER_ReadTimerCount(DUAL_Hander, channel, ticks);
	return 0;
}

static int counter_csk_set_alarm(const struct device *dev, uint8_t chan_id,
				 const struct counter_alarm_cfg *alarm_cfg)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	data->channel = chan_id;

	uint32_t alarm_tick = alarm_cfg->ticks;
	data->alarm_cfg.callback = alarm_cfg->callback;
	data->alarm_cfg.user_data = alarm_cfg->user_data;

	TIMER_Control(DUAL_Hander,
		      CSK_TIMER_PRESCALE_Divide_1 | CSK_TIMER_SIZE_32Bit | CSK_TIMER_MODE_Periodic |
			      CSK_TIMER_INTERRUPT_Enabled,
		      data->channel);

	TIMER_SetTimerCallback(DUAL_Hander, data->channel, (CSK_TIMER_SignalEvent_t)data->alarm_cfg.callback, NULL);

	TIMER_SetTimerPeriodByCount(DUAL_Hander, data->channel, alarm_tick);
	return 0;
}

static int counter_csk_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;

	TIMER_PowerControl(DUAL_Hander, CSK_POWER_OFF);

	TIMER_Uninitialize(DUAL_Hander);
	return 0;
}

static uint32_t counter_csk_get_top_value(const struct device *dev)
{

	return 0;
}

static const struct counter_driver_api counter_api = {.start = counter_csk_start,
						      .stop = counter_csk_stop,
						      .get_value = counter_csk_get_value,
						      .set_alarm = counter_csk_set_alarm,
						      .cancel_alarm = counter_csk_cancel_alarm,
						      .get_top_value = counter_csk_get_top_value};

#define CSK_COUNTER_INIT(n)                                                                        \
                                                                                                   \
	static struct counter_csk_data counter_data_##n;                                           \
                                                                                                   \
	static const struct counter_csk_config counter_config_##n = {                              \
		.counter_info = {.max_top_value = UINT32_MAX,                                      \
				 .freq = 1,                                                        \
				 .flags = TIMER_FLAG_INITIALIZED,                                  \
				 .channels = 2},                                                   \
		.TIME_RESOURCES =                                                                  \
			{                                                                          \
				VENUS_TIMER0,                                                      \
				IRQ_TIMER_VECTOR,                                                  \
				NULL,                                                              \
				{{0},{0}},                                                            \
				0,\
			},                                                                         \
		.irq_config = timer_csk_irq_config};                                               \
	DEVICE_DT_INST_DEFINE(n, counter_csk_init, NULL, &counter_data_##n, &counter_config_##n,   \
			      PRE_KERNEL_1, 90, &counter_api);
DT_INST_FOREACH_STATUS_OKAY(CSK_COUNTER_INIT)
