/*
 * Copyright (c) 2022 .
 */

#define DT_DRV_COMPAT listenai_csk_gptchannel

#include "hal/Driver_GPT_TIMER.h"
#include "hal/gpt_timer.h"
#include "hal/gpt.h"

#include <drivers/counter.h>

#define DEV_CFG(dev)  ((struct counter_csk_config *)(dev)->config)
#define DEV_DATA(dev) ((struct counter_csk_data *)(dev)->data)

#define MODE_TIMER  (0)
#define MODE_PWM  (1)

#define GPTCHANNEL_MAX_CHANNELS  (8)
uint8_t g_gpt0_init_flag = 0;

struct counter_csk_config {
	struct counter_config_info counter_info;
	uint32_t gpt0_freq;
	uint32_t gpt0_bits;
	uint32_t gpt0_clkdiv;
	uint8_t gptchannel;
	uint8_t channel_mode;
	void (*irq_event_func)(uint32_t event, void *param);
};

struct counter_csk_data {
	struct counter_top_cfg top_cfg;
	void *handle;
	uint8_t is_running;
	uint8_t gptchannel;
	uint32_t max_top_tick;
	uint32_t top_tick;
};


#define CSK_GPT_HANDER_ISR(index) \
static inline void CSK_GPT_SignalEvent_wrap##index(uint32_t event, void *param){\
	const struct device *dev = DEVICE_DT_INST_GET(index);\
	struct counter_csk_data *data = DEV_DATA(dev);\
	if(data->top_cfg.callback){data->top_cfg.callback(dev, data->top_cfg.user_data);}};\


#define CSK_GPTCHANNEL_IRQ_CFG_FUNC_CONF(index) .irq_event_func = CSK_GPT_SignalEvent_wrap##index


void (*hander_isr[GPTCHANNEL_MAX_CHANNELS])(uint32_t event, void *param);


static void gptchannel_csk_isr(const struct device *dev)
{
	extern void gpt0_irq_handler(void);
	gpt0_irq_handler();
}

static void csk_gpt0_total_irq_sign(void)
{
	IRQ_CONNECT(DT_IRQN(DT_INST_PARENT(0)), DT_IRQ(DT_INST_PARENT(0),priority), gptchannel_csk_isr, DEVICE_DT_INST_GET(0), 0);
	irq_enable(IRQ_GPT_VECTOR);
}

static int counter_csk_init(const struct device *dev)
{
	int32_t ret = 0;

	struct counter_csk_data *data = DEV_DATA(dev);
	struct counter_csk_config *cfg = DEV_CFG(dev);
	if(cfg->channel_mode != MODE_TIMER){
		return -EPERM;
	}
	/* Get device handle */
	data->handle = GPT0_TIMER();
	data->gptchannel = cfg->gptchannel;

	data->max_top_tick = UINT32_MAX;
	data->top_tick = data->max_top_tick;
	data->is_running = 0;
	
	data->top_cfg.callback =NULL;

	hander_isr[data->gptchannel]= cfg->irq_event_func;

	/* FIX ME: this func should init in gpt driver */
	if(g_gpt0_init_flag == 0)
	{
		csk_gpt0_total_irq_sign();

		ret = HAL_GPT_TimerInitialize(data->handle, NULL);
		if(ret != CSK_DRIVER_OK){
			printk("HAL_GPT_TimerInitialize Error = %d\n", ret);
			return -EBUSY;
		}

		ret = HAL_GPT_TimerPowerControl(data->handle, CSK_POWER_FULL);
		if(ret != CSK_DRIVER_OK){
			printk("HAL_GPT_TimerPowerControl Error = %d\n", ret);
			return -EBUSY;
		}

		g_gpt0_init_flag = 1;
	}

	ret = HAL_GPT_TimerControl(data->handle, cfg->gpt0_bits | CSK_GPT_TIMER_CLKSRC_PCLK | cfg->gpt0_clkdiv |
														CSK_GPT_TIMER_RUNMODE_REPEAT | CSK_GPT_TIMER_COUNTER_DOWN, data->gptchannel);
	if(ret != CSK_DRIVER_OK){
		printk("HAL_GPT_TimerControl Error = %d\n", ret);
		return -EBUSY;
	}

	ret = HAL_GPT_RegisterTimerCallback(data->handle, data->gptchannel, (CSK_GPT_SignalEvent_t)hander_isr[data->gptchannel]);
	if(ret != CSK_DRIVER_OK){
		printk("HAL_GPT_RegisterTimerCallback Error = %d\n", ret);
		return -EBUSY;
	}

	ret = HAL_GPT_SetTimerPeriodByCount(data->handle, data->gptchannel, data->top_tick);
	if(ret != CSK_DRIVER_OK){
		printk("init HAL_GPT_SetTimerPeriodByCount Error = %d\n", ret);
		return -EBUSY;
	}

	return 0;
}

static int counter_csk_gptchannel_start(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	if(data->is_running==0)	{
		HAL_GPT_StartTimer(data->handle, data->gptchannel);
		data->is_running = 1;
	}

	return 0;
}

