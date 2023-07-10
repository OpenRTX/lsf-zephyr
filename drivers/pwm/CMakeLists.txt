# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_PWM_CSK6   pwm_csk6.c)
zephyr_library_sources_ifdef(CONFIG_PWM_CSK6_CH32V003   pwm_csk6_ch32v003.c)
