/**
 * @file
 *
 * @brief Public APIs for the Infrared Radiation drivers.
 */

/**
 * Copyright (c) 2023 Anhui(Shenshen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DRIVERS_INFRARED_RADIATION_H_
#define ZEPHYR_INCLUDE_DRIVERS_INFRARED_RADIATION_H_

/**
 * @brief Infrared Radiation
 * @defgroup infrared_interface Infrared_Radiation Interface
 * @ingroup io_interfaces
 * @{
 */
#include <errno.h>

#include <zephyr/types.h>
#include <zephyr/device.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IR_PROTOCOL_FREQ_38KHZ 38000
#define IR_PROTOCOL_FREQ_DEFAULT IR_PROTOCOL_FREQ_38KHZ

/************IR Error**************/
#define IR_ERR_TIMEOUT 						(1)
#define IR_ERR_BUSY 						(2)
#define IR_ERR_INVALID_PARAM					(3)
#define IR_ERR_INVALID_PROTOCOL					(4)

/************IR Event**************/

#define IR_EVENT_HW_SEND_COMPLETE				(0x1UL << 0)
#define IR_EVENT_HW_RECEIVE_COMPLETE				(0x1UL << 1)
#define IR_EVENT_HW_TX_REPEAT_COMPLETE				(0x1UL << 2)
#define IR_EVENT_HW_RX_REPEAT_TRIGGER				(0x1UL << 3)
#define IR_EVENT_HW_ADDRESS_VERIFY_ERROR			(0x1UL << 4)
#define IR_EVENT_HW_COMMAND_VERIFY_ERROR			(0x1UL << 5)
#define IR_EVENT_SW_SEND_COMPLETE				(0x1UL << 6)
#define IR_EVENT_SW_RECEIVE_COMPLETE				(0x1UL << 7)
#define IR_EVENT_SW_RECEIVE_TIMEOUT				(0x1UL << 8)

/************IR Protocol**************/
#define IR_PROTOCOL_CUSTOM					(0x0UL)
#define IR_PROTOCOL_NEC						(0x1UL)
#define IR_PROTOCOL_TOSHIBA_9012				(0x2UL)
#define IR_PROTOCOL_PHILIPS_RC5					(0x3UL)
#define IR_PROTOCOL_DEFAULT					IR_PROTOCOL_NEC
typedef struct
{
    uint8_t protocol;		/* Protocol mode */
    uint32_t freq;		/* Protocol frequency */
    uint32_t timeout;		/* Timeout in milliseconds */
} ir_config_t;

typedef struct
{
    uint16_t addr;
    uint16_t cmd;
} ir_data_t;

/**
 * @brief Infrared Radiation driver API
 *
 * This is the mandatory API any Infrared Radiation driver needs to
 * expose.
 */

typedef int (*ir_configure_t)(const struct device *dev, ir_config_t *config);
typedef int (*ir_send_t)(const struct device *dev, ir_data_t *data);
typedef int (*ir_recv_t)(const struct device *dev, ir_data_t *data);
typedef int (*ir_recv_timeout_t)(const struct device *dev, ir_data_t *data, k_timeout_t timeout);
__subsystem struct infrared_driver_api {
    ir_configure_t configure;
    ir_send_t send;
    ir_recv_t recv;
    ir_recv_timeout_t recv_timeout;
};

/**
 * @brief Configure the Infrared Radiation device
 *
 * This routine is used to configure the Infrared Radiation device.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param[in] config Pointer to the configuration structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
__syscall int infrared_configure(const struct device *dev, ir_config_t *config);

static inline int z_impl_infrared_configure(const struct device *dev, ir_config_t *config)
{
    const struct infrared_driver_api *api = dev->api;

    return api->configure(dev, config);
}

/**
 * @brief Send data to the Infrared Radiation device
 *
 * This routine is used to send data to the Infrared Radiation device.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param[in] data Pointer to the data structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
__syscall int infrared_send(const struct device *dev, ir_data_t *data);

static inline int z_impl_infrared_send(const struct device *dev, ir_data_t *data)
{
    const struct infrared_driver_api *api = dev->api;

    return api->send(dev, data);
}

/**
 * @brief Receive data from the Infrared Radiation device
 *
 * This routine is used to receive data from the Infrared Radiation device.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param[out] data Pointer to the data structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
__syscall int infrared_recv(const struct device *dev, ir_data_t *data);

static inline int z_impl_infrared_recv(const struct device *dev, ir_data_t *data)
{
    const struct infrared_driver_api *api = dev->api;
    return api->recv(dev, data);
}

/**
 * @brief Receive data from the Infrared Radiation device
 *
 * This routine is used to receive data from the Infrared Radiation device.
 *
 * @param[in] dev Pointer to the device structure for the driver instance.
 * @param[out] data Pointer to the data structure.
 * @param[in] timeout Timeout in milliseconds.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
__syscall int infrared_recv_timeout(const struct device *dev, ir_data_t *data, k_timeout_t timeout);

static inline int z_impl_infrared_recv_timeout(const struct device *dev, ir_data_t *data, k_timeout_t timeout)
{
    const struct infrared_driver_api *api = dev->api;
    return api->recv_timeout(dev, data, timeout);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/infrared.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_INFRARED_RADIATION_H_ */
