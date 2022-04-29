if(${CONFIG_SOC_CSK6001})
#zephyr_library()
zephyr_include_directories(./)
zephyr_library_sources(./remote_core_ctrl.c)
zephyr_library_sources(./image_ops_csk6001_if.c)
zephyr_library_sources(./rproc_csk6001_if.c)
else()
    message(FATAL_ERROR 'Unsupport the platform')
endif()	


