/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include <device.h>

#ifndef _LISTENAI_CSK6001_SOC_DT_H_
#define _LISTENAI_CSK6001_SOC_DT_H_

#define CSK6_DEV_PINMUX(idx, inst)    DEVICE_DT_GET(DT_PHANDLE(DT_INST_PINCTRL_0(inst, idx), pinctrls))

#define CSK6_DEV_PIN(idx, inst)       DT_PHA(DT_INST_PINCTRL_0(inst, idx), pinctrls, pin)

#define CSK6_DEV_ALT_FUNC(idx, inst)  DT_PHA(DT_INST_PINCTRL_0(inst, idx), pinctrls, alt_func)


/**
 * @brief structure to convey pinctrl information for csk6 soc value
 */
struct csk6_pinctrl_cfg {
	/* Pinmux control group */
	const struct device *pinctrls;
	/* GPIO pin */
	uint8_t pin;
	/* Alternate function */
	uint8_t alt_fun;
};

#define PINCTRL_CONFIG_PINS(list, cnt)						\
	do {													\
		for(uint8_t i = 0; i < cnt; i++)					\
		{													\
			pinmux_pin_set(list[i].pinctrls, 				\
						list[i].pin, 						\
						list[i].alt_fun);					\
		}  													\
	} while (0)												

/**
 * @brief Macro function to construct csk6 alt item in UTIL_LISTIFY extension.
 *
 * @param idx index in UTIL_LISTIFY extension.
 * @param inst instance number for compatible defined in DT_DRV_COMPAT.
 * @return macro function to construct a csk6 alt structure.
 */
#define CSK6_DT_ALT_ITEMS_FUNC(idx, inst)                 \
	{                                                    \
		.pinctrls = CSK6_DEV_PINMUX(idx, inst),   \
		.pin = CSK6_DEV_PIN(idx, inst),           \
		.alt_fun = CSK6_DEV_ALT_FUNC(idx, inst),  \
	},

/**
 * @brief Macro function to construct a list of csk6 alt items with
 * compatible defined in DT_DRV_COMPAT by UTIL_LISTIFY func
 *
 * @param inst instance number for compatible defined in DT_DRV_COMPAT.
 * @return an array of csk6 alt items.
 */
#define CSK6_DT_ALT_ITEMS_LIST(inst) {			\
	UTIL_LISTIFY(DT_INST_NUM_PINCTRLS_BY_IDX(inst, 0),	\
		     CSK6_DT_ALT_ITEMS_FUNC,			\
		     inst)					\
	}

#endif /* _LISTENAI_CSK6001_SOC_DT_H_ */