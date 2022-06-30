# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED CONFIG_BOOTLOADER_MCUBOOT)
  set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
    COMMAND ${PYTHON_EXECUTABLE} ${SOC_DIR}/arm/csk6/add_header.py
    ${PROJECT_BINARY_DIR}/${CONFIG_KERNEL_BIN_NAME}.bin
  )
endif()
