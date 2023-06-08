/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_pcf8563

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(pcf8563, CONFIG_RTC_LOG_LEVEL);

/* PCF8563 register addresses */
#define PCF8563_CONTROL_STATUS1	0x00U
#define PCF8563_CONTROL_STATUS2	0x01U
#define PCF8563_SECONDS			0x02U
#define PCF8563_MINUTES			0x03U
#define PCF8563_HOURS			0x04U
#define PCF8563_DAYS			0x05U
#define PCF8563_WEEKDAYS		0x06U
#define PCF8563_CENTURY_MONTHS	0x07U
#define PCF8563_YEARS			0x08U

#define PCF8563_MINUTE_ALARM	0x09U
#define PCF8563_HOUR_ALARM		0x0AU
#define PCF8563_DAY_ALARM		0x0BU
#define PCF8563_WEEKDAY_ALARM	0x0CU

#define PCF8563_TIMER_CONTROL	0x0EU
#define PCF8563_TIMER 			0x0FU

/* Control register bits */
#define PCF8563_CONTROL_STATUS1_TEST1	BIT(7)
#define PCF8563_CONTROL_STATUS1_STOP	BIT(5)
#define PCF8563_CONTROL_STATUS1_TESTC	BIT(3)
// #define PCF8563_CONTROL_STATUS1_N6		BIT(6)
// #define PCF8563_CONTROL_STATUS1_N4	  	BIT(4)
// #define PCF8563_CONTROL_STATUS1_N2	  	BIT(2)
// #define PCF8563_CONTROL_STATUS1_N1	  	BIT(1)
// #define PCF8563_CONTROL_STATUS1_N0	  	BIT(0)

#define PCF8563_CONTROL_STATUS2_N7		BIT(7)
#define PCF8563_CONTROL_STATUS2_N6		BIT(6)
#define PCF8563_CONTROL_STATUS2_N5		BIT(5)
#define PCF8563_CONTROL_STATUS2_TI_TP	BIT(4)
#define PCF8563_CONTROL_STATUS2_AF		BIT(3)
#define PCF8563_CONTROL_STATUS2_TF		BIT(2)
#define PCF8563_CONTROL_STATUS2_AIE		BIT(1)
#define PCF8563_CONTROL_STATUS2_TIE		BIT(0)

/* Time and date register bits */
#define PCF8563_SECONDS_VL     BIT(7)
#define PCF8563_SECONDS_MASK   GENMASK(6, 0)
#define PCF8563_MINUTES_MASK   GENMASK(6, 0)

#define PCF8563_HOURS_AMPM     BIT(5)
#define PCF8563_HOURS_12H_MASK GENMASK(4, 0)
#define PCF8563_HOURS_24H_MASK GENMASK(5, 0)

#define PCF8563_DAYS_MASK      GENMASK(5, 0)
#define PCF8563_WEEKDAYS_MASK  GENMASK(2, 0)
#define PCF8563_MONTHS_MASK    GENMASK(4, 0)
#define PCF8563_MONTHS_CENTURY_MASK GENMASK(7, 7)
#define PCF8563_YEARS_MASK     GENMASK(7, 0)

/* Alarm register bits */
#define PCF8563_MINUTE_ALARM_AEN_M  BIT(7)
#define PCF8563_MINUTE_ALARM_MASK   GENMASK(6, 0)
#define PCF8563_HOUR_ALARM_AEN_H    BIT(7)
#define PCF8563_HOUR_ALARM_AMPM	    BIT(5)
#define PCF8563_HOUR_ALARM_12H_MASK GENMASK(4, 0)
#define PCF8563_HOUR_ALARM_24H_MASK GENMASK(5, 0)
#define PCF8563_DAY_ALARM_AEN_D	    BIT(7)
#define PCF8563_DAY_ALARM_MASK	    GENMASK(5, 0)
#define PCF8563_WEEKDAY_ALARM_AEN_W BIT(7)
#define PCF8563_WEEKDAY_ALARM_MASK  GENMASK(5, 0)

/* Timer register bits */
#define PCF8563_TMR_CLKOUT_CTRL_TAM	 BIT(7)
#define PCF8563_TMR_CLKOUT_CTRL_TBM	 BIT(6)
#define PCF8563_TMR_CLKOUT_CTRL_COF_MASK GENMASK(5, 3)
#define PCF8563_TMR_CLKOUT_CTRL_TAC_MASK GENMASK(2, 1)
#define PCF8563_TMR_CLKOUT_CTRL_TBC	 BIT(0)
#define PCF8563_TMR_A_FREQ_CTRL_TAQ_MASK GENMASK(2, 0)
#define PCF8563_TMR_A_REG_T_A_MASK	 GENMASK(7, 0)
#define PCF8563_TMR_B_FREQ_CTRL_TBW_MASK GENMASK(6, 4)
#define PCF8563_TMR_B_FREQ_CTRL_TBQ_MASK GENMASK(2, 0)
#define PCF8563_TMR_B_REG_T_B_MASK	 GENMASK(7, 0)

