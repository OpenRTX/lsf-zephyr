/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <kernel.h>
#include <zephyr.h>
#include <device.h>
#include <sys/printk.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <drivers/display.h>
#include <drivers/kscan.h>
#include <drivers/pinmux.h>
#include <drivers/pwm.h>
#include <drivers/adc.h>
#include <shell/shell.h>

LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);
// device:Keysense
#define KSCAN_DEVICE DT_LABEL(DT_ALIAS(analogkey))

// device:ADC
#define ADC_STACKSIZE       4096
struct k_thread  adc_task_id;
K_THREAD_STACK_DEFINE(adc_stack_area, ADC_STACKSIZE);

#if !DT_NODE_EXISTS(DT_PATH(vbatt)) || \
	!DT_NODE_HAS_PROP(DT_PATH(vbatt), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define ADC_NUM_CHANNELS	DT_PROP_LEN(DT_PATH(vbatt), io_channels)

#if ADC_NUM_CHANNELS > 2
#error "Currently only 1 or 2 channels supported in this sample"
#endif

#if ADC_NUM_CHANNELS == 2 && !DT_SAME_NODE( \
	DT_PHANDLE_BY_IDX(DT_PATH(vbatt), io_channels, 0), \
	DT_PHANDLE_BY_IDX(DT_PATH(vbatt), io_channels, 1))
#error "Channels have to use the same ADC."
#endif

#define ADC_NODE		DT_PHANDLE(DT_PATH(vbatt), io_channels)

/* Common settings supported by most ADCs */
#define ADC_RESOLUTION		11
#define ADC_GAIN		ADC_GAIN_1
#define ADC_REFERENCE		ADC_REF_INTERNAL

/* Get the numbers of up to two channels */
static uint8_t channel_ids[ADC_NUM_CHANNELS] = {
	DT_IO_CHANNELS_INPUT_BY_IDX(DT_PATH(vbatt), 0),
#if ADC_NUM_CHANNELS == 2
	DT_IO_CHANNELS_INPUT_BY_IDX(DT_PATH(vbatt), 1)
#endif
};

static int16_t sample_buffer[ADC_NUM_CHANNELS];


static struct adc_channel_cfg channel_cfg = {
	.gain = ADC_GAIN_1,
	.reference = ADC_REFERENCE,
	.channel_id = 0,
};

static struct adc_sequence sequence = {
	/* individual channels will be added below */
	.channels    = 0,
	.buffer      = sample_buffer,
	/* buffer size in bytes, not number of samples */
	.buffer_size = sizeof(sample_buffer),
	.resolution  = ADC_RESOLUTION,
};


// devicetree: LCD PWM
struct pwm_csk_config {
	uint8_t pwm_pin;
	uint8_t pwm_func;
	uint8_t clock_source;
	uint8_t clock_prescaler;
	uint8_t mode;
	const struct device *pinmux_drv;
};

// PWM
#define SDIO_PIN_FUNC 0
#define PWM_LED0_NODE	DT_NODELABEL(pwmlcd)

#if DT_NODE_HAS_PROP(PWM_LED0_NODE, pinmuxs)
#define PWM 		DEVICE_DT_GET(DT_NODELABEL(pwmlcd))
#else
#error "No PWM pinmuxs found"
#endif
// devicetree: power lock
#define PWR_LOCK_NODE 	DT_NODELABEL(pwrlock)
#define GPIO_PWR_LOCK	DT_GPIO_CTLR(PWR_LOCK_NODE, gpios) 
#define PIN_PWR_LOCK	DT_GPIO_PIN(PWR_LOCK_NODE, gpios) 
#define FLAGS_PWR_LOCK	DT_GPIO_FLAGS(PWR_LOCK_NODE, gpios) 



// devicetree: LED CAM NODE
struct k_thread  camled_task_id;
#define CAM_LED_STACKSIZE	1024 
K_THREAD_STACK_DEFINE(camled_stack_area, CAM_LED_STACKSIZE);
#define CAM_LED 		DT_NODELABEL(camleds1)
#define GPIO_LED_CAM 	DT_GPIO_CTLR_BY_IDX(CAM_LED, gpios, 0) 

