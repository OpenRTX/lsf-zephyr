/**
 * @file i2c_slave_dev.h
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-01
 * 
 * @copyright Copyright (c) 2023
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I2C_SLAVE_VDEV_H_
#define __I2C_SLAVE_VDEV_H_

// 用户需要关注的事件：
// 读取完成
// 写入完成
#define SLAVE_VDEV_EVENT_BUS_STOP 		(1UL << 0)
#define SLAVE_VDEV_EVENT_READ_REQ 		(1UL << 1)
#define SLAVE_VDEV_EVENT_WRITE_REQ 		(1UL << 2)
#define SLAVE_VDEV_EVENT_READ_DONE 		(1UL << 3)
#define SLAVE_VDEV_EVENT_WRITE_DONE 	(1UL << 4)

typedef int (*slave_vdev_handler_t)(const struct device *dev, const uint32_t event);

int slave_vdev_program(const struct device *dev, const uint8_t *slave_vdev_data,
			 unsigned int length);

int slave_vdev_read(const struct device *dev, uint8_t *slave_vdev_data,
		      unsigned int length);

int slave_vdev_read_byte(const struct device *dev, uint8_t *slave_vdev_data,
		      unsigned int offset);

int slave_vdev_event_handler_register(const struct device *dev, slave_vdev_handler_t handler);
int start_slave_vdev_handler_task(void);

#endif
