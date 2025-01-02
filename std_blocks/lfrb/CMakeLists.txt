set(MOD_NAME lfrb)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

set(MOD_SRCS ${MOD_SRC_DIR}/lfrb.c)

add_library(${MOD_NAME} MODULE ${MOD_SRCS})

target_include_directories(${MOD_NAME}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
  ${LFQ_SRC_DIR}
)

target_compile_options(${MOD_NAME} PRIVATE ${UBX_MOD_CFLAGS} )

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
)

target_link_libraries(${MOD_NAME} PRIVATE ubx lfq)

install(
  TARGETS ${MOD_NAME}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
