# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__pwm)

target_sources_ifdef(CONFIG_PWM_CSK6 ${TARGET_LIB} PRIVATE pwm_csk6.c)
target_sources_ifdef(CONFIG_PWM_CSK6_CH32V003 ${TARGET_LIB} PRIVATE pwm_csk6_ch32v003.c)
