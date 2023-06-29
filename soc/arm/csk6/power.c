/*
 * Copyright (c) 2023 LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Driver_PWR.h"

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>

LOG_MODULE_REGISTER(soc, CONFIG_SOC_LOG_LEVEL);

static void *pwr = NULL;

static int pm_state_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	pwr = PWR();
	HAL_PWR_Initialize(pwr);
	HAL_PWR_Control(pwr, CSK_POWER_FULL);

	int32_t cause = HAL_PWR_GetWakeCause(pwr);
	LOG_DBG("System init with wake cause: %x", cause);

	return 0;
}

__weak void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	LOG_DBG("Entering power state: %d", state);

	switch (state) {
	case PM_STATE_SOFT_OFF:
		k_cpu_idle();
		HAL_PWR_EnterLightSleepMode(pwr, PWR_LIGHTSLEEPENTRY_WFI);
		break;
	default:
		LOG_DBG("Unsupported power state %u", state);
		break;
	}
}

__weak void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);

	/*
	 * System is now in active mode. Reenable interrupts which were disabled
	 * when OS started idling code.
	 */
	irq_unlock(0);
}

SYS_INIT(pm_state_init, PRE_KERNEL_1, 0);
