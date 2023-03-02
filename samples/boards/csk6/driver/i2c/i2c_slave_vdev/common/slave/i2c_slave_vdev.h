/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I2C_SLAVE_VDEV_H_
#define __I2C_SLAVE_VDEV_H_

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#define SLAVE_VDEV_EVENT_BUS_STOP   (1UL << 0)
#define SLAVE_VDEV_EVENT_READ_REQ   (1UL << 1)
#define SLAVE_VDEV_EVENT_WRITE_REQ  (1UL << 2)
#define SLAVE_VDEV_EVENT_READ_DONE  (1UL << 3)
#define SLAVE_VDEV_EVENT_WRITE_DONE (1UL << 4)

typedef int (*slave_vdev_handler_t)(const struct device *dev, const uint32_t event,
				    const struct i2c_msg msg);

/**
 * @brief Specifies the byte length to write to the virtual device data buffer
 *
 * @param dev I2C Slave device instance
 * @param src Source memory buffer, in userspace
 * @param length Specifies the byte length to write to the virtual device data buffer
 * @return int
 */
int slave_vdev_program(const struct device *dev, const uint8_t *src, unsigned int length);

/**
 * @brief Read the specified length from the specified offset of the virtual device data buffer
 *
 * @param dev I2C Slave device instance
 * @param dst Destination memory buffer
 * @param length
 * @return int
 */
int slave_vdev_read(const struct device *dev, uint8_t *dst, unsigned int length);

/**
 * @brief
 *
 * @param dev I2C Slave device instance
 * @param dst Destination memory buffer
 * @param offset Specifies to read from the specified data buf offset
 * @return int
 */
int slave_vdev_read_byte(const struct device *dev, uint8_t *dst, unsigned int offset);

/**
 * @brief Register I2C Slave virtual event handler callback
 *
 * @param dev I2C Slave device instance
 * @param handler User event handler callback function
 * @return int Register operation result
 */
int slave_vdev_event_handler_register(const struct device *dev, slave_vdev_handler_t handler);

/**
 * @brief Start Slave virtual device handler task.
 *
 * @param dev I2C Slave device instance
 * @return int Start task status
 */
int start_slave_vdev_handler_task(const struct device *dev);

#endif
