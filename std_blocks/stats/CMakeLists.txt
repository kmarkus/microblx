set(MOD_NAME stats)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS ${MOD_SRC_DIR}/types/ubx_stat.h)
set(MOD_HDRS_GEN ${MOD_BUILD_DIR}/types/ubx_stat.h.hexarr)

generate_hexarr(${MOD_SRC_DIR}/types/ubx_stat.h ${MOD_BUILD_DIR}/types/ubx_stat.h.hexarr)

set(MOD_SRCS ${MOD_SRC_DIR}/stats.c)

add_library(${MOD_NAME} MODULE ${MOD_SRCS} ${MOD_HDRS} ${MOD_HDRS_GEN})

target_include_directories(${MOD_NAME}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
)

target_compile_options(${MOD_NAME} PRIVATE ${UBX_MOD_CFLAGS} )
target_link_libraries(${MOD_NAME} PRIVATE ubx m)

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
  PUBLIC_HEADER "${MOD_HDRS}"
)

install(
  TARGETS ${MOD_NAME}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev
)