#define FY_KEY 			DT_NODELABEL(fy_key_0)
#define GPIO_FY_KEY 	DT_GPIO_CTLR_BY_IDX(FY_KEY, gpios, 0) 

// device: touch and LCD
#define STACKSIZE       4096
struct k_thread  lcdtouch_task_id;
K_THREAD_STACK_DEFINE(lcdtouch_stack_area, STACKSIZE);

#if DT_NODE_HAS_STATUS(DT_INST(0, ilitek_ili9340), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, ilitek_ili9340))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd1306fb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd1306fb))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd16xxfb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd16xxfb))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, sitronix_st7789v), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, sitronix_st7789v))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, sitronix_st7735r), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, sitronix_st7735r))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, fsl_imx6sx_lcdif), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, fsl_imx6sx_lcdif))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, betterlife_bl6xxx), okay)
#define DISPLAY_KSAN_DEV_NAME DT_LABEL(DT_INST(0, betterlife_bl6xxx))
#endif

#ifdef CONFIG_SDL_DISPLAY_DEV_NAME
#define DISPLAY_DEV_NAME CONFIG_SDL_DISPLAY_DEV_NAME
#endif

#ifdef CONFIG_DUMMY_DISPLAY_DEV_NAME
#define DISPLAY_DEV_NAME CONFIG_DUMMY_DISPLAY_DEV_NAME
#endif

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

#ifdef CONFIG_ARCH_POSIX
#define RETURN_FROM_MAIN(exit_code) posix_exit_main(exit_code)
#else
#define RETURN_FROM_MAIN(exit_code) return
#endif

enum corner {
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_RIGHT,
	BOTTOM_LEFT
};

typedef void (*fill_buffer)(enum corner corner, uint8_t grey, uint8_t *buf,
			    size_t buf_size);


#ifdef CONFIG_ARCH_POSIX
static void posix_exit_main(int exit_code)
{
#if CONFIG_TEST
	if (exit_code == 0) {
		LOG_INF("PROJECT EXECUTION SUCCESSFUL");
	} else {
		LOG_INF("PROJECT EXECUTION FAILED");
	}
#endif
	posix_exit(exit_code);
}
#endif

static void k_callback(const struct device *dev, uint32_t row, uint32_t col,
		       bool pressed)
{
	ARG_UNUSED(dev);
	printk("row = %u col = %u, pressed:%s\n", row, col, pressed ? "TRUE" : "FLASE");
}

void kscan_init(void)
{
	const struct device *kscan_dev = device_get_binding(DISPLAY_KSAN_DEV_NAME);

	if (!device_is_ready(kscan_dev)) {
		LOG_ERR("kscan device %s not ready", kscan_dev->name);
		return;
	}

	kscan_config(kscan_dev, k_callback);
	kscan_enable_callback(kscan_dev);
}

static void fill_buffer_argb8888(enum corner corner, uint8_t grey, uint8_t *buf,
				 size_t buf_size)
{
	uint32_t color = 0;

	switch (corner) {
	case TOP_LEFT:
		color = 0x00FF0000u;
		break;
	case TOP_RIGHT:
		color = 0x0000FF00u;
		break;
	case BOTTOM_RIGHT:
		color = 0x000000FFu;
		break;
	case BOTTOM_LEFT:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 4) {
		*((uint32_t *)(buf + idx)) = color;
	}
}

static void fill_buffer_rgb888(enum corner corner, uint8_t grey, uint8_t *buf,
			       size_t buf_size)
{
	uint32_t color = 0;

	switch (corner) {
	case TOP_LEFT:
		color = 0x00FF0000u;
		break;
	case TOP_RIGHT:
		color = 0x0000FF00u;
		break;
	case BOTTOM_RIGHT:
		color = 0x000000FFu;
		break;
	case BOTTOM_LEFT:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 3) {
		*(buf + idx + 0) = color >> 16;
		*(buf + idx + 1) = color >> 8;
		*(buf + idx + 2) = color >> 0;
	}
}

