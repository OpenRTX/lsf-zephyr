if(${CONFIG_SOC_LS_ADSP_CSK6})
#zephyr_library()
zephyr_include_directories(./)
zephyr_library_sources(./algo_lib.c)
else()
    message(FATAL_ERROR 'Unsupport the platform')
endif()	


