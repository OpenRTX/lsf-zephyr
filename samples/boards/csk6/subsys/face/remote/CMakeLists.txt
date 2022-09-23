cmake_minimum_required(VERSION 3.20.0)

# SPDX-License-Identifier: Apache-2.0
#

if("${BOARD}" STREQUAL "ls_adsp_csk6")
set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc-clang")
endif()

if("${BOARD}" STREQUAL "ls_adsp_csk6")
	message(STATUS " ${REMOTE_ZEPHYR_DIR} compiles as remote in this sample")
else()
	message(FATAL_ERROR "${BOARD} was not supported for this sample")
endif()
find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sof_remote)


zephyr_include_directories(
  ./include
  ./include/luna/
  ./include/hifi/
  ./include/opi_cpy/
  ./include/face/
  ./include/img/
)

if(NOT CONFIG_ALGO_RES_BIN)
	message(STATUS "CONFIG_ALGO_RES_BIN is not defined")
	zephyr_include_directories(./include/res/)
else()
message(STATUS "generating inc file")
	set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

	foreach(inc_file
		gesture.bin
		head_shoulder_detect.bin
		)
	generate_inc_file_for_target(
		app
		res/${inc_file}
		${gen_dir}/${inc_file}.inc
		)
	endforeach()

endif()

target_sources(app PRIVATE
    src/main.c
    src/opi_psram_cpy.c
    src/main_app.c
  )


set(LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/lib)

target_link_libraries(app PUBLIC -Wl,--start-group 
						 -lgcc 
						 -lc 
						 -lm 
						 -lgloss 
						 -lminrt 
						#  ${LIB_DIR}/libopi_cpy.a 
             			 		 ${LIB_DIR}/libhifi_dsp.a 
						 ${LIB_DIR}/libluna.a 
						 ${LIB_DIR}/libthinker.a 
						 ${LIB_DIR}/libface.a 
						 -Wl,--end-group)