/* Offset register bits */
#define PCF8563_OFFSET_MODE BIT(7)
#define PCF8563_OFFSET_MASK GENMASK(6, 0)

/* RTC alarm time fields supported by the PCF8563 */
#define PCF8563_RTC_ALARM_TIME_MASK                                                                \
	(RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_HOUR | RTC_ALARM_TIME_MASK_MONTHDAY |    \
	 RTC_ALARM_TIME_MASK_WEEKDAY)

/* The PCF8563 only supports two-digit years, calculate offset to use */
#define PCF8563_YEARS_OFFSET (2000 - 1900)

/* The PCF8563 enumerates months 1 to 12, RTC API uses 0 to 11 */
#define PCF8563_MONTHS_OFFSET 1

/* Helper macro to guard int1-gpios related code */
#if DT_HAS_COMPAT_STATUS_OKAY(int1_gpios) &&                                                \
	(defined(CONFIG_RTC_ALARM) || defined(CONFIG_RTC_UPDATE))
#define PCF8563_INT1_GPIOS_IN_USE 1
#endif

struct pcf8563_config {
	const struct i2c_dt_spec i2c;
#ifdef PCF8563_INT1_GPIOS_IN_USE
	struct gpio_dt_spec int1;
#endif /* PCF8563_INT1_GPIOS_IN_USE */
	uint8_t cof;
	uint8_t pm;
	bool cap_sel;
	bool wakeup_source;
};

struct pcf8563_data {
	struct k_mutex lock;
#if PCF8563_INT1_GPIOS_IN_USE
	struct gpio_callback int1_callback;
	struct k_thread int1_thread;
	struct k_sem int1_sem;

	K_KERNEL_STACK_MEMBER(int1_stack, CONFIG_RTC_PCF8563_THREAD_STACK_SIZE);
#ifdef CONFIG_RTC_ALARM
	rtc_alarm_callback alarm_callback;
	void *alarm_user_data;
#endif /* CONFIG_RTC_ALARM */
#ifdef CONFIG_RTC_UPDATE
	rtc_update_callback update_callback;
	void *update_user_data;
#endif /* CONFIG_RTC_UPDATE */
#endif /* PCF8563_INT1_GPIOS_IN_USE */
};

static int pcf8563_read_regs(const struct device *dev, uint8_t addr, void *buf, size_t len)
{
	const struct pcf8563_config *config = dev->config;
	int err;

	err = i2c_write_read_dt(&config->i2c, &addr, sizeof(addr), buf, len);
	if (err != 0) {
		LOG_ERR("failed to read reg addr 0x%02x, len %d (err %d)", addr, len, err);
		return err;
	}

	return 0;
}

static int pcf8563_read_reg8(const struct device *dev, uint8_t addr, uint8_t *val)
{
	return pcf8563_read_regs(dev, addr, val, sizeof(*val));
}

static int pcf8563_write_regs(const struct device *dev, uint8_t addr, void *buf, size_t len)
{
	const struct pcf8563_config *config = dev->config;
	uint8_t block[sizeof(addr) + len];
	int err;

	block[0] = addr;
	memcpy(&block[1], buf, len);

	err = i2c_write_dt(&config->i2c, block, sizeof(block));
	if (err != 0) {
		LOG_ERR("failed to write reg addr 0x%02x, len %d (err %d)", addr, len, err);
		return err;
	}

	return 0;
}

static int pcf8563_write_reg8(const struct device *dev, uint8_t addr, uint8_t val)
{
	return pcf8563_write_regs(dev, addr, &val, sizeof(val));
}

static int pcf8563_write_stop_bit_unlocked(const struct device *dev, bool value)
{
	uint8_t control_status1;
	int err;

	err = pcf8563_read_reg8(dev, PCF8563_CONTROL_STATUS1, &control_status1);
	if (err != 0) {
		return err;
	}

	if (value) {
		control_status1 |= PCF8563_CONTROL_STATUS1_STOP;
	} else {
		control_status1 &= ~(PCF8563_CONTROL_STATUS1_STOP);
	}

	err = pcf8563_write_reg8(dev, PCF8563_CONTROL_STATUS1, control_status1);
	if (err != 0) {
		return err;
	}

	return 0;
}

