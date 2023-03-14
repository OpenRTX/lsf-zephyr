/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "venus_ap.h"

#include <zephyr/drivers/onewire/infrared/infrared_csk6.h>
#include <string.h>
#include <zephyr/init.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(csk6_infrared, CONFIG_LOG_DEFAULT_LEVEL);

/* CSK6 infrared driver */
