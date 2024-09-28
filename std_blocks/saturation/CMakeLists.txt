set(MOD_NAME saturation)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

set(MOD_SRCS ${MOD_SRC_DIR}/saturation.c)

# helper to compile saturation variants
function(add_saturation name def)
  add_library(${name} MODULE ${MOD_SRCS})

  target_include_directories(${name}
    PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
    ${MOD_BUILD_DIR}
  )

  target_compile_definitions(${name} PRIVATE -D${def})
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

add_saturation(saturation_float SATURATION_FLOAT_T=1)
add_saturation(saturation_double SATURATION_DOUBLE_T=1)
add_saturation(saturation_int32  SATURATION_INT32_T=1)
add_saturation(saturation_int64 SATURATION_INT64_T=1)
