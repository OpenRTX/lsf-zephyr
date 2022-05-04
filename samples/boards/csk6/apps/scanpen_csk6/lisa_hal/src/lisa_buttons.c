/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *	  1. Redistributions of source code must retain the above copyright
 *		 notice, this list of conditions and the following disclaimer.
 *	  2. Redistributions in binary form must reproduce the above copyright
 *		 notice, this list of conditions and the following disclaimer in the
 *		 documentation and/or other materials provided with the
 *		 distribution.
 *	  3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *		 its contributors may be used to endorse or promote products derived
 *		 from this software without specific prior written permission.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *	OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include <lisa_err.h>

#include <lisa_timer.h>
#include <lisa_time.h>
#include <lisa_thread.h>
#include <lisa_semaphore.h>

#include <lisa_buttons.h>

#include "list.h"
#define TAG "lisa_button"

typedef struct {
	uint32_t id_mask;
	uint32_t repeat_timeout_ms; /* only used in repeat button */
	uint32_t timeout_ms; /* the timeout to trigger the button object */
	uint8_t en; /* enable/disable the button object */
	BUTTON_TYPE func; /* the button type */
	BUTTON_STATE state; /* the button state */
	button_handle handle; /* the button handle */
	struct list_head node; /* all the buttons object will be added in one button list */
} button_obj;

/*static button_impl_t button_impl;*/
static struct list_head buttons_head;
static bool buttons_timer_time_up = 0;
static lisa_timer_t *buttons_timer;
static lisa_thread_t *buttons_thread;
static bool buttons_thread_run_flag = 0;
static uint32_t buttons_thread_stack_size = 0;
static OS_Priority buttons_thread_priority = OS_PRIORITY_NORMAL;
static button_obj *current_tiggered_obj = NULL;
static lisa_semaphore_t *buttons_sem;
static int buttons_st = 0;

#define BUTTONS_THREAD_STACK_SIZE_DEFAULT 512
#define GET_BASE(prt) (button_obj *)container_of(prt, button_obj, handle)

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int val = gpio_pin_get_dt(&button);
	if (val >= 0) {
		gpio_pin_set_dt(&led, val);
	}
	LISA_LOGD(TAG, "Button pressed val %d \n", val);

	buttons_st = val ? 0 : KEY1;

	lisa_semaphore_give(buttons_sem);
}

