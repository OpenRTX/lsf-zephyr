/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EXMCU_INCLUDE__
#define __EXMCU_INCLUDE__

#include <stdint.h>
#include <zephyr/device.h>

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
