/*
 * Copyright (c) 2022 .
 */

#define DT_DRV_COMPAT listenai_csk_rtc


#include <drivers/counter.h>
#include "hal/Driver_RTC.h"
#include "hal/rtc.h"


#define DEV_CFG(dev) (( struct counter_csk_config *)(dev)->config)
#define DEV_DATA(dev) ((struct counter_csk_data *)(dev)->data)

static RTC_INFO info = {0};
struct counter_csk_config {
	struct counter_config_info counter_info;
	void (*irq_config)(void);
	RTC_RESOURCES rtc_resources;

};
struct counter_csk_data {
	struct counter_alarm_cfg *alarm_cfg;
	uint32_t ticks;
	void *handle;
};

static void *rtc_csk_get_ctx(const struct device *dev)
{
	VENUS_RTC_RegDef *base = DEV_CFG(dev)->rtc_resources.reg;
	if (base == CSK_AON_RTC) {
		return RTC();
	} else {
		return NULL;
	}
}

static uint32_t csk_get_value(const struct device *dev){
	uint32_t now;
	CSK_RTC_TIME stime;
	struct counter_csk_data *data = DEV_DATA(dev);
	void *RTC_Handler = data->handle;
	RTC_GetTime(RTC_Handler, &stime);
	now = ((stime.day << 17) |\
		(stime.hour << 12) |\
		(stime.min << 6) |\
		(stime.sec));
	return now;
}

static void rtc_csk_isr(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	RTC_RESOURCES *rtc = data->handle;

	uint32_t status;
    uint32_t event = 0;
    status = rtc->reg->STATUS;
    rtc->reg->STATUS = (status & 0xff);

    // Status register hsec set 1 and user enable current interrupt
    if((status & STATUS_HSEC) && (rtc->info->flag & RTC_FLAG_HSEC_INT)){
        event |= CSK_RTC_EVENT_HSEC_INT;
    }

    if((status & STATUS_SEC) && (rtc->info->flag & RTC_FLAG_SEC_INT)){
        event |= CSK_RTC_EVENT_SEC_INT;
    }

    if((status & STATUS_MIN) && (rtc->info->flag & RTC_FLAG_MIN_INT)){
        event |= CSK_RTC_EVENT_MIN_INT;
    }

    if((status & STATUS_HOUR) && (rtc->info->flag & RTC_FLAG_HOUR_INT)){
        event |= CSK_RTC_EVENT_HOUR_INT;
    }

    if((status & STATUS_DAY) && (rtc->info->flag & RTC_FLAG_DAY_INT)){
        event |= CSK_RTC_EVENT_DAY_INT;
    }

    if((status & STATUS_ALARM_INT) && (rtc->info->flag & RTC_FLAG_ALARM)){
        event |= CSK_RTC_EVENT_ALARM_INT;
    }

	if (data->alarm_cfg->callback != NULL) {
		data->alarm_cfg->callback(dev, 0, data->alarm_cfg->ticks, data->alarm_cfg->user_data);
	}
}

static void rtc_csk_irq_config(void)
{
	IRQ_CONNECT(IRQ_AON_RTC_VECTOR,
		    2,
		    rtc_csk_isr, 
			DEVICE_DT_GET(DT_NODELABEL(rtc1)), 
			0);
	irq_enable(IRQ_AON_RTC_VECTOR);
}


static int counter_csk_init(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	struct counter_csk_config *cfg = DEV_CFG(dev);

    CSK_RTC_TIME stime = {0, 0, 0, 0};

	/* Get device handle */
	data->handle = rtc_csk_get_ctx(dev);
	void *RTC_Handler = data->handle;

    RTC_Initialize(RTC_Handler, NULL, NULL);
	RTC_PowerControl(RTC_Handler, CSK_POWER_OFF);
	RTC_PowerControl(RTC_Handler, CSK_POWER_FULL);
	RTC_SetTime(RTC_Handler, &stime);
    RTC_Control(RTC_Handler, CSK_RTC_CTRL_EN, 0);

	// Setting irq
	cfg->irq_config();
	return 0;
}

static int counter_csk_start(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *RTC_Handler = data->handle;
    RTC_Control(RTC_Handler, CSK_RTC_CTRL_EN, 1);

	return 0;
}

static int counter_csk_stop(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *RTC_Handler = data->handle;
    RTC_Control(RTC_Handler, CSK_RTC_CTRL_EN, 0);

	return 0;
}

static int counter_csk_get_value(const struct device *dev, uint32_t *ticks)
{
	*ticks = csk_get_value(dev);
	return 0;
}

static int counter_csk_set_alarm(const struct device *dev, uint8_t chan_id,
				   const struct counter_alarm_cfg *alarm_cfg)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *RTC_Handler = data->handle;

	// alarm time setting
	CSK_RTC_ALARM alarm_time;
	uint32_t alarm_tick = alarm_cfg->ticks;
	alarm_time.hour = (alarm_tick & ALARM_HOUR) >> 12;
    alarm_time.min = (alarm_tick & ALARM_MIN) >> 6;
    alarm_time.sec = (alarm_tick & ALARM_SEC);
	RTC_SetAlarm(RTC_Handler, &alarm_time);

	// alarm callback funt 
	 data->alarm_cfg = (struct counter_alarm_cfg *)alarm_cfg;

	// open alarm INT
	RTC_Control(RTC_Handler, CSK_RTC_CTRL_ALARM_INT, 1);
	irq_enable(IRQ_AON_RTC_VECTOR);

	return 0;
}

static int counter_csk_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *RTC_Handler = data->handle;
	RTC_RESOURCES *rtc = data->handle;

	irq_disable(rtc->irq_num);
	RTC_Control(RTC_Handler, CSK_RTC_CTRL_ALARM_INT, 0);

	return 0;
}

static uint32_t counter_csk_get_top_value(const struct device *dev)
{
	return  ((RTC_SET_TIME_DAY_MARGIN << 17) |\
            (RTC_SET_TIME_HOUR_MARGIN << 12) |\
            (RTC_SET_TIME_MIN_MARGIN << 6) |\
            (RTC_SET_TIME_SEC_MARGIN));
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
			.freq = 1, 								\
			.flags = RTC_FLAG_INITIALIZED,			\
			.channels = 1						 	\
		},								 	\
		.rtc_resources = {					\
			CSK_AON_RTC,					\
			IRQ_AON_RTC_VECTOR,				\
			NULL,					        \
			&info,							\
		},									\
		.irq_config = rtc_csk_irq_config};	    		 \
	DEVICE_DT_INST_DEFINE(n,						     \
			      counter_csk_init,				         \
			      NULL, &counter_data_##n,				 \
			      &counter_config_##n, PRE_KERNEL_1,		 \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &counter_api);

DT_INST_FOREACH_STATUS_OKAY(CSK_COUNTER_INIT)


