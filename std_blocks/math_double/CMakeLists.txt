set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/math_double)
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/math_double)

set(MOD_SRCS ${MOD_SRC_DIR}/math_double.c)

# helper to compile math variants
function(add_math name type)
  add_library(${name} MODULE ${MOD_SRCS})

  target_include_directories(${name}
    PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
    ${MOD_BUILD_DIR}
  )

  target_compile_definitions(${name} PRIVATE -DMATH_T=${type})
  target_compile_options(${name} PRIVATE ${UBX_MOD_CFLAGS})
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

add_math(math_double double)
add_math(math_float float)
