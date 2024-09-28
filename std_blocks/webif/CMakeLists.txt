set(MOD_NAME webif)

pkg_check_modules(LUAJIT luajit>=2.0.4)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

generate_hexarr(${MOD_SRC_DIR}/webif.lua ${MOD_BUILD_DIR}/webif.lua.hexarr)

# module
set(MOD_SRCS
  ${MOD_SRC_DIR}/webif.c
  ${MOD_SRC_DIR}/mongoose.c
  ${MOD_SRC_DIR}/mongoose.h
  ${MOD_BUILD_DIR}/webif.lua.hexarr
)

add_library(${MOD_NAME} MODULE ${MOD_SRCS})

target_include_directories(${MOD_NAME}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
  ${LUAJIT_INCLUDE_DIRS}
)

target_compile_options(${MOD_NAME} PRIVATE ${UBX_MOD_CFLAGS} )

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
)

target_link_libraries(${MOD_NAME} PRIVATE ubx ${LUAJIT_LIBRARIES})

install(
  TARGETS ${MOD_NAME}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
