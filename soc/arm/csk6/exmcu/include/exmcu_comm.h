/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EXMCU_INCLUDE__
#define __EXMCU_INCLUDE__

#include <stdint.h>
#include <zephyr/device.h>


#define EXMCU_PROTOCOL_VERSION "0.1"

enum {
	CH32V003_STATUS_INVALID = 0,
	CH32V003_STATUS_BUSY = 1 << 0,
	CH32V003_STATUS_DONE = 1 << 1,
};

#define CH32V003_BUSY_DELAY_TIMEOUT_MS (10)
#define CH32V003_BUSY_DELAY_CNT	       (3)

/* system common registers */
#define CH32V003_ID_REG	  0x01
#define CH32V003_VER_REG  0x02
#define CH32V003_STA_REG  0x03
#define CH32V003_PAGE_REG 0x04

/* gpio control registers */
#define CH32V003_DIR_REG_BASE	    0x10
#define CH32V003_OUT_REG_BASE	    0x11
#define CH32V003_OD_REG_BASE	    0x12
#define CH32V003_IN_REG_BASE	    0x13
#define CH32V003_PULL_UP_REG_BASE   0x14
#define CH32V003_PULL_DWON_REG_BASE 0x15
#define CH32V003_INT_STA_REG_BASE   0x16
#define CH32V003_INT_EN_REG_BASE    0x17
#define CH32V003_INT_CTL_REG_BASE   0x18

#define CH32V003_DIR_REG(index)	      ((CH32V003_DIR_REG_BASE) + (index * 0x10))
#define CH32V003_OUT_REG(index)	      ((CH32V003_OUT_REG_BASE) + (index * 0x10))
#define CH32V003_OD_REG(index)	      ((CH32V003_OD_REG_BASE) + (index * 0x10))
#define CH32V003_IN_REG(index)	      ((CH32V003_IN_REG_BASE) + (index * 0x10))
#define CH32V003_PULL_UP_REG(index)   ((CH32V003_PULL_UP_REG_BASE) + (index * 0x10))
#define CH32V003_PULL_DWON_REG(index) ((CH32V003_PULL_DWON_REG_BASE) + (index * 0x10))
#define CH32V003_INT_STA_REG(index)   ((CH32V003_INT_STA_REG_BASE) + (index * 0x10))
#define CH32V003_INT_EN_REG(index)    ((CH32V003_INT_EN_REG_BASE) + (index * 0x10))
#define CH32V003_INT_CTL_REG(index)   ((CH32V003_INT_CTL_REG_BASE) + (index * 0x10))

/* adc control registers */
#define CH32V003_ADC_CONFIG_0	0x50
#define CH32V003_ADC_CONFIG_1	0x51
#define CH32V003_ADC_EN_REG		0x52
#define CH32V003_ADC_DATA_H_REG	0x53
#define CH32V003_ADC_DATA_L_REG	0x54
#define CH32V003_ADC_STATUS_REG	0x55

#define CH32V003_ADC_CH_DATA_H_REG(ch) ((CH32V003_ADC_DATA_H_REG) + (ch)*16)
#define CH32V003_ADC_CH_DATA_L_REG(ch) ((CH32V003_ADC_DATA_L_REG) + (ch)*16)

/* pwm control registers */
#define CH32V003_PWM_CTRL_REG  0x70
#define CH32V003_PWM_ARR_REG_H 0x71
#define CH32V003_PWM_ARR_REG_L 0x72
#define CH32V003_PWM_PSC_REG_H 0x73
#define CH32V003_PWM_PSC_REG_L 0x74
#define CH32V003_PWM_CCP_REG_H 0x75
#define CH32V003_PWM_CCP_REG_L 0x76
#define CH32V003_PWM_DTS_REG   0x77

int exmcu_register_read_byte(const struct device *dev, uint8_t reg, uint8_t *value);
int exmcu_register_write_byte(const struct device *dev, uint8_t reg, uint8_t value);

#endif
