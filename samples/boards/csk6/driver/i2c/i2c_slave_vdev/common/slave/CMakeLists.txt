# SPDX-License-Identifier: Apache-2.0

# zephyr_library_sources_ifdef(CONFIG_VIRTUAL_I2C_SLAVE_VIRTUAL_DEVICE	i2c_slave_vdev.c)

if(CONFIG_I2C_SLAVE_VIRTUAL_DEVICE)
  # Add i2c_slave_vdev.h to the set of global include paths.
  zephyr_include_directories(.)

  zephyr_library()
  zephyr_library_sources(
    i2c_slave_vdev.c
  )
endif()

