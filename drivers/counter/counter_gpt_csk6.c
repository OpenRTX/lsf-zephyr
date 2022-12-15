/*
 * Copyright (c) 2022 .
 */

#define DT_DRV_COMPAT listenai_gpttimer
// #define DT_DRV_COMPAT listenai_csk_gpttmr

#include "hal/Driver_GPT_TIMER.h"
#include "hal/gpt_timer.h"
#include "hal/gpt.h"

#include <drivers/counter.h>
#define GPTTIMER_MAX_CHANNELS  (8)
#define GPTIMER_NODEID DT_INST(0, listenai_gpttimer)
#define PWM_CHANNELS DT_PROP(GPTIMER_NODEID, pwm_channels)

#if (PWM_CHANNELS > GPTTIMER_MAX_CHANNELS)
#error "The number of pwm channels exceeds the maximum(8) limit"
#endif

#define GPTTMR_CHANNEL(ch)  (ch + PWM_CHANNELS)

#define DEV_CFG(dev)  ((struct counter_csk_config *)(dev)->config)
#define DEV_DATA(dev) ((struct counter_csk_data *)(dev)->data)
#define MAX_FREQ 200000000
#define GPT_PERIOD_32BIT 	  (0xE3600)
#define GPT_PERIOD_16BIT 	  (0x55558888)
#define GPT_PERIOD_8BIT 	  (0x3388BBFF)
struct counter_csk_config {
	struct counter_config_info counter_info;
	void (*irq_config)(void);
};
struct counter_csk_data {
	struct counter_alarm_cfg alarm_cfg;
	uint32_t ticks;
	uint8_t channel;
	void *handle;
	uint32_t freq;
};

static void *timer_csk_get_ctx(const struct device *dev)
{
	return GPT0_TIMER();
}

static uint32_t csk_get_value(const struct device *dev)
{
	uint32_t now;
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	uint8_t channel = GPTTMR_CHANNEL(data->channel);

	HAL_GPT_ReadTimerCount(GPT_TIM_Hander, channel, &now);

	return now;
}

static void timer_csk_isr(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	GPT_TIMER_RESOURCES *pGpt_Temp = (GPT_TIMER_RESOURCES *)data->handle;
	

	uint32_t ch, intflag_timer, intflag_ch;

	//get interrupt status and clear interrupt flag
	intflag_timer = pGpt_Temp->gpt_resources->reg->ISR_TIMER;
	//wirte 1 to clear interrupt flag
	pGpt_Temp->gpt_resources->reg->ICR_TIMER = intflag_timer;

	//input capture and ledc interrupt status
	intflag_ch = pGpt_Temp->gpt_resources->reg->ISR_CH;
	pGpt_Temp->gpt_resources->reg->ICR_CH = intflag_ch;

	for(ch = 0; ch < GPT_NUMBER_OF_CHANNELS; ch++)
	{
		uint32_t now =0;
		HAL_GPT_ReadTimerCount(pGpt_Temp, ch, &now);
		//timer overflow interrupt handle
		if((pGpt_Temp->gpt_resources->hardware->channel_stat[ch] == HARDWARE_CHANNEL_STAT_USED_BY_TIM)
		&&(intflag_timer & (0x01010101 << ch)) && now == 0)
		{
			data->alarm_cfg.callback(dev, ch, now, data->alarm_cfg.user_data);
			// pGpt_Temp->gpt_resources->info->cb_event[ch](CSK_GPT_EVENT_OVERFLOW, pGpt_Temp->gpt_resources->user_param);
		}

	}
}

static void timer_csk_irq_config(void)
{
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), timer_csk_isr, DEVICE_DT_GET(DT_NODELABEL(gpt0)), 0);
	irq_enable(IRQ_GPT_VECTOR);
}

static int counter_csk_init(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	struct counter_csk_config *cfg = DEV_CFG(dev);
	/* Get device handle */

	data->handle = timer_csk_get_ctx(dev);
	data->freq = BOARD_BOOTCLOCKRUN_AP_CMN_CFG_PCLK_CLK;
	cfg->irq_config();

	return 0;
}

