# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_CPU_CORTEX_M common/cortex_m)
add_subdirectory_ifdef(CONFIG_CSK6_EXMCU csk6/exmcu)

if(SOC_FAMILY)
  add_subdirectory(${SOC_FAMILY})
else()
  add_subdirectory(${SOC_NAME})
endif()

