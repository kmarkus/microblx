set(MOD_NAME stdtypes)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_types/stdtypes)
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_types/stdtypes)

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS
  ${MOD_SRC_DIR}/types/tstat.h
  ${MOD_SRC_DIR}/types/triggee.h
)

set(MOD_GEN_HDRS
  ${MOD_BUILD_DIR}/types/tstat.h.hexarr
  ${MOD_BUILD_DIR}/types/triggee.h.hexarr
)

add_library(${MOD_NAME} MODULE ${MOD_SRC_DIR}/stdtypes.c ${MOD_HDRS} ${MOD_GEN_HDRS})

generate_hexarr(${MOD_SRC_DIR}/types/tstat.h ${MOD_BUILD_DIR}/types/tstat.h.hexarr)
generate_hexarr(${MOD_SRC_DIR}/types/triggee.h ${MOD_BUILD_DIR}/types/triggee.h.hexarr)

target_include_directories(${MOD_NAME} PRIVATE ${MOD_BUILD_DIR} )
target_link_libraries(${MOD_NAME} PRIVATE ubx)

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME "${MOD_NAME}"
  PUBLIC_HEADER "${MOD_HDRS}"
)

install(TARGETS ${MOD_NAME}
  LIBRARY DESTINATION ${UBX_MOD_DIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev
)
