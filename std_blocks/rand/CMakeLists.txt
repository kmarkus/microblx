set(MOD_NAME rand)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

add_library(rand MODULE ${MOD_SRC_DIR}/rand_generic.c)

target_include_directories(rand
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
)

target_compile_options(rand PRIVATE ${UBX_MOD_CFLAGS} )
target_link_libraries(rand PRIVATE ubx)

set_target_properties(rand
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME rand
)

install(
  TARGETS rand
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