static uint16_t get_rgb565_color(enum corner corner, uint8_t grey)
{
	uint16_t color = 0;
	uint16_t grey_5bit;

	switch (corner) {
	case TOP_LEFT:
		color = 0xF800u;
		break;
	case TOP_RIGHT:
		color = 0x07E0u;
		break;
	case BOTTOM_RIGHT:
		color = 0x001Fu;
		break;
	case BOTTOM_LEFT:
		grey_5bit = grey & 0x1Fu;
		/* shift the green an extra bit, it has 6 bits */
		color = grey_5bit << 11 | grey_5bit << (5 + 1) | grey_5bit;
		break;
	}
	return color;
}

static void fill_buffer_rgb565(enum corner corner, uint8_t grey, uint8_t *buf,
			       size_t buf_size)
{
	uint16_t color = get_rgb565_color(corner, grey);

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(buf + idx + 0) = (color >> 8) & 0xFFu;
		*(buf + idx + 1) = (color >> 0) & 0xFFu;
	}
}

static void fill_buffer_bgr565(enum corner corner, uint8_t grey, uint8_t *buf,
			       size_t buf_size)
{
	uint16_t color = get_rgb565_color(corner, grey);

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(uint16_t *)(buf + idx) = color;
	}
}

static void fill_buffer_mono(enum corner corner, uint8_t grey, uint8_t *buf,
			     size_t buf_size)
{
	uint16_t color;

	switch (corner) {
	case BOTTOM_LEFT:
		color = (grey & 0x01u) ? 0xFFu : 0x00u;
		break;
	default:
		color = 0;
		break;
	}

	memset(buf, color, buf_size);
}

void cam_led_test(void){
	bool led_is_on = true;
	const struct device *cam_led = DEVICE_DT_GET(GPIO_LED_CAM);;
	if (!device_is_ready(cam_led)) {
		printk("Error: CAM LED device %s is not ready\n", cam_led->name);
		return;
	}
	int ret = gpio_pin_configure(cam_led, 7, GPIO_OUTPUT_ACTIVE | 0);
	if (ret < 0) {
		LOG_ERR("Error: GPIO pin configure fault!");
		return;
	}
	
	while(1){
		led_is_on = !led_is_on;
		gpio_pin_set(cam_led, 7, (int)led_is_on);
		if(led_is_on)
			k_msleep(1000);
		else
			k_msleep(20);
	}
	return;
}

