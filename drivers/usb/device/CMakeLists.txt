# SPDX-License-Identifier: Apache-2.0

if(CONFIG_USB_DEVICE_DRIVER)

set(TARGET_LIB drivers__usb__device)

target_sources_ifdef(CONFIG_USB_DC_CSK6 ${TARGET_LIB} PRIVATE usb_dc_csk6.c)

endif()
