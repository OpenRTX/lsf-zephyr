# SPDX-License-Identifier: Apache-2.0

if(CONFIG_USB_DEVICE_DRIVER)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_USB_DC_CSK6 	 usb_dc_csk6.c)

endif()