void lcd_touch_test(){
	size_t x;
	size_t y;
	size_t rect_w;
	size_t rect_h;
	size_t h_step;
	size_t scale;
	size_t grey_count;
	uint8_t *buf;
	int32_t grey_scale_sleep;
	const struct device *display_dev;
	struct display_capabilities capabilities;
	struct display_buffer_descriptor buf_desc;
	size_t buf_size = 0;
	fill_buffer fill_buffer_fnc = NULL;
	LOG_INF("Display sample for %s", DISPLAY_DEV_NAME);

	kscan_init();

	display_dev = device_get_binding(DISPLAY_DEV_NAME);

	if (display_dev == NULL) {
		LOG_ERR("Device %s not found. Aborting sample.",
			DISPLAY_DEV_NAME);
		RETURN_FROM_MAIN(1);
	}

	display_get_capabilities(display_dev, &capabilities);

	if (capabilities.screen_info & SCREEN_INFO_MONO_VTILED) {
		rect_w = 16;
		rect_h = 8;
	} else {
		rect_w = 2;
		rect_h = 1;
	}

	h_step = rect_h;
	scale = (capabilities.x_resolution / 8) / rect_h;

	rect_w *= scale;
	rect_h *= scale;

	if (capabilities.screen_info & SCREEN_INFO_EPD) {
		grey_scale_sleep = 10000;
	} else {
		grey_scale_sleep = 100;
	}

	buf_size = rect_w * rect_h;

	if (buf_size < (capabilities.x_resolution * h_step)) {
		buf_size = capabilities.x_resolution * h_step;
	}

	switch (capabilities.current_pixel_format) {
	case PIXEL_FORMAT_ARGB_8888:
		fill_buffer_fnc = fill_buffer_argb8888;
		buf_size *= 4;
		break;
	case PIXEL_FORMAT_RGB_888:
		fill_buffer_fnc = fill_buffer_rgb888;
		buf_size *= 3;
		break;
	case PIXEL_FORMAT_RGB_565:
		fill_buffer_fnc = fill_buffer_rgb565;
		buf_size *= 2;
		break;
	case PIXEL_FORMAT_BGR_565:
		fill_buffer_fnc = fill_buffer_bgr565;
		buf_size *= 2;
		break;
	case PIXEL_FORMAT_MONO01:
	case PIXEL_FORMAT_MONO10:
		fill_buffer_fnc = fill_buffer_mono;
		buf_size /= 8;
		break;
	default:
		LOG_ERR("Unsupported pixel format. Aborting sample.");
		RETURN_FROM_MAIN(1);
	}

	buf = k_malloc(buf_size);

	if (buf == NULL) {
		LOG_ERR("Could not allocate memory. Aborting sample.");
		RETURN_FROM_MAIN(1);
	}

	(void)memset(buf, 0xFFu, buf_size);

	buf_desc.buf_size = buf_size;
	buf_desc.pitch = capabilities.x_resolution;
	buf_desc.width = capabilities.x_resolution;
	buf_desc.height = h_step;

	for (int idx = 0; idx < capabilities.y_resolution; idx += h_step) {
		display_write(display_dev, 0, idx, &buf_desc, buf);
	}

	buf_desc.pitch = rect_w;
	buf_desc.width = rect_w;
	buf_desc.height = rect_h;

	fill_buffer_fnc(TOP_LEFT, 0, buf, buf_size);
	x = 0;
	y = 0;
	display_write(display_dev, x, y, &buf_desc, buf);

	fill_buffer_fnc(TOP_RIGHT, 0, buf, buf_size);
	x = capabilities.x_resolution - rect_w;
	y = 0;
	display_write(display_dev, x, y, &buf_desc, buf);

	fill_buffer_fnc(BOTTOM_RIGHT, 0, buf, buf_size);
	x = capabilities.x_resolution - rect_w;
	y = capabilities.y_resolution - rect_h;
	display_write(display_dev, x, y, &buf_desc, buf);

	display_blanking_off(display_dev);

	grey_count = 0;
	x = 0;
	y = capabilities.y_resolution - rect_h;

	while (1) {
		fill_buffer_fnc(BOTTOM_LEFT, grey_count, buf, buf_size);
		display_write(display_dev, x, y, &buf_desc, buf);
		++grey_count;
		k_msleep(grey_scale_sleep);
#if CONFIG_TEST
		if (grey_count >= 1024) {
			break;
		}
#endif
	}
	RETURN_FROM_MAIN(0);
}

void adc_test(){
	int err;
	const struct device *dev_adc = DEVICE_DT_GET(ADC_NODE);

	LOG_INF("ADC Sample start");

	if (!device_is_ready(dev_adc)) {
		LOG_INF("ADC device not found");
		return;
	}

	/*
	 * Configure channels individually prior to sampling
	 */
	for (uint8_t i = 0; i < ADC_NUM_CHANNELS; i++) {
		channel_cfg.channel_id = channel_ids[i];

		adc_channel_setup(dev_adc, &channel_cfg);

		sequence.channels |= BIT(channel_ids[i]);
	}

	int32_t adc_vref = adc_ref_internal(dev_adc);

	LOG_INF("adc_vref %d.", adc_vref);

	while (1) {
		/*
		 * Read sequence of channels (fails if not supported by MCU)
		 */
		err = adc_read(dev_adc, &sequence);
		if (err != 0) {
			LOG_ERR("ADC reading failed with error %d.", err);
			return;
		}

		for (uint8_t i = 0; i < ADC_NUM_CHANNELS; i++) {
			int32_t raw_value = sample_buffer[i];

			if (adc_vref > 0) {
				/*
				 * Convert raw reading to millivolts if driver
				 * supports reading of ADC reference voltage
				 */
				int32_t vcc_value = raw_value - 2048;

				adc_raw_to_millivolts(adc_vref, ADC_GAIN_1,
					ADC_RESOLUTION, &vcc_value); // Output 1/8 Vcc
				
				vcc_value *= 8; 
				LOG_INF(" adc(%d)= %d mV", raw_value, vcc_value);
			}
		}

		k_sleep(K_MSEC(1300));
	}
}

