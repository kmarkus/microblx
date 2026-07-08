set(MOD_NAME ramp)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

set(MOD_SRCS ${MOD_SRC_DIR}/ramp.c)

# helper to compile ramp variants
function(add_ramp name type block_name)
  add_library(${name} MODULE ${MOD_SRCS})

  target_include_directories(${name}
    PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
    ${MOD_BUILD_DIR}
  )

  target_compile_definitions(${name} PRIVATE -DRAMP_T=${type} -DBLOCK_NAME=${block_name})
  target_compile_options(${name} PRIVATE ${UBX_MOD_CFLAGS} )
  target_link_libraries(${name} PRIVATE ubx)

  set_target_properties(${name}
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME ${name}
  )

  install(
    TARGETS ${name}
    LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
  )
endfunction()

add_ramp(ramp_float float "float")
add_ramp(ramp_double double "double")
add_ramp(ramp_uint8 uint8_t "uint8")
add_ramp(ramp_uint16 uint16_t "uint16")
add_ramp(ramp_uint32 uint32_t "uint32")
add_ramp(ramp_uint64 uint64_t "uint64")
add_ramp(ramp_int8 int8_t "int8")
add_ramp(ramp_int16 int16_t "int16")
add_ramp(ramp_int32 int32_t "int32")
add_ramp(ramp_int64 int64_t "int64")

# generic runtime-typed variant (ubx/ramp); supersedes the compile-time
# typed variants above, which are kept for backwards compatibility
add_library(ramp MODULE ${MOD_SRC_DIR}/ramp_generic.c)

target_include_directories(ramp
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
)

target_compile_options(ramp PRIVATE ${UBX_MOD_CFLAGS} )
target_link_libraries(ramp PRIVATE ubx m)

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
