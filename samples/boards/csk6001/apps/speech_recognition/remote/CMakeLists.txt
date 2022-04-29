cmake_minimum_required(VERSION 3.20.0)

# SPDX-License-Identifier: Apache-2.0
#

if("${BOARD}" STREQUAL "ls_adsp_csk6")
set(ENV{XCC_USE_CLANG} "1")
set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc")
endif()

if("${BOARD}" STREQUAL "ls_adsp_csk6")
	message(INFO " ${BOARD} compiles as remote in this sample")
else()
	message(FATAL_ERROR "${BOARD} was not supported for this sample")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(csk6_ai)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_subdirectory(./src)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ./src) 

set(LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/lib)
message(INFO " ${CMAKE_CURRENT_SOURCE_DIR} ABC")

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/opi_cpy/)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/luna/)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/cjson/)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/cae/)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/esr/)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/aiwrap/)

target_link_libraries(app PUBLIC -Wl,--start-group -lgcc -lc -lm -lgloss -lminrt ${LIB_DIR}/libopi_cpy.a ${LIB_DIR}/libcjson.a ${LIB_DIR}/libluna.a ${LIB_DIR}/libcae.a ${LIB_DIR}/libesr.a ${LIB_DIR}/libaiwrap.a -Wl,--end-group)
 