lisa_err_t zephyr_button_init(void)
{
	int ret;

	if (!device_is_ready(button.port)) {
		LISA_LOGE(TAG, "Error: button device %s is not ready\n", button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LISA_LOGE(TAG, "Error %d: failed to configure %s pin %d\n", ret, button.port->name,
				button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH | GPIO_INT_DEBOUNCE);
	if (ret != 0) {
		LISA_LOGE(TAG, "Error %d: failed to configure interrupt on %s pin %d\n", ret,
				button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LISA_LOGD(TAG, "Set up button at %s pin %d\n", button.port->name, button.pin);

	if (led.port && !device_is_ready(led.port)) {
		LISA_LOGE(TAG, "Error %d: LED device %s is not ready; ignoring it\n", ret, led.port->name);
		led.port = NULL;
	}
	if (led.port) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			LISA_LOGE(TAG, "Error %d: failed to configure LED device %s pin %d\n", ret,
					led.port->name, led.pin);
			led.port = NULL;
		} else {
			LISA_LOGD(TAG, "Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}

	LISA_LOGD(TAG, " button init success\n");
#if 0
	if (led.port) {
		while (1) {
			/* If we have an LED, match its state to the button's. */
			int val = gpio_pin_get_dt(&button);

			if (val >= 0) {
				gpio_pin_set_dt(&led, val);
			}
			k_msleep(SLEEP_TIME_MS);
		}
	}
#endif
}

static void short_button_release(int buttons_state)
{
	/* a button can be released only when the button state is all 0,
	 * in other cases, it is determined that it is interrupted in midway. Only when the
	 * button has been released, the callback function can be called.
	 */
	if (buttons_state == ALL_BUTTONS_RELEASE) {
		if (current_tiggered_obj->state == PRESS) {
			current_tiggered_obj->state = RELEASE;
			if (current_tiggered_obj->handle.cb)
				current_tiggered_obj->handle.cb(
						current_tiggered_obj->state, current_tiggered_obj->handle.arg);
		}
	}

	/* Whether it is a complete release or a middle interruption,
	 * the button state must set to an invalid state.
	 */
	current_tiggered_obj->state = INVALID_STA;
	/* the pointer can only be reset when the buttons are fully released. */
	if (buttons_state == ALL_BUTTONS_RELEASE) current_tiggered_obj = NULL;
}

static void long_button_release(int buttons_state)
{
	/* whether the timer is active or not, stop the timer and reset the flag. */
	lisa_timer_stop(buttons_timer);
	buttons_timer_time_up = false;

	if (current_tiggered_obj->state == PRESS || current_tiggered_obj->state == REPEAT_PRESS) {
		current_tiggered_obj->state = RELEASE;
		if (current_tiggered_obj->handle.cb)
			current_tiggered_obj->handle.cb(
					current_tiggered_obj->state, current_tiggered_obj->handle.arg);
	}

	current_tiggered_obj->state = INVALID_STA;
	if (buttons_state == ALL_BUTTONS_RELEASE) current_tiggered_obj = NULL;
}

static void short_long_button_release(int buttons_state)
{
	/* if the timer is active, that mean press_time < timeout_ms. */
	// if (OS_TimerIsActive(buttons_timer)) {
	if (lisa_timer_isactive(buttons_timer)) {
		/* stop the timer and set the RELEASE state, call the callback function. */
		lisa_timer_stop(buttons_timer);
		buttons_timer_time_up = false;
		current_tiggered_obj->state = RELEASE;
		if (current_tiggered_obj->handle.cb)
			current_tiggered_obj->handle.cb(
					current_tiggered_obj->state, current_tiggered_obj->handle.arg);
	}
	/* else the timer is not active, that mean press_time >= timeout_ms. */
	else if (current_tiggered_obj->state == PRESS) {
		current_tiggered_obj->state = REPEAT_RELEASE;
		if (current_tiggered_obj->handle.cb)
			current_tiggered_obj->handle.cb(
					current_tiggered_obj->state, current_tiggered_obj->handle.arg);
	}

	current_tiggered_obj->state = INVALID_STA;
	if (buttons_state == ALL_BUTTONS_RELEASE) current_tiggered_obj = NULL;
}

static void repeat_long_button_release(int buttons_state)
{
	long_button_release(buttons_state);
}

static void combined_long_button_release(int buttons_state)
{
	lisa_timer_stop(buttons_timer);
	buttons_timer_time_up = false;

	if (current_tiggered_obj->state == PRESS) {
		current_tiggered_obj->state = RELEASE;
		if (current_tiggered_obj->handle.cb)
			current_tiggered_obj->handle.cb(
					current_tiggered_obj->state, current_tiggered_obj->handle.arg);
	}

	current_tiggered_obj->state = INVALID_STA;
	if (buttons_state == ALL_BUTTONS_RELEASE) current_tiggered_obj = NULL;
}

static void short_button_press(button_obj *obj)
{
	/* if not NULL, that mean the previous button has not been fully released. */
	if (current_tiggered_obj) return;

	/* set PRESS state of short button */
	obj->state = PRESS;
	/* record the current triggered button object */
	current_tiggered_obj = obj;
}

static void long_button_press(button_obj *obj)
{
	lisa_err_t sta;

	/* if not NULL, that mean the previous button has not been fully released. */
	if (current_tiggered_obj) return;

	/* set the time */
	sta = lisa_timer_change_period(buttons_timer, obj->timeout_ms);
	if (sta != LISA_OK) {
		LISA_LOGE(TAG, "button timer change period error");
		return;
	}
	sta = lisa_timer_start(buttons_timer);
	if (sta != LISA_OK) {
		LISA_LOGE(TAG, "button timer start error");
		return;
	}

	/* button has not been triggerd, the state should be INVALID_STA. */
	obj->state = INVALID_STA;
	/* record the current triggered button object */
	current_tiggered_obj = obj;
}

static void short_long_button_press(button_obj *obj)
{
	long_button_press(obj);
}

static void combined_long_button_press(button_obj *obj)
{
	lisa_err_t sta;
	int buttons_state = obj->id_mask;

	/* The combination long button is a subset of long press buttons.
	 * Because all the buttons can not be pressed at the same time,
	 * so the combination button can interrupt long press buttton and short press button.
	 */
	/* if one button object has been triggered, then release it, interrupt it. */
	if (current_tiggered_obj && current_tiggered_obj->func == SHORT_BUTTON)
		short_button_release(buttons_state);
	else if (current_tiggered_obj && current_tiggered_obj->func == LONG_BUTTON)
		long_button_release(buttons_state);
	else if (current_tiggered_obj && current_tiggered_obj->func == SHORT_LONG_BUTTON)
		short_long_button_release(buttons_state);
	else if (current_tiggered_obj && current_tiggered_obj->func == COMBINED_LONG_BUTTON) {
		combined_long_button_release(buttons_state);
		/* combination button can not interrupt combination button,
		 * and the combination button must wait all the buttons released.
		 */
		return;
	}

	/* set the time */
	sta = lisa_timer_change_period(buttons_timer, obj->timeout_ms);
	if (sta != LISA_OK) {
		LISA_LOGE(TAG, "button timer change period error");
		return;
	}
	sta = lisa_timer_start(buttons_timer);
	if (sta != LISA_OK) {
		LISA_LOGE(TAG, "button timer start error");
		return;
	}

	/* button has not been triggerd, the state should be INVALID_STA. */
	obj->state = INVALID_STA;
	/* record the current triggered button object */
	current_tiggered_obj = obj;
}

static void repeat_long_button_press(button_obj *obj)
{
	long_button_press(obj);
}

static void button_timer_event(void)
{
	lisa_err_t sta;

	if (!current_tiggered_obj) return;

	/* if current button is LONG_BUTTON/SHORT_LONG_BUTTON/COMBINED_LONG_BUTTON,
	 * button timer time's up mean that the press time long enough than timeout_ms.
	 * so the button state should be set PRESS.
	 */
	if (current_tiggered_obj->func == LONG_BUTTON ||
			current_tiggered_obj->func == SHORT_LONG_BUTTON ||
			current_tiggered_obj->func == COMBINED_LONG_BUTTON) {
		/* set the state */
		current_tiggered_obj->state = PRESS;
		/* call the callback function */
		if (current_tiggered_obj->handle.cb)
			current_tiggered_obj->handle.cb(
					current_tiggered_obj->state, current_tiggered_obj->handle.arg);
	}
	/* if the button is REPEAT_LONG_BUTTON */
	else if (current_tiggered_obj->func == REPEAT_LONG_BUTTON) {
		/* if the button state is INVALID_STA, that mean it is the first time trigger the button. */
		if (current_tiggered_obj->state == INVALID_STA) /* set the PRESS state */
			current_tiggered_obj->state = PRESS;
		/* if the state is PRESS/REPEAT_PRESS, that mean it is not the first time trigger the
		 * button. */
		else if (current_tiggered_obj->state == PRESS ||
				 current_tiggered_obj->state == REPEAT_PRESS)
			/* set the REPEAT_PRESS state */
			current_tiggered_obj->state = REPEAT_PRESS;

		/* call the callback function */
		if (current_tiggered_obj->handle.cb)
			current_tiggered_obj->handle.cb(
					current_tiggered_obj->state, current_tiggered_obj->handle.arg);
		/* whether it is the first time trigger the button or not, the timer should be changed. */
		sta = lisa_timer_change_period(buttons_timer, current_tiggered_obj->repeat_timeout_ms);
		if (sta != LISA_OK) {
			LISA_LOGE(TAG, "button timer change period error");
			return;
		}
		sta = lisa_timer_start(buttons_timer);
		if (sta != LISA_OK) {
			LISA_LOGE(TAG, "button timer start error");
			return;
		}
	}
}

static void release_current_tiggered_obj(int buttons_state)
{
	if (!current_tiggered_obj) return;

	if (current_tiggered_obj->func == SHORT_BUTTON)
		short_button_release(buttons_state);
	else if (current_tiggered_obj->func == LONG_BUTTON)
		long_button_release(buttons_state);
	else if (current_tiggered_obj->func == SHORT_LONG_BUTTON)
		short_long_button_release(buttons_state);
	else if (current_tiggered_obj->func == COMBINED_LONG_BUTTON)
		combined_long_button_release(buttons_state);
	else if (current_tiggered_obj->func == REPEAT_LONG_BUTTON)
		repeat_long_button_release(buttons_state);
}

static button_obj *get_buttons_obj(uint32_t id_mask)
{
	button_obj *obj;

	if (list_empty(&buttons_head)) return NULL;

	list_for_each_entry(obj, &buttons_head, node) if (obj->id_mask == id_mask) return obj;

	return NULL;
}

static void buttons_process_thread(void *arg)
{
	int buttons_state = 0;
	int prev_buttons_state = 0;
	button_obj *obj;

	while (buttons_thread_run_flag) {
		/* wait a button trigger or button timer time is up */
		lisa_semaphore_take(buttons_sem, LISA_WAIT_FOREVER);

		/* if button timer time is up, then process the timer event. */
		if (buttons_timer_time_up) {
			button_timer_event();
			buttons_timer_time_up = false;
			continue;
		}

		/* get all the buttons' state form low levle buttons */
		// TODO
		//		if (button_impl.low_level_get_state)
		//			buttons_state = button_impl.low_level_get_state();
		//		else
		//			buttons_state = 0;
		buttons_state = buttons_st;

		/* if the buttons state not change, then continue
		 * sometimes the AD buttons will trigger by mistake.
		 */
		if (prev_buttons_state == buttons_state) continue;
		prev_buttons_state = buttons_state;
#if BUTTON_DBG
		char s[33] = {0};
		int n = buttons_state;
		for (int i = 31; i >= 0; i--)
			s[31 - i] = (n & (1 << i)) == 0 ? '0' : '1';
		BUTTON_DEBUG("%s", s);
#endif
		/* get button's object based on state */
		obj = get_buttons_obj(buttons_state);

		/* if not NULL, that mean one button object has been pressed,
		 * must release it or let it in invalid state first.
		 */
		if (current_tiggered_obj) release_current_tiggered_obj(buttons_state);

		/* if find one button object, then press and record it. */
		if (obj) {
			if (!obj->en) continue;
			if (obj->func == SHORT_BUTTON)
				short_button_press(obj);
			else if (obj->func == LONG_BUTTON)
				long_button_press(obj);
			else if (obj->func == SHORT_LONG_BUTTON)
				short_long_button_press(obj);
			else if (obj->func == COMBINED_LONG_BUTTON)
				combined_long_button_press(obj);
			else if (obj->func == REPEAT_LONG_BUTTON)
				repeat_long_button_press(obj);
		}
	}

	lisa_thread_delete(buttons_thread);
}

static void buttons_timer_cb(void *arg)
{
	/* release the semaphore, let the buttons thread running. */
	// if (button_impl.low_level_release_semaphore) button_impl.low_level_release_semaphore();
	lisa_semaphore_give(buttons_sem);
	buttons_timer_time_up = true;
}

/**
 * @brief Set the stack size of the buttons thread.
 * @note If the callback function of buttons need lots of stacks, then need
 *       add the buttons thread's stack.
 * @param priority: The stack size of buttons thread.
 * @retval void.
 */
void buttons_set_thread_stack_size(uint32_t size)
{
	if (size > 0) buttons_thread_stack_size = size;
}

/**
 * @brief Set the priority of the buttons thread.
 * @note none.
 * @param priority: The priority of buttons thread.
 * @retval void.
 */
void buttons_set_thread_priority(uint32_t priority)
{
	if (priority >= OS_PRIORITY_IDLE && priority <= OS_PRIORITY_REAL_TIME)
		buttons_thread_priority = priority;
}

/**
 * @brief Buttons initialize.
 * @note Initialize the button module.
 * @param impl: The interface that the low level buttons pass to the buttons module,
 *              which is used to operate the low level buttons.
 * @retval 0: success, -1: fail
 */
int buttons_init(void)
{
	int ret;
	lisa_err_t sta;

	/*
	 *if (!impl) {
	 *    LISA_LOGE(TAG, "buttons impl is NULL");
	 *    return -1;
	 *}
	 */
	/*memcpy(&button_impl, impl, sizeof(button_impl));*/

	buttons_sem = lisa_semaphore_create(1);

	zephyr_button_init();
	/* initialize the low level buttons */
	/*
	 *if (button_impl.low_level_init) {
	 *    ret = button_impl.low_level_init();
	 *    if (ret != 0) {
	 *        LISA_LOGE(TAG, "buttons low level init err");
	 *        return -1;
	 *    }
	 *}
	 */

	buttons_thread_run_flag = 1;
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "button_thread";
	thread_attr.stack_size = 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	buttons_thread = lisa_thread_create(&thread_attr, buttons_process_thread, NULL);
	if (buttons_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: spi_task_protocol_thread failed");
		return;
	}

	/* create timer for buttons */
	buttons_timer = lisa_timer_create(OS_TIMER_ONCE, buttons_timer_cb, NULL, LISA_WAIT_FOREVER);
	if (buttons_timer == NULL) {
		LISA_LOGE(TAG, "buttons timer create error");
		return -1;
	}

	INIT_LIST_HEAD(&buttons_head);

	return 0;
}

/**
 * @brief Buttons deinitialize.
 * @note Deinitialize the buttons module.
 *       The interface will delete and free all the buttons objects.
 * @param void
 * @retval 0: success, -1: fail
 */
int buttons_deinit(void)
{
	button_obj *obj_t;

	if (list_empty(&buttons_head)) return 0;

	buttons_thread_run_flag = 0;
	/* release the semaphore,
	 * prevent the buttons thread from waiting for the semaphore all the time.
	 */
	// if (button_impl.low_level_release_semaphore) button_impl.low_level_release_semaphore();
	lisa_semaphore_give(buttons_sem);

	/* waiting for the buttons thread delete */
	//	while (OS_ThreadIsValid(&buttons_thread))
	//		OS_MSleep(1);
	lisa_thread_delay(1);

	/* delete and free all the button objects */
	while (!list_empty(&buttons_head)) {
		obj_t = list_first_entry(&buttons_head, button_obj, node);
		list_del(&obj_t->node);
		free(obj_t);
	}

	lisa_semaphore_delete(buttons_sem);
	lisa_timer_delete(buttons_timer);

	// if (button_impl.low_level_deinit) button_impl.low_level_deinit();

	/*memset(&button_impl, 0, sizeof(button_impl));*/

	return 0;
}

static void button_start(button_handle *handle)
{
	button_obj *obj = GET_BASE(handle);

	obj->en = 1;
}

static void button_stop(button_handle *handle)
{
	button_obj *obj = GET_BASE(handle);

	obj->en = 0;
}

static int button_get_state(button_handle *handle)
{
	button_obj *obj = GET_BASE(handle);

	return obj->state == PRESS ? PRESS : RELEASE;
}

static int button_destroy(button_handle *handle)
{
	button_obj *obj = GET_BASE(handle);
	button_obj *obj_t;

	if (obj == current_tiggered_obj) {
		LISA_LOGE(TAG, "the button is working, can not destroy");
		return -1;
	}

	if (list_empty(&buttons_head)) return -1;

	/* delete and free the button object from buttons list head */
	list_for_each_entry(obj_t, &buttons_head, node)
	{
		if (obj_t == obj) {
			list_del(&obj->node);
			free(obj);
			return 0;
		}
	}

	return 0;
}

/**
 * @brief Create one short button.
 * @note The short button has only the RELEASE state. When it pressed, the
 *       callback function of the button object will not be called.
 *       When it released, the callback function will be called and pass the
 *       RELEASE state immediately.
 * @param id_mask: the button id
 * @retval The button object handle, NULL if create failed.
 */
button_handle *create_short_button(uint32_t id_mask)
{
	uint32_t num = id_mask;
	int count = 0;

	/* record how many '1' in id_mask */
	while (num) {
		if (num & 1) count++;
		num >>= 1;
	}
	/* not support multiple buttons */
	if (count != 1) {
		LISA_LOGE(TAG, "short button must only 1 buttons");
		return NULL;
	}

	button_obj *obj;

	/* check if the button object exists */
	obj = get_buttons_obj(id_mask);
	if (obj) {
		LISA_LOGE(TAG, "button %x object has already exist", id_mask);
		return NULL;
	}

	obj = (button_obj *)malloc(sizeof(button_obj));
	if (obj == NULL) {
		LISA_LOGE(TAG, "button object malloc error");
		return NULL;
	}

	memset(obj, 0, sizeof(button_obj));
	obj->state = INVALID_STA;
	obj->id_mask = id_mask;
	obj->func = SHORT_BUTTON;
	obj->handle.start = button_start;
	obj->handle.stop = button_stop;
	obj->handle.get_state = button_get_state;
	obj->handle.destroy = button_destroy;

	/* add button object to buttons list head */
	list_add_tail(&obj->node, &buttons_head);

	return &obj->handle;
}

/**
 * @brief Create one long button.
 * @note The long button has the PRESS/RELEASE state. When it pressed for
 *       more than timeout_ms milliseconds, the callback function of the button
 *       object will be called and pass the PRESS state. When it released, the
 *       callback function will be called and pass the RELEASE state. If release
 *       the button earlier than timeout_ms, nothing will happen, the callback
 *       function will not be called.
 * @param id_mask: the button id
 *        timeout_ms：the timeout to trigger PRESS state.
 * @retval The button object handle, NULL if create failed.
 */
button_handle *create_long_button(uint32_t id_mask, uint32_t timeout_ms)
{
	uint32_t num = id_mask;
	int count = 0;

	while (num) {
		if (num & 1) count++;
		num >>= 1;
	}

	if (count != 1) {
		LISA_LOGE(TAG, "long button must only 1 buttons");
		return NULL;
	}

	button_obj *obj;

	obj = get_buttons_obj(id_mask);
	if (obj) {
		LISA_LOGE(TAG, "button %x object has already exist", id_mask);
		return NULL;
	}

	obj = (button_obj *)malloc(sizeof(button_obj));
	if (obj == NULL) {
		LISA_LOGE(TAG, "button object malloc error");
		return NULL;
	}

	memset(obj, 0, sizeof(button_obj));
	obj->state = INVALID_STA;
	obj->id_mask = id_mask;
	obj->func = LONG_BUTTON;
	obj->timeout_ms = timeout_ms;
	obj->handle.start = button_start;
	obj->handle.stop = button_stop;
	obj->handle.get_state = button_get_state;
	obj->handle.destroy = button_destroy;

	list_add_tail(&obj->node, &buttons_head);

	return &obj->handle;
}

/**
 * @brief Create one short_long button.
 * @note The short_long button has the PRESS/RELEASE/REPEAT_RELEASE state.
 *       PRESS: if the button pressed for more than timeout_ms(press_time >= timeout_ms),
 *              this state will be passed to callback function.
 *       RELEASE: if the button released earlier than timeout_ms(press_time < timeout_ms),
 *                this state will be passed to callback function.
 *       REPEAT_RELEASE: if the button pressed more than timeout_ms and released(press_time >=
 * timeout_ms), this state will be passed to callback function.
 * @param id_mask: the button id
 *        timeout_ms：the timeout to trigger PRESS state.
 * @retval The button object handle, NULL if create failed.
 */
button_handle *create_short_long_button(uint32_t id_mask, uint32_t timeout_ms)
{
	uint32_t num = id_mask;
	int count = 0;

	while (num) {
		if (num & 1) count++;
		num >>= 1;
	}

	if (count != 1) {
		LISA_LOGE(TAG, "short and long button must only 1 buttons");
		return NULL;
	}

	button_obj *obj;

	obj = get_buttons_obj(id_mask);
	if (obj) {
		LISA_LOGE(TAG, "button %x object has already exist", id_mask);
		return NULL;
	}

	obj = (button_obj *)malloc(sizeof(button_obj));
	if (obj == NULL) {
		LISA_LOGE(TAG, "button object malloc error");
		return NULL;
	}

	memset(obj, 0, sizeof(button_obj));
	obj->state = INVALID_STA;
	obj->id_mask = id_mask;
	obj->func = SHORT_LONG_BUTTON;
	obj->timeout_ms = timeout_ms;
	obj->handle.start = button_start;
	obj->handle.stop = button_stop;
	obj->handle.get_state = button_get_state;
	obj->handle.destroy = button_destroy;

	list_add_tail(&obj->node, &buttons_head);

	return &obj->handle;
}

/**
 * @brief Create one combined long button.
 * @note The combined button has the PRESS/RELEASE state. Combined long
 *       button support multiple buttons, for example, key1|key2|key3, when all
 *       the three buttons are pressed and more than timeout_ms(all_press_time >= timeout_ms),
 *       the PRESS state will be passed to callback function. If any button released,
 *       the RELEASE state will be passed to callback function. If any button released
 *       earlier than timeout_ms(all_press_time < timeout_ms), nothing will happen.
 * @param id_mask: the button id, support multiple buttons(KEY1|KEY2|KEY3).
 *        timeout_ms：the timeout to trigger PRESS state.
 * @retval The button object handle, NULL if create failed.
 */
button_handle *create_combined_long_button(uint32_t id_mask, uint32_t timeout_ms)
{
	uint32_t num = id_mask;
	int count = 0;

	while (num) {
		if (num & 1) count++;
		num >>= 1;
	}

	if (count < 2) {
		LISA_LOGE(TAG, "combined button must have more than 2 buttons");
		return NULL;
	}

	button_obj *obj;

	obj = get_buttons_obj(id_mask);
	if (obj) {
		LISA_LOGE(TAG, "button %x object has already exist", id_mask);
		return NULL;
	}

	obj = (button_obj *)malloc(sizeof(button_obj));
	if (obj == NULL) {
		LISA_LOGE(TAG, "button object malloc error");
		return NULL;
	}

	memset(obj, 0, sizeof(button_obj));
	obj->state = INVALID_STA;
	obj->id_mask = id_mask;
	obj->func = COMBINED_LONG_BUTTON;
	obj->timeout_ms = timeout_ms;
	obj->handle.start = button_start;
	obj->handle.stop = button_stop;
	obj->handle.get_state = button_get_state;
	obj->handle.destroy = button_destroy;

	list_add_tail(&obj->node, &buttons_head);

	return &obj->handle;
}

/**
 * @brief Create one repeat long button.
 * @note The repeat long button has the PRESS/RELEASE/REPEAT_PRESS state.
 *       PRESS: if the button pressed for more than timeout_ms(press_time >= timeout_ms),
 *              this state will be passed to callback function.
 *       RELEASE: if the button pressed for more than timeout_ms and released,
 *                this state will be passed to callback function.
 *       REPEAT_PRESS: if the button pressed all the time, the callback function
 *                     will be called in every repeat_timeout_ms, and REPEAT_PRESS
 *                     will be passed to callback.
 * @param id_mask: the button id.
 *        timeout_ms：the timeout to trigger PRESS state.
 *        repeat_timeout_ms: the timeout to trigger REPEAT_PRESS state.
 * @retval The button object handle, NULL if create failed.
 */
button_handle *create_repeat_long_button(
		uint32_t id_mask, uint32_t timeout_ms, uint32_t repeat_timeout_ms)
{
	uint32_t num = id_mask;
	int count = 0;

	while (num) {
		if (num & 1) count++;
		num >>= 1;
	}

	if (count != 1) {
		LISA_LOGE(TAG, "repeat long button must only 1 buttons");
		return NULL;
	}

	button_obj *obj;

	obj = get_buttons_obj(id_mask);
	if (obj) {
		LISA_LOGE(TAG, "button %x object has already exist", id_mask);
		return NULL;
	}

	obj = (button_obj *)malloc(sizeof(button_obj));
	if (obj == NULL) {
		LISA_LOGE(TAG, "button object malloc error");
		return NULL;
	}

	memset(obj, 0, sizeof(button_obj));
	obj->state = INVALID_STA;
	obj->id_mask = id_mask;
	obj->func = REPEAT_LONG_BUTTON;
	obj->timeout_ms = timeout_ms;
	obj->repeat_timeout_ms = repeat_timeout_ms;
	obj->handle.start = button_start;
	obj->handle.stop = button_stop;
	obj->handle.get_state = button_get_state;
	obj->handle.destroy = button_destroy;

	list_add_tail(&obj->node, &buttons_head);

	return &obj->handle;
}

