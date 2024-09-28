set(MOD_NAME cppdemo)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS ${MOD_SRC_DIR}/types/cpp_demo_type.h )
set(MOD_HDRS_GEN ${MOD_BUILD_DIR}/types/cpp_demo_type.h.hexarr)

generate_hexarr(${MOD_SRC_DIR}/types/cpp_demo_type.h ${MOD_BUILD_DIR}/types/cpp_demo_type.h.hexarr)

set(MOD_SRCS ${MOD_SRC_DIR}/cppdemo.cpp)

add_library(${MOD_NAME} MODULE ${MOD_SRCS} ${MOD_HDRS} ${MOD_HDRS_GEN})

target_include_directories(${MOD_NAME}
  PRIVATE ${CMAKE_SOURCE_DIR}/libubx/
  ${MOD_BUILD_DIR}
)

target_compile_options(${MOD_NAME}
  PRIVATE
  ${UBX_MOD_CFLAGS}
  -Wno-missing-field-initializers
)

set_target_properties(${MOD_NAME}
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME ${MOD_NAME}
)

target_link_libraries(${MOD_NAME} PRIVATE ubx)

install(
  TARGETS ${MOD_NAME}
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
)