#if PCF8563_INT1_GPIOS_IN_USE
static int pcf8563_int1_enable_unlocked(const struct device *dev, bool enable)
{
	const struct pcf8563_config *config = dev->config;
	uint8_t tmr_clkout_ctrl;
	int err;

	if (!config->wakeup_source) {
		/* Only change COF if not configured as wakeup-source */
		err = pcf8563_read_reg8(dev, PCF8563_TMR_CLKOUT_CTRL, &tmr_clkout_ctrl);
		if (err != 0) {
			return err;
		}

		if (enable) {
			/* Disable CLKOUT */
			tmr_clkout_ctrl |= PCF8563_TMR_CLKOUT_CTRL_COF_MASK;
		} else if (!config->wakeup_source) {
			/* Enable CLKOUT */
			tmr_clkout_ctrl &= ~(PCF8563_TMR_CLKOUT_CTRL_COF_MASK);
			tmr_clkout_ctrl |=
				FIELD_PREP(PCF8563_TMR_CLKOUT_CTRL_COF_MASK, config->cof);
		}

		err = pcf8563_write_reg8(dev, PCF8563_TMR_CLKOUT_CTRL, tmr_clkout_ctrl);
		if (err != 0) {
			return err;
		}
	}

	/* Use edge interrupts to avoid multiple GPIO IRQs while servicing the IRQ in the thread */
	err = gpio_pin_interrupt_configure_dt(&config->int1,
					      enable ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_DISABLE);
	if (err != 0) {
		LOG_ERR("failed to %s GPIO IRQ (err %d)", enable ? "enable" : "disable", err);
		return err;
	}

	return 0;
}

static void pcf8563_int1_thread(const struct device *dev)
{
	struct pcf8563_data *data = dev->data;
	rtc_alarm_callback alarm_callback = NULL;
	void *alarm_user_data = NULL;
	rtc_update_callback update_callback = NULL;
	void *update_user_data = NULL;
	uint8_t control_2;
	int err;

	while (true) {
		k_sem_take(&data->int1_sem, K_FOREVER);
		k_mutex_lock(&data->lock, K_FOREVER);

		err = pcf8563_read_reg8(dev, PCF8563_CONTROL_2, &control_2);
		if (err != 0) {
			goto unlock;
		}

#ifdef CONFIG_RTC_ALARM
		if ((control_2 & PCF8563_CONTROL_2_AF) != 0 && data->alarm_callback != NULL) {
			control_2 &= ~(PCF8563_CONTROL_2_AF);
			alarm_callback = data->alarm_callback;
			alarm_user_data = data->alarm_user_data;
		}
#endif /* CONFIG_RTC_ALARM */

#ifdef CONFIG_RTC_UPDATE
		if ((control_2 & PCF8563_CONTROL_2_SF) != 0) {
			control_2 &= ~(PCF8563_CONTROL_2_SF);
			update_callback = data->update_callback;
			update_user_data = data->update_user_data;
		}
#endif /* CONFIG_RTC_UPDATE */

		control_2 |= PCF8563_CONTROL_2_CTAF | PCF8563_CONTROL_2_CTBF;

		err = pcf8563_write_reg8(dev, PCF8563_CONTROL_2, control_2);
		if (err != 0) {
			goto unlock;
		}

		/* Check if interrupt occurred between CONTROL_2 read/write */
		err = pcf8563_read_reg8(dev, PCF8563_CONTROL_2, &control_2);
		if (err != 0) {
			goto unlock;
		}

		if (((control_2 & PCF8563_CONTROL_2_AF) != 0U && alarm_callback != NULL) ||
		    ((control_2 & PCF8563_CONTROL_2_SF) != 0U)) {
			/*
			 * Another interrupt occurred while servicing this one, process current
			 * callback(s) and yield.
			 */
			k_sem_give(&data->int1_sem);
		}

unlock:
		k_mutex_unlock(&data->lock);

		if (alarm_callback != NULL) {
			alarm_callback(dev, 0U, alarm_user_data);
			alarm_callback = NULL;
		}

		if (update_callback != NULL) {
			update_callback(dev, update_user_data);
			update_callback = NULL;
		}
	}
}

static void pcf8563_int1_callback_handler(const struct device *port, struct gpio_callback *cb,
					  gpio_port_pins_t pins)
{
	struct pcf8563_data *data = CONTAINER_OF(cb, struct pcf8563_data, int1_callback);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	k_sem_give(&data->int1_sem);
}
#endif /* PCF8563_INT1_GPIOS_IN_USE */

