set(MOD_NAME rand)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

set(MOD_SRCS ${MOD_SRC_DIR}/rand.c)

# helper to compile rand variants
function(add_rand name def)
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

add_rand(rand_float RAND_FLOAT_T=1)
add_rand(rand_double RAND_DOUBLE_T=1)
add_rand(rand_uint32  RAND_UINT32_T=1)
add_rand(rand_int32 RAND_INT32_T=1)
