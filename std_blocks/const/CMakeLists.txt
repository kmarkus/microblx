set(MOD_NAME const)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

set(MOD_SRCS ${MOD_SRC_DIR}/const.c)

function(add_const name def)
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

add_const(cconst BUILD_CBLOCK)
add_const(iconst BUILD_IBLOCK)
