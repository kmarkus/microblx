cmake_minimum_required(VERSION 3.10)
project(scale C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UBX REQUIRED ubx0)
pkg_get_variable(UBX_MODDIR_DEFAULT ubx0 UBX_MODDIR)
set(UBX_MODDIR ${UBX_MODDIR_DEFAULT} CACHE PATH "module install dir")
find_program(UBX_TOCARR ubx-tocarr REQUIRED)

# type headers are embedded as char arrays for reflection
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/types)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/types/scale_config.h.hexarr
  COMMAND ${UBX_TOCARR} -s ${CMAKE_SOURCE_DIR}/types/scale_config.h
                        -d ${CMAKE_BINARY_DIR}/types/scale_config.h.hexarr
  DEPENDS ${CMAKE_SOURCE_DIR}/types/scale_config.h)

add_library(scale MODULE scale.c ${CMAKE_BINARY_DIR}/types/scale_config.h.hexarr)
set_target_properties(scale PROPERTIES PREFIX "")
target_include_directories(scale PRIVATE ${UBX_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_compile_options(scale PRIVATE ${UBX_CFLAGS_OTHER} -fvisibility=hidden)
target_link_libraries(scale PRIVATE ${UBX_LINK_LIBRARIES})

install(TARGETS scale LIBRARY DESTINATION ${UBX_MODDIR})
