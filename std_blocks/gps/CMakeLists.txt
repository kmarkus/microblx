set(MOD_NAME gps)
set(MOD_TARGET ubx_gps)  # avoid clash with the 'gps' import library

pkg_check_modules(LIBGPS libgps)

if(NOT LIBGPS_FOUND)
  message(WARNING "libgps not found, skipping ubx/gps block")
  return()
endif()

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS ${MOD_SRC_DIR}/types/gps_data.h)
set(MOD_HDRS_GEN ${MOD_BUILD_DIR}/types/gps_data.h.hexarr)

generate_hexarr(${MOD_SRC_DIR}/types/gps_data.h ${MOD_BUILD_DIR}/types/gps_data.h.hexarr)

add_library(${MOD_TARGET} MODULE
  ${MOD_SRC_DIR}/gps.c
  ${MOD_HDRS}
  ${MOD_HDRS_GEN}
)

target_include_directories(${MOD_TARGET}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
  ${LIBGPS_INCLUDE_DIRS}
)

target_compile_options(${MOD_TARGET} PRIVATE ${UBX_MOD_CFLAGS})
target_link_libraries(${MOD_TARGET} PRIVATE ubx ${LIBGPS_LIBRARIES})

set_target_properties(${MOD_TARGET}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
)

install(
  TARGETS ${MOD_TARGET}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
