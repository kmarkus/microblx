set(MOD_NAME iio)
set(MOD_TARGET ubx_iio)  # avoid clash with the 'iio' import library

pkg_check_modules(LIBIIO libiio>=0.21)

if(NOT LIBIIO_FOUND)
  message(WARNING "libiio >= 0.21 not found, skipping ubx/iio block")
  return()
endif()

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS ${MOD_SRC_DIR}/types/iio_config.h)
set(MOD_HDRS_GEN ${MOD_BUILD_DIR}/types/iio_config.h.hexarr)

generate_hexarr(${MOD_SRC_DIR}/types/iio_config.h ${MOD_BUILD_DIR}/types/iio_config.h.hexarr)

set(MOD_SRCS ${MOD_SRC_DIR}/iio.c)

add_library(${MOD_TARGET} MODULE ${MOD_SRCS} ${MOD_HDRS} ${MOD_HDRS_GEN})

target_include_directories(${MOD_TARGET}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
  ${LIBIIO_INCLUDE_DIRS}
)

target_compile_options(${MOD_TARGET} PRIVATE ${UBX_MOD_CFLAGS})
target_link_libraries(${MOD_TARGET} PRIVATE ubx ${LIBIIO_LIBRARIES})

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

#
# ubx/iio_buf — buffered IIO block (shares type and hexarr with ubx/iio)
#

set(BUF_TARGET ubx_iio_buf)

add_library(${BUF_TARGET} MODULE
  ${MOD_SRC_DIR}/iio_buf.c
  ${MOD_HDRS}
  ${MOD_HDRS_GEN}
)

target_include_directories(${BUF_TARGET}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
  ${LIBIIO_INCLUDE_DIRS}
)

target_compile_options(${BUF_TARGET} PRIVATE ${UBX_MOD_CFLAGS})
target_link_libraries(${BUF_TARGET} PRIVATE ubx ${LIBIIO_LIBRARIES})

set_target_properties(${BUF_TARGET}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME "iio_buf"
)

install(
  TARGETS ${BUF_TARGET}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
