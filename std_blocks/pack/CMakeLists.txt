set(MOD_NAME pack)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})

add_library(${MOD_NAME} MODULE ${MOD_SRC_DIR}/pack.c)

target_compile_options(${MOD_NAME} PRIVATE ${UBX_MOD_CFLAGS})
target_link_libraries(${MOD_NAME} PRIVATE ubx lfb rt)

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
)

install(
  TARGETS ${MOD_NAME}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)

install(FILES ${MOD_SRC_DIR}/pack_layout.h ${MOD_SRC_DIR}/pack_shm.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev)
