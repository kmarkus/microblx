set(MOD_NAME trig)

set(MOD_SRC_DIR ${CMAKE_SOURCE_DIR}/std_blocks/${MOD_NAME})
set(MOD_BUILD_DIR ${CMAKE_BINARY_DIR}/std_blocks/${MOD_NAME})

include(CheckCSourceCompiles)

check_c_source_compiles("
#define _GNU_SOURCE
#include <pthread.h>
int main(void) { struct sched_attr a; (void)a.size; return 0; }
" HAVE_STRUCT_SCHED_ATTR)

check_c_source_compiles("
#define _GNU_SOURCE
#include <sched.h>
int main(void) { return SCHED_FLAG_DL_OVERRUN; }
" HAVE_SCHED_FLAG_DL_OVERRUN)

file(MAKE_DIRECTORY ${MOD_BUILD_DIR}/types)

set(MOD_HDRS
  ${MOD_SRC_DIR}/types/ptrig_period.h
  ${MOD_SRC_DIR}/types/ptrig_deadline.h)

set(MOD_HDRS_GEN
  ${MOD_BUILD_DIR}/types/ptrig_period.h.hexarr
  ${MOD_BUILD_DIR}/types/ptrig_deadline.h.hexarr)

generate_hexarr(${MOD_SRC_DIR}/types/ptrig_period.h   ${MOD_BUILD_DIR}/types/ptrig_period.h.hexarr)
generate_hexarr(${MOD_SRC_DIR}/types/ptrig_deadline.h ${MOD_BUILD_DIR}/types/ptrig_deadline.h.hexarr)

set(COMMON_SRCS ${MOD_SRC_DIR}/common.c ${MOD_SRC_DIR}/common.h )

# trig
add_library(trig MODULE  ${MOD_SRC_DIR}/trig.c ${COMMON_SRCS})
target_include_directories(trig PRIVATE ${CMAKE_SOURCE_DIR}/libubx/ ${MOD_BUILD_DIR} )
target_compile_options(trig PRIVATE ${UBX_MODE_CFLAGS})
target_link_libraries(trig PRIVATE ubx)

set_target_properties(trig
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME "trig"
)

# ptrig
add_library(ptrig MODULE  ${MOD_SRC_DIR}/ptrig.c ${COMMON_SRCS} ${MOD_HDRS_GEN})
target_include_directories(ptrig PRIVATE ${CMAKE_SOURCE_DIR}/libubx/ ${MOD_BUILD_DIR} )
target_compile_options(ptrig PRIVATE ${UBX_MODE_CFLAGS})
target_link_libraries(ptrig PRIVATE ubx)

if(HAVE_STRUCT_SCHED_ATTR)
  target_compile_definitions(ptrig PRIVATE HAVE_STRUCT_SCHED_ATTR)
endif()
if(HAVE_SCHED_FLAG_DL_OVERRUN)
  target_compile_definitions(ptrig PRIVATE HAVE_SCHED_FLAG_DL_OVERRUN)
endif()

set_target_properties(ptrig
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
  OUTPUT_NAME "ptrig"
  PUBLIC_HEADER "${MOD_HDRS}"
)

install(
  TARGETS trig ptrig
  LIBRARY DESTINATION ${UBX_MOD_DIR} COMPONENT libraries
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev
)
