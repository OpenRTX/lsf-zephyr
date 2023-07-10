set(CSK_SDK ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(drivers)

if(CONFIG_LEGACY_INCLUDE_PATH)
  zephyr_include_directories(include/zephyr)
endif()

zephyr_include_directories(
  include
  ${PROJECT_BINARY_DIR}/include/generated
  ${USERINCLUDE}
  ${STDINCLUDE}
)