static int pcf8563_set_time(const struct device *dev, const struct rtc_time *timeptr)
{
	struct pcf8563_data *data = dev->data;
	uint8_t regs[7];
	int err;

	if (timeptr->tm_year < PCF8563_YEARS_OFFSET ||
	    timeptr->tm_year > PCF8563_YEARS_OFFSET + 99) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	/* Freeze the time circuits */
	err = pcf8563_write_stop_bit_unlocked(dev, true);
	if (err != 0) {
		goto unlock;
	}

	LOG_DBG("set time: year = %d, mon = %d, mday = %d, wday = %d, hour = %d, "
		"min = %d, sec = %d",
		timeptr->tm_year, timeptr->tm_mon, timeptr->tm_mday, timeptr->tm_wday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);

	regs[0] = bin2bcd(timeptr->tm_sec) & PCF8563_SECONDS_MASK;
	regs[1] = bin2bcd(timeptr->tm_min) & PCF8563_MINUTES_MASK;
	regs[2] = bin2bcd(timeptr->tm_hour) & PCF8563_HOURS_24H_MASK;
	regs[3] = bin2bcd(timeptr->tm_mday) & PCF8563_DAYS_MASK;
	regs[4] = bin2bcd(timeptr->tm_wday) & PCF8563_WEEKDAYS_MASK;
	regs[5] = bin2bcd(timeptr->tm_mon + PCF8563_MONTHS_OFFSET) & PCF8563_MONTHS_MASK;
	regs[6] = bin2bcd(timeptr->tm_year - PCF8563_YEARS_OFFSET) & PCF8563_YEARS_MASK;

	/* Write registers PCF8563_SECONDS through PCF8563_YEARS */
	err = pcf8563_write_regs(dev, PCF8563_SECONDS, &regs, sizeof(regs));
	if (err != 0) {
		goto unlock;
	}

	/* Unfreeze time circuits */
	err = pcf8563_write_stop_bit_unlocked(dev, false);
	if (err != 0) {
		goto unlock;
	}

unlock:
	k_mutex_unlock(&data->lock);

	return err;
}

static int pcf8563_get_time(const struct device *dev, struct rtc_time *timeptr)
{
	uint8_t regs[9];
	int err;

	/* Read registers PCF8563_CONTROL_STATUS1 through PCF8563_YEARS */
	err = pcf8563_read_regs(dev, PCF8563_CONTROL_STATUS1, &regs, sizeof(regs));
	if (err != 0) {
		return err;
	}

	if ((regs[0] & PCF8563_CONTROL_STATUS1_STOP) != 0) {
		LOG_WRN("time circuits frozen");
		return -ENODATA;
	}

	// if ((regs[2] & PCF8563_SECONDS_VL) != 0) {
	// 	LOG_WRN("oscillator stopped or interrupted");
	// 	return -ENODATA;
	// }
	memset(timeptr, 0U, sizeof(*timeptr));
	timeptr->tm_sec = bcd2bin(regs[2] & PCF8563_SECONDS_MASK);
	timeptr->tm_min = bcd2bin(regs[3] & PCF8563_MINUTES_MASK);
	timeptr->tm_hour = bcd2bin(regs[4] & PCF8563_HOURS_24H_MASK);
	timeptr->tm_mday = bcd2bin(regs[5] & PCF8563_DAYS_MASK);
	timeptr->tm_wday = bcd2bin(regs[6] & PCF8563_WEEKDAYS_MASK);

	timeptr->tm_mon = bcd2bin(regs[7] & PCF8563_MONTHS_MASK) - PCF8563_MONTHS_OFFSET;
	timeptr->tm_year = bcd2bin(regs[8] & PCF8563_YEARS_MASK) + PCF8563_YEARS_OFFSET;
	timeptr->tm_yday = -1;
	timeptr->tm_isdst = -1;

	LOG_DBG("get time: year = %d, mon = %d, mday = %d, wday = %d, hour = %d, "
		"min = %d, sec = %d",
		timeptr->tm_year, timeptr->tm_mon, timeptr->tm_mday, timeptr->tm_wday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);

	return 0;
}

#ifdef CONFIG_RTC_ALARM
static int pcf8563_alarm_get_supported_fields(const struct device *dev, uint16_t id, uint16_t *mask)
{
	ARG_UNUSED(dev);

	if (id != 0U) {
		LOG_ERR("invalid ID %d", id);
		return -EINVAL;
	}

	*mask = PCF8563_RTC_ALARM_TIME_MASK;

	return 0;
}

static int pcf8563_alarm_set_time(const struct device *dev, uint16_t id, uint16_t mask,
				  const struct rtc_time *timeptr)
{
	uint8_t regs[4];

	if (id != 0U) {
		LOG_ERR("invalid ID %d", id);
		return -EINVAL;
	}

	if ((mask & ~(PCF8563_RTC_ALARM_TIME_MASK)) != 0U) {
		LOG_ERR("unsupported alarm field mask 0x%04x", mask);
		return -EINVAL;
	}

	if ((mask & RTC_ALARM_TIME_MASK_MINUTE) != 0U) {
		regs[0] = bin2bcd(timeptr->tm_min) & PCF8563_MINUTE_ALARM_MASK;
	} else {
		regs[0] = PCF8563_MINUTE_ALARM_AEN_M;
	}

	if ((mask & RTC_ALARM_TIME_MASK_HOUR) != 0U) {
		regs[1] = bin2bcd(timeptr->tm_hour) & PCF8563_HOUR_ALARM_24H_MASK;
	} else {
		regs[1] = PCF8563_HOUR_ALARM_AEN_H;
	}

	if ((mask & RTC_ALARM_TIME_MASK_MONTHDAY) != 0U) {
		regs[2] = bin2bcd(timeptr->tm_mday) & PCF8563_DAY_ALARM_MASK;
	} else {
		regs[2] = PCF8563_DAY_ALARM_AEN_D;
	}

	if ((mask & RTC_ALARM_TIME_MASK_WEEKDAY) != 0U) {
		regs[3] = bin2bcd(timeptr->tm_wday) & PCF8563_WEEKDAY_ALARM_MASK;
	} else {
		regs[3] = PCF8563_WEEKDAY_ALARM_AEN_W;
	}

	LOG_DBG("set alarm: year = %d, mon = %d, mday = %d, hour = %d, min = %d, mask = 0x%04x",
		timeptr->tm_year, timeptr->tm_mon, timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, mask);

	/* Write registers PCF8563_MINUTE_ALARM through PCF8563_WEEKDAY_ALARM */
	return pcf8563_write_regs(dev, PCF8563_MINUTE_ALARM, &regs, sizeof(regs));
}

