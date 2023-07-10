set(CSK_SDK ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(drivers)

if(CONFIG_LEGACY_INCLUDE_PATH)
  zephyr_include_directories(include/zephyr)
endif()

message(STATUS "This is csk sdk")
zephyr_include_directories(
  include
  ${PROJECT_BINARY_DIR}/include/generated
  ${USERINCLUDE}
  ${STDINCLUDE}
)

message(STATUS "csk sdk SYSCALL_INCLUDE_DIRS: ${SYSCALL_INCLUDE_DIRS}")
