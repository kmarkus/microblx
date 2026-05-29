set(MOD_NAME luablock)

pkg_check_modules(LUAJIT luajit>=2.0.4)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})
set(MOD_SRCS ${MOD_SRC_DIR}/luablock.c)

add_library(${MOD_NAME} MODULE ${MOD_SRCS} ${MOD_HDRS} ${MOD_HDRS_GEN})

target_include_directories(${MOD_NAME}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
  ${LUAJIT_INCLUDE_DIRS}
)

target_compile_options(${MOD_NAME} PRIVATE ${UBX_MOD_CFLAGS} )

# name the self-trigger thread after the block (see luablock_start)
if(HAVE_PTHREAD_SETNAME_NP)
  target_compile_definitions(${MOD_NAME} PRIVATE CONFIG_PTHREAD_SETNAME _GNU_SOURCE)
endif()

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

install(FILES luablock-util.lua
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/lua/${LUA_ABIVER}/ubx/)


if(INSTALL_EXAMPLES)
  install(FILES luablock-example.lua DESTINATION ${UBX_LUABLOCK_DIR})
endif()