static int counter_csk_gpt_start(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	uint32_t runmode = CSK_GPT_TIMER_RUNMODE_SINGLE;
	uint32_t ret;

	ret = HAL_GPT_TimerInitialize(GPT_TIM_Hander,NULL);
	ret = HAL_GPT_TimerPowerControl(GPT_TIM_Hander, CSK_POWER_FULL);
	for(int ii = 0; ii< 8;ii ++){
		ret = HAL_GPT_TimerControl(GPT_TIM_Hander, CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_1 |
							   CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, ii);
		if(ret != CSK_DRIVER_OK){
			printk(" HAL_GPT_TimerControl err %d\n",ret);
			return -ENOTSUP;
		}
	}
	return 0;
}

static int counter_csk_gpt_stop(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	uint8_t channel = data->channel;

	HAL_GPT_StopTimer(GPT_TIM_Hander, channel);
	return 0;
}

static int counter_csk_gpt_get_value(const struct device *dev, uint32_t *ticks)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	uint8_t channel = data->channel;

	HAL_GPT_ReadTimerCount(GPT_TIM_Hander, channel, ticks);
	return 0;
}

static int counter_csk_gpt_set_alarm(const struct device *dev, uint8_t chan_id,
				 const struct counter_alarm_cfg *alarm_cfg)
{
	int ret = 0;
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;

	data->channel = chan_id;
	uint32_t alarm_tick = alarm_cfg->ticks;

	data->alarm_cfg.callback = alarm_cfg->callback;
	data->alarm_cfg.user_data = alarm_cfg->user_data;
	if(data->channel >= GPT_NUMBER_OF_CHANNELS)
	{
		return -ENOTSUP;
	}
	HAL_GPT_StopTimer(GPT_TIM_Hander, data->channel);	
	ret = HAL_GPT_RegisterTimerCallback(GPT_TIM_Hander, data->channel, (CSK_GPT_SignalEvent_t)data->alarm_cfg.callback);
	if(ret != CSK_DRIVER_OK){
		printk(" HAL_GPT_RegisterTimerCallback err %d\n",ret);
		return -ENOTSUP;
	}
	ret = HAL_GPT_SetTimerPeriodByCount(GPT_TIM_Hander, data->channel, alarm_tick);
	if(ret != CSK_DRIVER_OK){
		printk(" HAL_GPT_SetTimerPeriodByCount err %d\n",ret);
		return -ENOTSUP;
	}
	ret = HAL_GPT_StartTimer(GPT_TIM_Hander, data->channel);	
	if(ret != CSK_DRIVER_OK){
		printk(" HAL_GPT_StartTimer err %d\n",ret);
		return -ENOTSUP;
	}
	return 0;
}

static int counter_csk_gpt_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;

	HAL_GPT_TimerUninitialize(GPT_TIM_Hander);	
	return 0;
}
static int counter_csk_gpt_set_top_value(const struct device *dev,
					 const struct counter_top_cfg *cfg)
{
	if (cfg->ticks != UINT32_MAX) {
		return -ENOTSUP;
	} else {
		return 0;
	}
	return 0;
}
static uint32_t counter_csk_gpt_get_top_value(const struct device *dev)
{
	return 0;
}
static uint32_t counter_csk_gpt_get_freq(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);

	return data->freq;
}

static const struct counter_driver_api counter_api = {
							  .start = counter_csk_gpt_start,
						      .stop = counter_csk_gpt_stop,
						      .get_value = counter_csk_gpt_get_value,
						      .set_alarm = counter_csk_gpt_set_alarm,
						      .cancel_alarm = counter_csk_gpt_cancel_alarm,
							  .set_top_value = counter_csk_gpt_set_top_value,
							  .get_freq = counter_csk_gpt_get_freq,
						      .get_top_value = counter_csk_gpt_get_top_value};

#define CSK_COUNTER_INIT(n)                                                                        \
                                                                                                   \
	static struct counter_csk_data counter_data_##n;                                           \
                                                                                                   \
	static const struct counter_csk_config counter_config_##n = {                              \
		.counter_info = {.max_top_value = UINT32_MAX,                                      \
				 .freq = MAX_FREQ,                                                        \
				 .flags = GPT_FLAG_INITIALIZED,                                  \
				 .channels = GPT_NUMBER_OF_CHANNELS},                                                   \
		.irq_config = timer_csk_irq_config};                                               \
	DEVICE_DT_INST_DEFINE(n, counter_csk_init, NULL, &counter_data_##n, &counter_config_##n,   \
			      PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &counter_api);
DT_INST_FOREACH_STATUS_OKAY(CSK_COUNTER_INIT)
