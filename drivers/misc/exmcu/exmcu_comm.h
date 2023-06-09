/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EXMCU_INCLUDE__
#define __EXMCU_INCLUDE__

#include <stdint.h>
#include <zephyr/device.h>


#define EXMCU_PROTOCOL_VERSION 13 /* v1.3 */

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
#define CH32V003_DIR_REG_BASE	    		0x10
#define CH32V003_OUT_REG_BASE	    		0x11
#define CH32V003_OD_REG_BASE	    		0x12
#define CH32V003_IN_REG_BASE	    		0x13
#define CH32V003_PULL_EN_REG_BASE  	0x14
#define CH32V003_PULL_CTRL_REG_BASE 	0x15
#define CH32V003_INT_STA_REG_BASE   		0x16
#define CH32V003_INT_EN_REG_BASE    		0x17
#define CH32V003_INT_CTL_REG_BASE   		0x18

#define CH32V003_DIR_REG(index)	      ((CH32V003_DIR_REG_BASE) + (index * 0x10))
#define CH32V003_OUT_REG(index)	      ((CH32V003_OUT_REG_BASE) + (index * 0x10))
#define CH32V003_OD_REG(index)	      ((CH32V003_OD_REG_BASE) + (index * 0x10))
#define CH32V003_IN_REG(index)	      ((CH32V003_IN_REG_BASE) + (index * 0x10))
#define CH32V003_PULL_EN_REG(index)   ((CH32V003_PULL_EN_REG_BASE) + (index * 0x10))
#define CH32V003_PULL_CTRL_REG(index) ((CH32V003_PULL_CTRL_REG_BASE) + (index * 0x10))
#define CH32V003_INT_STA_REG(index)   ((CH32V003_INT_STA_REG_BASE) + (index * 0x10))
#define CH32V003_INT_EN_REG(index)    ((CH32V003_INT_EN_REG_BASE) + (index * 0x10))
#define CH32V003_INT_CTL_REG(index)   ((CH32V003_INT_CTL_REG_BASE) + (index * 0x10))

/* adc control registers */
#define CH32V003_ADC_CONFIG_0	0x50
#define CH32V003_ADC_CONFIG_1	0x51
#define CH32V003_ADC_STATUS_REG	0x52
#define CH32V003_ADC_DATA_REG	0x60

#define CH32V003_ADC_CH_DATA_H_REG(ch) ((CH32V003_ADC_DATA_REG) + (ch)*2)
#define CH32V003_ADC_CH_DATA_L_REG(ch) ((CH32V003_ADC_DATA_REG) + (ch)*2 + 1)

#define CH32V003_ADC_SAMPLING_CYCLE_3 (0)
#define CH32V003_ADC_SAMPLING_CYCLE_9 (1)
#define CH32V003_ADC_SAMPLING_CYCLE_15 (2)
#define CH32V003_ADC_SAMPLING_CYCLE_30 (3)
#define CH32V003_ADC_SAMPLING_CYCLE_43 (4)
#define CH32V003_ADC_SAMPLING_CYCLE_57 (5)
#define CH32V003_ADC_SAMPLING_CYCLE_73 (6)
#define CH32V003_ADC_SAMPLING_CYCLE_241 (7)

#define CH32V003_ADC_SAMPLING_CYCLE_TIME_US (33)
#define CH32V003_ADC_CONVERT_TIME_US(cycle) ((cycle) * CH32V003_ADC_ACQ_CYCLE_TIME_US + 11 * CH32V003_ADC_ACQ_CYCLE_TIME_US)

#define CH32V003_ADC_MODE_ONCE   0x00
#define CH32V003_ADC_MODE_REPEAT 0x01

#define CH32V003_ADC_EOC 0x01

/* pwm control registers */
#define CH32V003_PWM_CTRL_REG  0x70
#define CH32V003_PWM_ARR_REG_H 0x71
#define CH32V003_PWM_ARR_REG_L 0x72
#define CH32V003_PWM_PSC_REG_H 0x73
#define CH32V003_PWM_PSC_REG_L 0x74
#define CH32V003_PWM_CCP_REG_H 0x75
#define CH32V003_PWM_CCP_REG_L 0x76
#define CH32V003_PWM_DTS_REG   0x77
#define CH32V003_PWM_BRAKE_REG   0x78

#define CH32V003_PWM_EN (1 << 0)
#define CH32V003_PWM_DEADTIME_EN (1 << 1)
#define CH32V003_PWM_BRAKE_EN (1 << 4)

int exmcu_register_read_byte(const struct device *dev, uint8_t reg, uint8_t *value);
int exmcu_register_write_byte(const struct device *dev, uint8_t reg, uint8_t value);

#endif
