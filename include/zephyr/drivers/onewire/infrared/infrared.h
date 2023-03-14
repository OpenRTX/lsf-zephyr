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
#ifndef ZEPHYR_INCLUDE_DRIVERS_ONEWIRE_INFRARED_RADIATION_H_
#define ZEPHYR_INCLUDE_DRIVERS_ONEWIRE_INFRARED_RADIATION_H_

/**
 * @brief Infrared Radiation
 * @defgroup infrared_interface Interface Interface
 * @ingroup io_interfaces
 * @{
 */

#include <zephyr/types.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ir_protoclo_mode
{
    IR_MODE_NEC,            /* NES */
    IR_MODE_TOSHIBA_9012,   /* Toshiba 9012 */
    IR_MODE_PHILIPS_RC5,    /* Philips RC5 */
    IR_MODE_CUSTOM,         /* Private protocol */
};

typedef struct
{
    enum ir_protoclo_mode mode;
    uint32_t freq;
    uint32_t duty_cycle;
    uint32_t timeout;
} ir_config_t;

typedef struct
{
    uint32_t addr;
    uint32_t cmd;
} ir_data_t;

/**
 * @brief Infrared Radiation driver API
 *
 * This is the mandatory API any Infrared Radiation driver needs to
 * expose.
 */

struct infrared_driver_api {
    int (*init)(struct device *dev, ir_config_t *config);
    int (*send)(struct device *dev, ir_data_t *data);
    int (*recv)(struct device *dev, ir_data_t *data);
    int (*set_mode)(struct device *dev, enum ir_protoclo_mode mode);
    int (*set_freq)(struct device *dev, uint32_t freq);
};

/**
 * @brief Initialize the Infrared Radiation device
 *
 * This routine is used to initialize the Infrared Radiation device.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param config Pointer to the configuration structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
static inline int infrared_init(struct device *dev, ir_config_t *config)    
{
    const struct infrared_driver_api *api = dev->driver_api;

    return api->init(dev, config);
}

/**
 * @brief Send data to the Infrared Radiation device
 *
 * This routine is used to send data to the Infrared Radiation device.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param data Pointer to the data structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */

static inline int infrared_send(struct device *dev, ir_data_t *data)
{
    const struct infrared_driver_api *api = dev->driver_api;

    return api->send(dev, data);
}

/**
 * @brief Receive data from the Infrared Radiation device
 *
 * This routine is used to receive data from the Infrared Radiation device.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param data Pointer to the data structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */

static inline int infrared_recv(struct device *dev, ir_data_t *data)
{
    const struct infrared_driver_api *api = dev->driver_api;
    return api->recv(dev, data);
}

/**
 * @brief Receive data from the Infrared Radiation device
 *
 * This routine is used to receive data from the Infrared Radiation device.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param data Pointer to the data structure.
 * @param timeout Timeout in milliseconds.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
static inline int infrared_recv_timeout(struct device *dev, ir_data_t *data, uint32_t timeout)
{
    const struct infrared_driver_api *api = dev->driver_api;
    return api->recv_timeout(dev, data, timeout);
}

/**
 * @brief Set the Infrared Radiation device duty cycle
 *
 * This routine is used to set the Infrared Radiation device duty cycle.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param duty_cycle Pointer to the duty cycle structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
static inline int infrared_set_freq(struct device *dev, uint32_t freq)
{
    const struct infrared_driver_api *api = dev->driver_api;
    return api->set_freq(dev, freq);
}


/**
 * @brief Set the Infrared Radiation device mode
 *
 * This routine is used to set the Infrared Radiation device mode.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param mode Pointer to the mode structure.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to configure device.
 */
static inline int infrared_set_mode(struct device *dev, enum ir_protoclo_mode mode)
{
    const struct infrared_driver_api *api = dev->driver_api;
    return api->set_mode(dev, mode);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_ONEWIRE_INFRARED_RADIATION_H_ */