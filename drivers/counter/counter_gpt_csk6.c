/*
 * Copyright (c) 2022 .
 */

#define DT_DRV_COMPAT listenai_csk_gpt

#include "hal/Driver_GPT_TIMER.h"
#include "hal/gpt_timer.h"
#include "hal/gpt.h"

#include <drivers/counter.h>
#define GPTTIMER_MAX_CHANNELS  (8)
#define GPTIMER_NODEID DT_INST(0, listenai_csk_gpt)
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
#define USR2GPT_CHANNEL(X) 	  (GPTTIMER_MAX_CHANNELS - X -1)
struct counter_csk_config {
	struct counter_config_info counter_info;
	void (*irq_config)(void);
};
typedef struct {
	struct counter_alarm_cfg alarm_cfg;
	
}T_Config_E;
struct counter_csk_data {
	T_Config_E config[GPTTIMER_MAX_CHANNELS];
	uint8_t channel;
	void *handle;
	uint32_t freq;
};

static void *timer_csk_get_ctx(const struct device *dev)
{
	return GPT0_TIMER();
}

void counter_alarm_callback_wrap(const struct device *dev,
					 uint8_t chan_id, uint32_t ticks,
					 void *user_data){

}

#define CSK_GPT_HANDER_ISR(n) \
static inline void CSK_GPT_SignalEvent_wrap##n(uint32_t event, void *param){\
	const struct device *dev = device_get_binding(DT_LABEL(DT_NODELABEL(gpt0)));\
	struct counter_csk_data *data = DEV_DATA(dev);\
	data->config[n].alarm_cfg.callback(dev, n-PWM_CHANNELS, 0, data->config[n].alarm_cfg.user_data);}\

CSK_GPT_HANDER_ISR(0)
CSK_GPT_HANDER_ISR(1)
CSK_GPT_HANDER_ISR(2)
CSK_GPT_HANDER_ISR(3)
CSK_GPT_HANDER_ISR(4)
CSK_GPT_HANDER_ISR(5)
CSK_GPT_HANDER_ISR(6)
CSK_GPT_HANDER_ISR(7)

void (*hander_isr[GPTTIMER_MAX_CHANNELS])(uint32_t event, void *param) = {
	CSK_GPT_SignalEvent_wrap0,
	CSK_GPT_SignalEvent_wrap1,
	CSK_GPT_SignalEvent_wrap2,
	CSK_GPT_SignalEvent_wrap3,
	CSK_GPT_SignalEvent_wrap4,
	CSK_GPT_SignalEvent_wrap5,
	CSK_GPT_SignalEvent_wrap6,
	CSK_GPT_SignalEvent_wrap7};
static void timer_csk_isr(const struct device *dev)
{
	extern void gpt0_irq_handler(void);
	gpt0_irq_handler();

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
	void *GPT_TIM_Hander = data->handle;
	HAL_GPT_TimerInitialize(GPT_TIM_Hander,NULL);
	return 0;
}

static int counter_csk_gpt_start(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	uint32_t ret;
	
	ret = HAL_GPT_TimerInitialize(GPT_TIM_Hander,NULL);
	ret = HAL_GPT_TimerPowerControl(GPT_TIM_Hander, CSK_POWER_FULL);

	for(int ii = PWM_CHANNELS; ii< GPTTIMER_MAX_CHANNELS;ii ++){
		ret = HAL_GPT_TimerControl(GPT_TIM_Hander, CSK_GPT_TIMER_32_BIT_TIMER | CSK_GPT_TIMER_CLKSRC_PCLK | CSK_GPT_TIMER_CLKDIV_1 |
							   CSK_GPT_TIMER_RUNMODE_SINGLE | CSK_GPT_TIMER_COUNTER_DOWN, ii);
		if(ret != CSK_DRIVER_OK){
			return -ENOTSUP;
		}

	}
	return 0;
}

static int counter_csk_gpt_stop(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	HAL_GPT_TimerUninitialize(GPT_TIM_Hander);

	
	return 0;
}

static int counter_csk_gpt_get_value(const struct device *dev, uint32_t *ticks)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	uint8_t channel = data->channel;
	if(channel >= GPTTIMER_MAX_CHANNELS || channel <PWM_CHANNELS){
		return -ENOTSUP;
	}
	HAL_GPT_ReadTimerCount(GPT_TIM_Hander, channel, ticks);
	return 0;
}
#define TESTER_FUNC(prefix,number)  prefix##number

static int counter_csk_gpt_set_alarm(const struct device *dev, uint8_t chan_id,
				 const struct counter_alarm_cfg *alarm_cfg)
{
	int ret = 0;
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;

	data->channel = USR2GPT_CHANNEL(chan_id);
	uint32_t alarm_tick = alarm_cfg->ticks;
	data->config[data->channel].alarm_cfg.callback = alarm_cfg->callback;
	data->config[data->channel].alarm_cfg.user_data = alarm_cfg->user_data;
	if(data->channel >= GPT_NUMBER_OF_CHANNELS)
	{
		return -ENOTSUP;
	}

	HAL_GPT_StopTimer(GPT_TIM_Hander, data->channel);	
	
	ret = HAL_GPT_RegisterTimerCallback(GPT_TIM_Hander, data->channel, (CSK_GPT_SignalEvent_t)hander_isr[data->channel]);
	if(ret != CSK_DRIVER_OK){
		return -ENOTSUP;
	}

	ret = HAL_GPT_SetTimerPeriodByCount(GPT_TIM_Hander, data->channel, alarm_tick);
	if(ret != CSK_DRIVER_OK){
		return -ENOTSUP;
	}

	ret = HAL_GPT_StartTimer(GPT_TIM_Hander, data->channel);	
	if(ret != CSK_DRIVER_OK){
		return -ENOTSUP;
	}

	return 0;
}

static int counter_csk_gpt_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	void *GPT_TIM_Hander = data->handle;
	
	HAL_GPT_StopTimer(GPT_TIM_Hander, chan_id);
	
	
	return 0;
}
static int counter_csk_gpt_set_top_value(const struct device *dev,
					 const struct counter_top_cfg *cfg)
{//not support
	if (cfg->ticks != UINT32_MAX) {
		return -ENOTSUP;
	} else {
		return UINT32_MAX;
	}
	return 0;
}
static uint32_t counter_csk_gpt_get_top_value(const struct device *dev)
{//not support
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
				 .channels = GPT_NUMBER_OF_CHANNELS - PWM_CHANNELS},                                                   \
		.irq_config = timer_csk_irq_config};                                               \
	DEVICE_DT_INST_DEFINE(n, counter_csk_init, NULL, &counter_data_##n, &counter_config_##n,   \
			      PRE_KERNEL_1, 90, &counter_api);
DT_INST_FOREACH_STATUS_OKAY(CSK_COUNTER_INIT)
