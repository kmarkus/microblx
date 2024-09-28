set(MOD_NAME testtypes)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_types/testtypes)
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_types/testtypes)

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS
  ${MOD_SRC_DIR}/types/kdl_frame.h
  ${MOD_SRC_DIR}/types/kdl_rotation.h
  ${MOD_SRC_DIR}/types/kdl_vector.h
  ${MOD_SRC_DIR}/types/test_trig_conf.h
)

set(MOD_GEN_HDRS
  ${MOD_BUILD_DIR}/types/kdl_frame.h.hexarr
  ${MOD_BUILD_DIR}/types/kdl_rotation.h.hexarr
  ${MOD_BUILD_DIR}/types/kdl_vector.h.hexarr
  ${MOD_BUILD_DIR}/types/test_trig_conf.h.hexarr
)

add_library(${MOD_NAME} MODULE
  ${MOD_SRC_DIR}/testtypes.c
  ${MOD_HDRS}
  ${MOD_GEN_HDRS}
)

generate_hexarr(${MOD_SRC_DIR}/types/kdl_frame.h      ${MOD_BUILD_DIR}/types/kdl_frame.h.hexarr)
generate_hexarr(${MOD_SRC_DIR}/types/kdl_rotation.h   ${MOD_BUILD_DIR}/types/kdl_rotation.h.hexarr)
generate_hexarr(${MOD_SRC_DIR}/types/kdl_vector.h     ${MOD_BUILD_DIR}/types/kdl_vector.h.hexarr)
generate_hexarr(${MOD_SRC_DIR}/types/test_trig_conf.h ${MOD_BUILD_DIR}/types/test_trig_conf.h.hexarr)

target_include_directories(${MOD_NAME} PRIVATE ${MOD_BUILD_DIR})
target_link_libraries(${MOD_NAME} PRIVATE ubx)

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
)

install(TARGETS ${MOD_NAME} LIBRARY DESTINATION ${UBX_MOD_DIR})
