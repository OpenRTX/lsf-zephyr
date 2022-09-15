/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * CSK6 SoC specific helpers for pinctrl driver
 */

#ifndef ZEPHYR_SOC_ARM_CSK6_PINCTRL_SOC_H_
#define ZEPHYR_SOC_ARM_CSK6_PINCTRL_SOC_H_

#include <zephyr/devicetree.h>
#include <zephyr/types.h>


#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */


typedef struct pinctrl_soc_pin {
	/* Pinmux control group */
	const struct device *pinctrls;
	/* GPIO pin */
	uint8_t pin;
	/* Alternate function */
	uint8_t alt_func;
} pinctrl_soc_pin_t;

/**
 * @brief Utility macro to initialize pinctrls of pinmux field in #pinctrl_pin_t.
 *
 * @param node_id Node identifier.
 */
#define Z_PINCTRL_CSK6_PICTRL_INIT(node_id)      \
    DEVICE_DT_GET_OR_NULL(DT_PHANDLE(node_id, pinctrls))

/**
 * @brief Utility macro to initialize pin of pinmux field in #pinctrl_pin_t.
 *
 * @param node_id Node identifier.
 */
#define Z_PINCTRL_CSK6_PIN_INIT(node_id)          \
	DT_PHA_OR(node_id, pinctrls, pin, 0)
/**
 * @brief Utility macro to initialize alt_func of pinmux field in #pinctrl_pin_t.
 *
 * @param node_id Node identifier.
 */
#define Z_PINCTRL_CSK6_ALT_INIT(node_id)          \
	DT_PHA_OR(node_id, pinctrls, alt_func, 0)

/**
 * @brief Utility macro to initialize each pin.
 *
 * @param node_id Node identifier.
 * @param state_prop State property name.
 * @param idx State property entry index.
 */
#define Z_PINCTRL_STATE_PIN_INIT(node_id, state_prop, idx)		       \
    {                                                                  \
	    .pinctrls = Z_PINCTRL_CSK6_PICTRL_INIT(                        \
		    DT_PROP_BY_IDX(node_id, state_prop, idx)),                 \
        .pin = Z_PINCTRL_CSK6_PIN_INIT(                                \
            DT_PROP_BY_IDX(node_id, state_prop, idx)),                 \
        .alt_func = Z_PINCTRL_CSK6_ALT_INIT(                           \
            DT_PROP_BY_IDX(node_id, state_prop, idx))                  \
	},                                                                

		
/**
 * @brief Utility macro to initialize state pins contained in a given property.
 *
 * @param node_id Node identifier.
 * @param prop Property name describing state pins.
 */
#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop)			       \
	{DT_FOREACH_PROP_ELEM(node_id, prop, Z_PINCTRL_STATE_PIN_INIT)}

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SOC_ARM_CSK6_PINCTRL_SOC_H_ */
