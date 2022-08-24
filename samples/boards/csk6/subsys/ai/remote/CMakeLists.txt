cmake_minimum_required(VERSION 3.20.0)

# SPDX-License-Identifier: Apache-2.0
#

if("${BOARD}" STREQUAL "ls_adsp_csk6")
set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc-clang")
endif()

if("${BOARD}" STREQUAL "ls_adsp_csk6")
	message(INFO " ${REMOTE_ZEPHYR_DIR} compiles as remote in this sample")
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
  ./include/pilot/
  ./include/pilot_app/
)

# set_target_properties(app PROPERTIES IMPORTED_LOCATION ./lib/libpilot.a)

# ./lib/libopi_cpy.a
# ./lib/libpilot.a
# ./lib/libthinker.a
                                  
# zephyr_link_libraries(ALGO_LIB ./lib/libluna.a
#                               ./lib/libopi_cpy.a
#                               ./lib/libpilot.a
#                               ./lib/libthinker.a
# )
# zephyr_ld_options(
#   -lm
# )

# zephyr_library_import(algo_luna ./lib/libluna.a)
# zephyr_library_import(algo_opi_cpy ./lib/libopi_cpy.a)
# zephyr_library_import(algo_pilot ./lib/libpilot.a)
# zephyr_library_import(algo_thinker ./lib/libthinker.a)
target_sources(app PRIVATE
    src/main.c
    src/opi_psram_cpy.c
    src/pilot_app.c
  )


set(LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/lib)

target_link_libraries(app PUBLIC -Wl,--start-group 
						 -lgcc 
						 -lc 
						 -lm 
						 -lgloss 
						 -lminrt 
						 ${LIB_DIR}/libopi_cpy.a 
             			 ${LIB_DIR}/libhifi_dsp.a 
						 ${LIB_DIR}/libluna.a 
						 ${LIB_DIR}/libthinker.a 
						 ${LIB_DIR}/libpilot.a 
						 -Wl,--end-group)
