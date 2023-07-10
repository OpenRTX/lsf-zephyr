# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__i2c)

target_sources_ifdef(CONFIG_I2C_CSK6 ${TARGET_LIB} PRIVATE i2c_csk6.c)