static void kb_callback(const struct device *dev, uint32_t row, uint32_t col,
			bool pressed)
{
	ARG_UNUSED(dev);

	LOG_INF("Key value = %d; Unused values = %d Pressed = %u", row, col, pressed);

}
void power_lock_init(void){
	const struct device *power_lock = DEVICE_DT_GET(GPIO_PWR_LOCK);
	if (!device_is_ready(power_lock)) {
		LOG_ERR("Error: power_lock device %s is not ready", power_lock->name);
		return;
	}
	LOG_INF("Get: power_lock device %s", power_lock->name);

	int ret = gpio_pin_configure(power_lock, PIN_PWR_LOCK, GPIO_OUTPUT_ACTIVE | 0);
	if (ret < 0) {
		LOG_ERR("Error: power_lock configure %d", ret);
		return;
	}
	
	ret = gpio_pin_set(power_lock, PIN_PWR_LOCK, 1);
	if (ret < 0) {
		LOG_ERR("Error: power_lock set %d", ret);
		return;
	}
	LOG_INF("Power is on:%d", ret);
}

void main(void)
{
	const struct device *pwm;
	power_lock_init();
	int pri = k_thread_priority_get(k_current_get());
	k_thread_create(&lcdtouch_task_id, lcdtouch_stack_area,
					K_THREAD_STACK_SIZEOF(lcdtouch_stack_area),
					lcd_touch_test, NULL, NULL, NULL, pri, 0, K_NO_WAIT);

	// ADC：Get Vcc Voltage thread
	pri = k_thread_priority_get(k_current_get());
	k_thread_create(&adc_task_id, adc_stack_area,
					K_THREAD_STACK_SIZEOF(adc_stack_area),
					adc_test, NULL, NULL, NULL, pri, 0, K_NO_WAIT);

	#if 0
	k_thread_create(&camled_task_id, camled_stack_area,
					K_THREAD_STACK_SIZEOF(camled_stack_area),
					cam_led_test, NULL, NULL, NULL, pri, 0, K_NO_WAIT);
	#endif


	// PWM Init
	#if 1
	pwm = PWM;
	if (!device_is_ready(pwm)) {
		LOG_ERR("Error: PWM device %s is not ready", pwm->name);
		return;
	}
	struct pwm_csk_config *pwmconfig = (struct pwm_csk_config*)pwm->config;
	LOG_INF("下载模式,5s后变为PWM模式不可下载程序，重新下载需要上电并在5s内进行下载");
	pinmux_pin_set(pwmconfig->pinmux_drv, pwmconfig->pwm_pin, SDIO_PIN_FUNC);	//配成sdio
	k_msleep(5000);
	LOG_INF("PWM模式,需要进入swd模式请输入：swd");
	pinmux_pin_set(pwmconfig->pinmux_drv, pwmconfig->pwm_pin, pwmconfig->pwm_func);		//配成pwm
	int ret;
	ret = pwm_pin_set_nsec(pwm, 1, 20000,  200, 1);
	#endif

	// Keysense init
	const struct device *key_dev = device_get_binding(KSCAN_DEVICE);
	if (!device_is_ready(key_dev)) {
		LOG_ERR("kscan device is not ready");
		return;
	}
	kscan_config(key_dev, kb_callback);
	LOG_INF("key_dev init success!");

	while(1){
		k_msleep(1000);
	}
	return;
}

int swd_mode_shell_handler(const struct shell *shell,
                                 size_t argc, char **argv)
{
    const struct device *pwm = PWM;
        if (!device_is_ready(pwm)) {
                LOG_ERR("Error: PWM device %s is not ready", pwm->name);
                return -1;
        }
        struct pwm_csk_config *pwmconfig = (struct pwm_csk_config*)pwm->config;

        pinmux_pin_set(pwmconfig->pinmux_drv, pwmconfig->pwm_pin, SDIO_PIN_FUNC);

    shell_print(shell, "Enter SWD IO Mode.");

    return 0;
}

SHELL_CMD_ARG_REGISTER(swd, NULL, "swd mode select.", swd_mode_shell_handler, 1, 0);