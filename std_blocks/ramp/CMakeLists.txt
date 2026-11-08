set(MOD_NAME ramp)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

add_library(ramp MODULE ${MOD_SRC_DIR}/ramp_generic.c)

target_include_directories(ramp
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
)

target_compile_options(ramp PRIVATE ${UBX_MOD_CFLAGS} )
target_link_libraries(ramp PRIVATE ubx)

set_target_properties(ramp
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ramp
)

install(
  TARGETS ramp
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