static int counter_csk_gptchannel_stop(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	if(data->is_running==1)	{
		HAL_GPT_StopTimer(data->handle, data->gptchannel);
		data->is_running = 0;
	}
	return 0;
}


static int counter_csk_gptchannel_set_top_value(const struct device *dev,
					 const struct counter_top_cfg *top_cfg)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	int32_t ret = 0;
	if(data->max_top_tick >= top_cfg->ticks){
		data->top_tick = top_cfg->ticks;
	}
	else{
		printk("the top value [%d] is too big than the max value[%d] \n", top_cfg->ticks, data->max_top_tick);
		return -EINVAL;
	}

	if(top_cfg->flags){
		printk("attention:counter_top_cfg flags is not supported \n");
	}

	data->top_cfg.callback = top_cfg->callback;
	data->top_cfg.user_data = top_cfg->user_data;
	if(data->is_running){
		HAL_GPT_StopTimer(data->handle, data->gptchannel);
		ret = HAL_GPT_SetTimerPeriodDirectByCount(data->handle, data->gptchannel, data->top_tick);
		HAL_GPT_StartTimer(data->handle, data->gptchannel);

	}else{
		ret = HAL_GPT_SetTimerPeriodDirectByCount(data->handle, data->gptchannel, data->top_tick);
	}

	if(ret != CSK_DRIVER_OK){
		printk("HAL_GPT_SetTimerPeriodDirectByCount Error = %d \n", ret);
		return -EBUSY;
	}
	return 0;
}

static uint32_t counter_csk_gptchannel_get_top_value(const struct device *dev)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	return data->top_tick;
}
static uint32_t counter_csk_gptchannel_get_freq(const struct device *dev)
{
	struct counter_csk_config *cfg = DEV_CFG(dev);
	return cfg->gpt0_freq;
}

static int counter_csk_gptchannel_get_value(const struct device *dev, uint32_t *ticks)
{
	struct counter_csk_data *data = DEV_DATA(dev);
	HAL_GPT_ReadTimerCount(data->handle, data->gptchannel, ticks);
	return 0;
}

static int counter_csk_gptchannel_set_alarm(const struct device *dev, uint8_t chan_id,
				 const struct counter_alarm_cfg *alarm_cfg)
{
	/* not support */
	return 0;
}

static int counter_csk_gptchannel_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
	/* not support */
	return 0;
}

static uint32_t counter_csk_gptchannel_get_pending_int(const struct device *dev)
{
	/* not support */
	return 0;
}

static uint32_t  counter_csk_gptchannel_get_guard_period(const struct device *dev, uint32_t flags)
{
	/* not support */
	return 0;
}

static int counter_csk_gptchannel_set_guard_period(const struct device *dev, uint32_t ticks, uint32_t flags)
{
	/* not support */
	return 0;
}


static const struct counter_driver_api counter_api = {
							  .start = counter_csk_gptchannel_start,
						      .stop = counter_csk_gptchannel_stop,
						      .get_value = counter_csk_gptchannel_get_value,
						      .set_alarm = counter_csk_gptchannel_set_alarm,
						      .cancel_alarm = counter_csk_gptchannel_cancel_alarm,
							  .set_top_value = counter_csk_gptchannel_set_top_value,
							  .get_pending_int =counter_csk_gptchannel_get_pending_int,
						      .get_top_value = counter_csk_gptchannel_get_top_value,
							  .get_guard_period =counter_csk_gptchannel_get_guard_period,
						      .set_guard_period = counter_csk_gptchannel_set_guard_period,
							  .get_freq = counter_csk_gptchannel_get_freq};

#define CSK_GPTCHANNEL_COUNTER_INIT(index)                                                           \
	CSK_GPT_HANDER_ISR(index)                                                                        \
	static struct counter_csk_data counter_data_##index;                                             \
	static const struct counter_csk_config counter_config_##index = {                                \
		.counter_info = {                                                                            \
			.max_top_value = UINT32_MAX,                                                             \
			.freq = 0,                                                                               \
			.flags = 0,                                                                              \
			.channels = 0                                                                            \
		},                                                                                           \
		.gpt0_freq = BOARD_BOOTCLOCKRUN_AP_CMN_CFG_PCLK_CLK,                                         \
		.gpt0_bits = CSK_GPT_TIMER_32_BIT_TIMER,                                                     \
		.gpt0_clkdiv = CSK_GPT_TIMER_CLKDIV_1,                                                       \
		.gptchannel = DT_INST_PROP(index, channel_id),                                               \
		.channel_mode = DT_INST_PROP(index, channel_mode),                                           \
		CSK_GPTCHANNEL_IRQ_CFG_FUNC_CONF(index),                                                     \
	};                                                                                               \
	DEVICE_DT_INST_DEFINE(index, counter_csk_init, NULL, &counter_data_##index, &counter_config_##index,   \
			      PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &counter_api);

DT_INST_FOREACH_STATUS_OKAY(CSK_GPTCHANNEL_COUNTER_INIT)