static int pcf8563_alarm_get_time(const struct device *dev, uint16_t id, uint16_t *mask,
				  struct rtc_time *timeptr)
{
	uint8_t regs[4];
	int err;

	if (id != 0U) {
		LOG_ERR("invalid ID %d", id);
		return -EINVAL;
	}

	/* Read registers PCF8563_MINUTE_ALARM through PCF8563_WEEKDAY_ALARM */
	err = pcf8563_read_regs(dev, PCF8563_MINUTE_ALARM, &regs, sizeof(regs));
	if (err != 0) {
		return err;
	}

	memset(timeptr, 0U, sizeof(*timeptr));
	*mask = 0U;

	if ((regs[0] & PCF8563_MINUTE_ALARM_AEN_M) == 0) {
		timeptr->tm_min = bcd2bin(regs[0] & PCF8563_MINUTE_ALARM_MASK);
		*mask |= RTC_ALARM_TIME_MASK_MINUTE;
	}

	if ((regs[1] & PCF8563_HOUR_ALARM_AEN_H) == 0) {
		timeptr->tm_hour = bcd2bin(regs[1] & PCF8563_HOUR_ALARM_24H_MASK);
		*mask |= RTC_ALARM_TIME_MASK_HOUR;
	}

	if ((regs[2] & PCF8563_DAY_ALARM_AEN_D) == 0) {
		timeptr->tm_mday = bcd2bin(regs[2] & PCF8563_DAY_ALARM_MASK);
		*mask |= RTC_ALARM_TIME_MASK_MONTHDAY;
	}

	if ((regs[3] & PCF8563_WEEKDAY_ALARM_AEN_W) == 0) {
		timeptr->tm_wday = bcd2bin(regs[3] & PCF8563_WEEKDAY_ALARM_MASK);
		*mask |= RTC_ALARM_TIME_MASK_WEEKDAY;
	}

	LOG_DBG("get alarm: year = %d, mon = %d, mday = %d, hour = %d, min = %d, mask = 0x%04x",
		timeptr->tm_year, timeptr->tm_mon, timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, *mask);

	return 0;
}

