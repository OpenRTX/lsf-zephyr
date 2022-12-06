/*
 * Copyright (c) 2022 .
 */

#define DT_DRV_COMPAT listenai_csk_counter


#include <drivers/counter.h>
#include "hal/dual_timer.h"
#include "hal/Driver_DUAL_TIMER.h"
#define DEV_CFG(dev) (( struct counter_csk_config *)(dev)->config)
#define DEV_DATA(dev) ((struct counter_csk_data *)(dev)->data)

static TIMER_INFO info = {0};
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

static uint32_t csk_get_value(const struct device *dev){
	uint32_t now;
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;

	TIMER_ReadTimerCount(DUAL_Hander,channel,&now);

	return now;
}

static void timer_csk_isr(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	TIMER_RESOURCES *DUAL_Hander = data->handle;

	uint32_t status;
    uint32_t event = 0;
	uint32_t now = csk_get_value(dev);

    // Status register hsec set 1 and user enable current interrupt
    // if((status & STATUS_HSEC) && (DUAL_Hander->info->flag & RTC_FLAG_HSEC_INT)){
    //     event |= CSK_RTC_EVENT_HSEC_INT;
    // }

    // if((status & STATUS_SEC) && (DUAL_Hander->info->flag & RTC_FLAG_SEC_INT)){
    //     event |= CSK_RTC_EVENT_SEC_INT;
    // }

    // if((status & STATUS_MIN) && (DUAL_Hander->info->flag & RTC_FLAG_MIN_INT)){
    //     event |= CSK_RTC_EVENT_MIN_INT;
    // }

    // if((status & STATUS_HOUR) && (DUAL_Hander->info->flag & RTC_FLAG_HOUR_INT)){
    //     event |= CSK_RTC_EVENT_HOUR_INT;
    // }

    // if((status & STATUS_DAY) && (DUAL_Hander->info->flag & RTC_FLAG_DAY_INT)){
    //     event |= CSK_RTC_EVENT_DAY_INT;
    // }

    // if((status & STATUS_ALARM_INT) && (DUAL_Hander->info->flag & RTC_FLAG_ALARM)){
    //     event |= CSK_RTC_EVENT_ALARM_INT;
	// 	if (data->alarm_cfg.callback != NULL) {
	// 		data->alarm_cfg.callback(dev, 0, now, data->alarm_cfg.user_data);
	// 	}
    // }
	if (data->alarm_cfg.callback != NULL) {
			data->alarm_cfg.callback(dev, 0, now, data->alarm_cfg.user_data);
		}

}

static void timer_csk_irq_config(void)
{
	IRQ_CONNECT(IRQ_TIMER_VECTOR,
		    2,
		    timer_csk_isr, 
			DEVICE_DT_GET(DT_NODELABEL(dultim)), 
			0);
	irq_enable(IRQ_TIMER_VECTOR);
}


static int counter_csk_init(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	struct counter_csk_config *cfg = DEV_CFG(dev);

	/* Get device handle */
	data->handle = timer_csk_get_ctx(dev);
	void *DUAL_Hander = data->handle;
	TIMER_Initialize(DUAL_Hander);
	TIMER_PowerControl(DUAL_Hander, CSK_POWER_FULL);
	// Setting irq
	cfg->irq_config();
	return 0;
}

static int counter_csk_start(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;
    // RTC_Control(DUAL_Hander, CSK_RTC_CTRL_EN, 1);
	TIMER_StartTimer(DUAL_Hander, channel);
	return 0;
}

static int counter_csk_stop(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;
	uint8_t channel = data->channel;
    // RTC_Control(DUAL_Hander, CSK_RTC_CTRL_EN, 0);
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
	uint8_t channel = data->channel;

	uint32_t alarm_tick = alarm_cfg->ticks;

	data->alarm_cfg.callback = alarm_cfg->callback;
	data->alarm_cfg.user_data = alarm_cfg->user_data;

    TIMER_Control(DUAL_Hander, CSK_TIMER_PRESCALE_Divide_1 | \
                                    CSK_TIMER_SIZE_32Bit | \
                                    CSK_TIMER_MODE_Periodic | \
                                    CSK_TIMER_INTERRUPT_Enabled, channel);

    TIMER_SetTimerCallback(DUAL_Hander, channel, data->alarm_cfg.callback, NULL);

    TIMER_SetTimerPeriodByCount(DUAL_Hander, channel, alarm_tick);
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
	struct counter_csk_data *data = DEV_DATA(dev);
	void *DUAL_Hander = data->handle;

	return  0;
}

static const struct counter_driver_api counter_api = {
	.start = counter_csk_start,
	.stop = counter_csk_stop,
	.get_value = counter_csk_get_value,
	.set_alarm = counter_csk_set_alarm,
	.cancel_alarm = counter_csk_cancel_alarm,
	.get_top_value = counter_csk_get_top_value
};


#define CSK_COUNTER_INIT(n)						 \
										 \
	static struct counter_csk_data counter_data_##n;			 \
										 \
	static const struct counter_csk_config counter_config_##n = {		 \
		.counter_info = {						 	\
			.max_top_value = UINT32_MAX,			\
			.freq = 1000, 								\
			.flags = TIMER_FLAG_INITIALIZED,			\
			.channels = 0						 	\
		},								 	\
		.irq_config = timer_csk_irq_config};	    		 \
	DEVICE_DT_INST_DEFINE(n,						     \
			      counter_csk_init,				         \
			      NULL, &counter_data_##n,				 \
			      &counter_config_##n, PRE_KERNEL_1,		 \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &counter_api);

DT_INST_FOREACH_STATUS_OKAY(CSK_COUNTER_INIT)