static int pcf8563_alarm_is_pending(const struct device *dev, uint16_t id)
{
	struct pcf8563_data *data = dev->data;
	uint8_t control_2;
	int err;

	if (id != 0U) {
		LOG_ERR("invalid ID %d", id);
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	err = pcf8563_read_reg8(dev, PCF8563_CONTROL_2, &control_2);
	if (err != 0) {
		goto unlock;
	}

	if ((control_2 & PCF8563_CONTROL_2_AF) != 0) {
		/* Clear alarm flag */
		control_2 &= ~(PCF8563_CONTROL_2_AF);

		/* Ensure other flags are left unchanged (PCF8563 performs logic AND at write) */
		control_2 |= PCF8563_CONTROL_2_CTAF | PCF8563_CONTROL_2_CTBF | PCF8563_CONTROL_2_SF;

		err = pcf8563_write_reg8(dev, PCF8563_CONTROL_2, control_2);
		if (err != 0) {
			goto unlock;
		}

		/* Alarm pending */
		err = 1;
	}

unlock:
	k_mutex_unlock(&data->lock);

	return err;
}

#if PCF8563_INT1_GPIOS_IN_USE
static int pcf8563_alarm_set_callback(const struct device *dev, uint16_t id,
				      rtc_alarm_callback callback, void *user_data)
{
	const struct pcf8563_config *config = dev->config;
	struct pcf8563_data *data = dev->data;
	uint8_t control_status1;
	int err = 0;

	if (config->int1.port == NULL) {
		return -ENOTSUP;
	}

	if (id != 0U) {
		LOG_ERR("invalid ID %d", id);
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	data->alarm_callback = callback;
	data->alarm_user_data = user_data;

	if (!config->wakeup_source) {
		/* Only change AIE if not configured as wakeup-source */
		err = pcf8563_read_reg8(dev, PCF8563_CONTROL_STATUS1, &control_status1);
		if (err != 0) {
			goto unlock;
		}

		if (callback != NULL) {
			control_status1 |= PCF8563_CONTROL_STATUS1_AIE;
		} else {
			control_status1 &= ~(PCF8563_CONTROL_STATUS1_AIE);
		}

		if ((control_status1 & PCF8563_CONTROL_STATUS1_SIE) == 0U) {
			/* Only change INT1 GPIO if seconds timer interrupt not enabled */
			err = pcf8563_int1_enable_unlocked(dev, callback != NULL);
			if (err != 0) {
				goto unlock;
			}
		}

		err = pcf8563_write_reg8(dev, PCF8563_CONTROL_STATUS1, control_status1);
		if (err != 0) {
			goto unlock;
		}
	}

unlock:
	k_mutex_unlock(&data->lock);

	/* Wake up the INT1 thread since the alarm flag may already be set */
	k_sem_give(&data->int1_sem);

	return err;
}
#endif /* PCF8563_INT1_GPIOS_IN_USE */
#endif /* CONFIG_RTC_ALARM */

#if PCF8563_INT1_GPIOS_IN_USE && defined(CONFIG_RTC_UPDATE)
static int pcf8563_update_set_callback(const struct device *dev, rtc_update_callback callback,
				       void *user_data)
{
	const struct pcf8563_config *config = dev->config;
	struct pcf8563_data *data = dev->data;
	uint8_t control_status1;
	int err;

	if (config->int1.port == NULL) {
		return -ENOTSUP;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	data->update_callback = callback;
	data->update_user_data = user_data;

	err = pcf8563_read_reg8(dev, PCF8563_CONTROL_STATUS1, &control_status1);
	if (err != 0) {
		goto unlock;
	}

	if (callback != NULL) {
		control_status1 |= PCF8563_CONTROL_STATUS1_SIE;
	} else {
		control_status1 &= ~(PCF8563_CONTROL_STATUS1_SIE);
	}

	if ((control_status1 & PCF8563_CONTROL_STATUS1_AIE) == 0U) {
		/* Only change INT1 GPIO if alarm interrupt not enabled */
		err = pcf8563_int1_enable_unlocked(dev, callback != NULL);
		if (err != 0) {
			goto unlock;
		}
	}

	err = pcf8563_write_reg8(dev, PCF8563_CONTROL_STATUS1, control_status1);
	if (err != 0) {
		goto unlock;
	}

unlock:
	k_mutex_unlock(&data->lock);

	/* Wake up the INT1 thread since the seconds flag may already be set */
	k_sem_give(&data->int1_sem);

	return err;
}
#endif /* PCF8563_INT1_GPIOS_IN_USE && defined(CONFIG_RTC_UPDATE) */

#ifdef CONFIG_RTC_CALIBRATION

/* See PCF8563 data sheet table 29 */
#if defined(CONFIG_RTC_PCF8563_OFFSET_MODE_SLOW)
#define PCF8563_OFFSET_PPB_PER_LSB 4340
#elif defined(CONFIG_RTC_PCF8563_OFFSET_MODE_FAST)
#define PCF8563_OFFSET_PPB_PER_LSB 4069
#else
#error Unsupported offset mode
#endif

#define PCF8563_OFFSET_PPB_MIN (-64 * PCF8563_OFFSET_PPB_PER_LSB)
#define PCF8563_OFFSET_PPB_MAX (63 * PCF8563_OFFSET_PPB_PER_LSB)

static int pcf8563_set_calibration(const struct device *dev, int32_t freq_ppb)
{
	int32_t period_ppb = freq_ppb * -1;
	int8_t offset;

	if (period_ppb < PCF8563_OFFSET_PPB_MIN || period_ppb > PCF8563_OFFSET_PPB_MAX) {
		LOG_WRN("calibration value (%d ppb) out of range", freq_ppb);
		return -EINVAL;
	}

	offset = period_ppb / PCF8563_OFFSET_PPB_PER_LSB;

	if (IS_ENABLED(CONFIG_RTC_PCF8563_OFFSET_MODE_FAST)) {
		offset |= PCF8563_OFFSET_MODE;
	}

	LOG_DBG("freq_ppb = %d, period_ppb = %d, offset = %d", freq_ppb, period_ppb, offset);

	return pcf8563_write_reg8(dev, PCF8563_OFFSET, offset);
}

static int pcf8563_get_calibration(const struct device *dev, int32_t *freq_ppb)
{
	int32_t period_ppb;
	int8_t offset;
	int err;

	err = pcf8563_read_reg8(dev, PCF8563_OFFSET, &offset);
	if (err != 0) {
		return err;
	}

	/* Clear mode bit and sign extend the offset */
	period_ppb = (offset << 1U) >> 1U;

	period_ppb = period_ppb * PCF8563_OFFSET_PPB_PER_LSB;
	*freq_ppb = period_ppb * -1;

	LOG_DBG("freq_ppb = %d, period_ppb = %d, offset = %d", *freq_ppb, period_ppb, offset);

	return 0;
}
#endif /* CONFIG_RTC_CALIBRATION */

static int pcf8563_init(const struct device *dev)
{
	const struct pcf8563_config *config = dev->config;
	struct pcf8563_data *data = dev->data;
	uint8_t regs[2];
	int err;

	k_mutex_init(&data->lock);

	const struct device *i2c_dev = device_get_binding(config->i2c.bus->name);
	if (i2c_dev == NULL) {
		LOG_ERR("%s: Device is not found.", i2c_dev->name);
		return -ENODEV;
	}
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("%s: Device is not ready.", i2c_dev->name);
		return -ENODEV;
	}
	if (i2c_configure(i2c_dev, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER)) {
		LOG_ERR("%s: Failed to configure I2C.", i2c_dev->name);
		return -EINVAL;
	}
	LOG_INF("I2C: %s configured.", i2c_dev->name);

#if PCF8563_INT1_GPIOS_IN_USE
	k_tid_t tid;

	if (config->int1.port != NULL) {
		k_sem_init(&data->int1_sem, 0, INT_MAX);

		if (!gpio_is_ready_dt(&config->int1)) {
			LOG_ERR("GPIO not ready");
			return -ENODEV;
		}

		err = gpio_pin_configure_dt(&config->int1, GPIO_INPUT);
		if (err != 0) {
			LOG_ERR("failed to configure GPIO (err %d)", err);
			return -ENODEV;
		}

		gpio_init_callback(&data->int1_callback, pcf8563_int1_callback_handler,
				   BIT(config->int1.pin));

		err = gpio_add_callback_dt(&config->int1, &data->int1_callback);
		if (err != 0) {
			LOG_ERR("failed to add GPIO callback (err %d)", err);
			return -ENODEV;
		}

		tid = k_thread_create(&data->int1_thread, data->int1_stack,
				      K_THREAD_STACK_SIZEOF(data->int1_stack),
				      (k_thread_entry_t)pcf8563_int1_thread, (void *)dev, NULL,
				      NULL, CONFIG_RTC_PCF8563_THREAD_PRIO, 0, K_NO_WAIT);
		k_thread_name_set(tid, "pcf8563");

		/*
		 * Defer GPIO interrupt configuration due to INT1/CLKOUT pin sharing. This allows
		 * using the CLKOUT square-wave signal for RTC calibration when no alarm/update
		 * callbacks are enabled (and not configured as a wakeup-source).
		 */
	}
#endif /* PCF8563_INT1_GPIOS_IN_USE */

	/*
	 * Manually initialize the required PCF8563 registers as performing a software reset will
	 * reset the time circuits.
	 */

	/* Read registers PCF8563_CONTROL_STATUS1 through PCF8563_CONTROL_STATUS2 */
	err = pcf8563_read_regs(dev, PCF8563_CONTROL_STATUS1, &regs, sizeof(regs));
	if (err != 0) {
		return -ENODEV;
	}
    if (0 != (regs[0] & PCF8563_CONTROL_STATUS1_TEST1)) {
        LOG_ERR("%s: CONTROL_STATUS1 failed", dev->name);
		return -ENODEV;
    }

#if 0
	/* Use 24h time format */
	regs[0] &= ~(PCF8563_CONTROL_STATUS1_12_24);

	/* Disable second, alarm, and correction interrupts */
	regs[0] &= ~(PCF8563_CONTROL_STATUS1_SIE | PCF8563_CONTROL_STATUS1_AIE | PCF8563_CONTROL_STATUS1_CIE);

	if (config->wakeup_source) {
		/*
		 * Always set AIE if wakeup-source. This allows the RTC to wake up the system even
		 * if the INT1 interrupt output is not directly connected to a GPIO (i.e. if
		 * connected to a PMIC input).
		 */
		regs[0] |= PCF8563_CONTROL_STATUS1_AIE;
	}

	/* Clear interrupt flags (except alarm flag, as a wake-up alarm may be pending) */
	regs[1] &= ~(PCF8563_CONTROL_2_CTAF | PCF8563_CONTROL_2_CTBF | PCF8563_CONTROL_2_SF);

	/* Disable watchdog, timer A, and timer B interrupts */
	regs[1] &= ~(PCF8563_CONTROL_2_WTAIE | PCF8563_CONTROL_2_CTAIE | PCF8563_CONTROL_2_CTBIE);

	/* Configure battery switch-over function */
	regs[2] &= ~(PCF8563_CONTROL_3_PM_MASK);
	regs[2] |= FIELD_PREP(PCF8563_CONTROL_3_PM_MASK, config->pm);

	/* Clear battery status interrupt flag */
	regs[2] &= ~(PCF8563_CONTROL_3_BSF);

	/* Disable battery status interrupts */
	regs[2] &= ~(PCF8563_CONTROL_3_BSIE | PCF8563_CONTROL_3_BLIE);

	/* Write registers PCF8563_CONTROL_STATUS1 through PCF8563_CONTROL_3 */
	err = pcf8563_write_regs(dev, PCF8563_CONTROL_STATUS1, &regs, sizeof(regs));
	if (err != 0) {
		return -ENODEV;
	}

	/* Disable watchdog and countdown timers, configure IRQ level*/
	tmr_clkout_ctrl = 0U;

	if (config->wakeup_source) {
		/* Disable CLKOUT */
		tmr_clkout_ctrl |= PCF8563_TMR_CLKOUT_CTRL_COF_MASK;
	} else {
		/* Configure CLKOUT frequency */
		tmr_clkout_ctrl |= FIELD_PREP(PCF8563_TMR_CLKOUT_CTRL_COF_MASK, config->cof);
	}

	err = pcf8563_write_reg8(dev, PCF8563_TMR_CLKOUT_CTRL, tmr_clkout_ctrl);
	if (err != 0) {
		return -ENODEV;
	}
#endif
	return 0;
}

/* Mapping from DT battery-switch-over enum to CONTROL_3 PM field value */
#define PCF8563_PM_STANDARD 4U
#define PCF8563_PM_DIRECT   5U
#define PCF8563_PM_DISABLED 7U

#ifdef CONFIG_PM_DEVICE
static int pcf8563_pm_action(const struct device *dev, enum pm_device_action action)
{
	const struct pcf8563_config *config = dev->config;
	uint8_t control_3;
	int err;

	if (config->pm == PCF8563_PM_DISABLED) {
		/* Only one power supply */
		return -ENOTSUP;
	}

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		/* Disable battery switch-over function */
		control_3 = FIELD_PREP(PCF8563_CONTROL_3_PM_MASK, PCF8563_PM_DISABLED);
		break;
	case PM_DEVICE_ACTION_RESUME:
		/* Re-enable battery switch-over function */
		control_3 = FIELD_PREP(PCF8563_CONTROL_3_PM_MASK, config->pm);
		break;
	default:
		return -ENOTSUP;
	}

	err = pcf8563_write_reg8(dev, PCF8563_CONTROL_3, control_3);
	if (err != 0) {
		return -EIO;
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

static const struct rtc_driver_api pcf8563_driver_api = {
	.set_time = pcf8563_set_time,
	.get_time = pcf8563_get_time,
#ifdef CONFIG_RTC_ALARM
	.alarm_get_supported_fields = pcf8563_alarm_get_supported_fields,
	.alarm_set_time = pcf8563_alarm_set_time,
	.alarm_get_time = pcf8563_alarm_get_time,
	.alarm_is_pending = pcf8563_alarm_is_pending,
#if PCF8563_INT1_GPIOS_IN_USE
	.alarm_set_callback = pcf8563_alarm_set_callback,
#endif /* PCF8563_INT1_GPIOS_IN_USE */
#endif /* CONFIG_RTC_ALARM */
#if PCF8563_INT1_GPIOS_IN_USE && defined(CONFIG_RTC_UPDATE)
	.update_set_callback = pcf8563_update_set_callback,
#endif /* PCF8563_INT1_GPIOS_IN_USE && defined(CONFIG_RTC_UPDATE) */
#ifdef CONFIG_RTC_CALIBRATION
	.set_calibration = pcf8563_set_calibration,
	.get_calibration = pcf8563_get_calibration,
#endif /* CONFIG_RTC_CALIBRATION */
};

#define PCF8563_PM_FROM_DT_INST(inst)                                                              \
	UTIL_CAT(PCF8563_PM_, DT_INST_STRING_UPPER_TOKEN(inst, battery_switch_over))
#define PCF8563_CAP_SEL_FROM_DT_INST(inst) (DT_INST_PROP(inst, quartz_load_femtofarads) == 12500)

#define PCF8563_INIT(inst)                                                                         \
	static const struct pcf8563_config pcf8563_config_##inst = {                               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.cof = DT_INST_ENUM_IDX(inst, clkout_frequency),                                   \
		.pm = PCF8563_PM_FROM_DT_INST(inst),                                               \
		.cap_sel = PCF8563_CAP_SEL_FROM_DT_INST(inst),                                     \
		.wakeup_source = DT_INST_PROP(inst, wakeup_source),                                \
		IF_ENABLED(PCF8563_INT1_GPIOS_IN_USE,                                              \
			   (.int1 = GPIO_DT_SPEC_INST_GET_OR(inst, int1_gpios, {0})))};            \
                                                                                                   \
	static struct pcf8563_data pcf8563_data_##inst;                                            \
                                                                                                   \
	PM_DEVICE_DT_INST_DEFINE(inst, pcf8563_pm_action);                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &pcf8563_init, PM_DEVICE_DT_INST_GET(inst),                    \
			      &pcf8563_data_##inst, &pcf8563_config_##inst, POST_KERNEL,           \
			      CONFIG_RTC_INIT_PRIORITY, &pcf8563_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PCF8563_INIT)
