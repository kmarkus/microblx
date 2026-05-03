cmake_minimum_required(VERSION 3.10)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 9)
set(VERSION_PATCH 2)

project(microblx VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" LANGUAGES C CXX)

include(GNUInstallDirs)

# module install path
set(UBX_MOD_DIR ${CMAKE_INSTALL_PREFIX}/lib/ubx/${VERSION_MAJOR}.${VERSION_MINOR}/)
set(UBX_LUABLOCK_DIR ${CMAKE_INSTALL_DATADIR}/ubx/blocks/${VERSION_MAJOR}.${VERSION_MINOR}/)
set(UBX_MOD_CFLAGS -fvisibility=hidden)
set(UBX_TOCARR ${CMAKE_SOURCE_DIR}/tools/ubx-tocarr)

add_compile_options(-W -Wextra -Wall -Werror)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# generate hexarr helper
function(generate_hexarr INPUT_HEADER OUTPUT_HEXARR)
  add_custom_command(
    OUTPUT ${OUTPUT_HEXARR}
    COMMAND ${UBX_TOCARR} -s ${INPUT_HEADER} -d ${OUTPUT_HEXARR}
    DEPENDS ${INPUT_HEADER}
  )
endfunction()

find_package(PkgConfig REQUIRED)
pkg_check_modules(CMOCKA cmocka>=1.1.5)
pkg_check_modules(LUAJIT REQUIRED luajit>=2.0.4)
pkg_get_variable(LUA_ABIVER luajit abiver)

# check for uthash
find_path(UTHASH_INCLUDE_DIRS
  NAMES uthash.h
  DOC "Directory containing uthash.h"
)

if(NOT UTHASH_INCLUDE_DIRS)
  message(FATAL_ERROR "Unable to find uthash headers (install uthash-dev)")
else()
  include_directories(${UTHASH_INCLUDE_DIRS})
endif()

#
# libubx
#
set(UBX_SRC_DIR ${CMAKE_SOURCE_DIR}/libubx/)

configure_file("${UBX_SRC_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/config.h")

set(UBX_SRCS
  ${UBX_SRC_DIR}/md5.c
  ${UBX_SRC_DIR}/ubx.c
  ${UBX_SRC_DIR}/ubx_time.c
  ${UBX_SRC_DIR}/ubx_utils.c
  ${UBX_SRC_DIR}/trig_utils.c
  ${UBX_SRC_DIR}/rtlog.c
  ${UBX_SRC_DIR}/accessors.c
)

include_directories(
  ${CMAKE_SOURCE_DIR}/std_types/stdtypes/types
  ${CMAKE_SOURCE_DIR}/libubx/
)

set(UBX_PUBLIC_HDRS
  ${UBX_SRC_DIR}/ubx.h
  ${UBX_SRC_DIR}/typemacros.h
  ${UBX_SRC_DIR}/accessors.h
  ${UBX_SRC_DIR}/ubx_core.h
  ${UBX_SRC_DIR}/ubx_types.h
  ${UBX_SRC_DIR}/ubx_time.h
  ${UBX_SRC_DIR}/ubx_uthash_ffi.h
  ${UBX_SRC_DIR}/trig_utils.h
  ${UBX_SRC_DIR}/md5.h
  ${UBX_SRC_DIR}/ubx_utils.h
  ${UBX_SRC_DIR}/rtlog.h
)

add_library(ubx SHARED ${UBX_SRCS})

# TIMESRC_TSC
option(ENABLE_TIMESRC_TSC "Use TSC as timesource instead of POSIX time" OFF)

if(ENABLE_TIMESRC_TSC)
  set(CPU_HZ 1600000000 CACHE STRING "CPU frequency in Hz for TSC to time calculation")
  target_compile_definitions(ubx PRIVATE -DTIMESRC_TSC=1 -DCPU_HZ=${CPU_HZ})
endif()

# TIMESRC_CNTVCT
option(ENABLE_TIMESRC_CNTVCT "Use  CNTVCT as timesource instead of POSIX time" OFF)

if(ENABLE_TIMESRC_CNTVCT)
  add_compile_definitions(TIMESRC_CNTVCT)
endif()

# for config.h
target_include_directories(ubx PRIVATE ${CMAKE_BINARY_DIR})

set_target_properties(ubx
  PROPERTIES
  VERSION "${PROJECT_VERSION}"
  SOVERSION "${VERSION_MAJOR}"
  PUBLIC_HEADER "${UBX_PUBLIC_HDRS}")

target_link_libraries(ubx dl rt)

install(TARGETS ubx
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev)

# pkg-config libubx
configure_file(${UBX_SRC_DIR}/ubx0.pc.in ubx0.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/ubx0.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig COMPONENT dev)

#
# rtlog-client
#

set(RTLOG_CLIENT_HDRS
  ${UBX_SRC_DIR}/rtlog_client.h
  ${UBX_SRC_DIR}/internal/rtlog_common.h
  ${UBX_SRC_DIR}/internal/rtlog_client.h
)

add_library(rtlog_client SHARED
  ${UBX_SRC_DIR}/rtlog_client.c
  ${RTLOG_CLIENT_HDRS}
)

set_target_properties(rtlog_client PROPERTIES VERSION "${PROJECT_VERSION}")
target_compile_options(rtlog_client PRIVATE -lrt)
install(TARGETS rtlog_client LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${UBX_SRC_DIR}/rtlog_client.h DESTINATION include/ubx/)

install(FILES
  ${UBX_SRC_DIR}/internal/rtlog_common.h
  ${UBX_SRC_DIR}/internal/rtlog_client.h
  DESTINATION include/ubx/internal)

# pkg-config real-time logging client lib
configure_file(${UBX_SRC_DIR}/ubxrtl0.pc.in ubxrtl0.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/ubxrtl0.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig COMPONENT dev)

# liblfq (lock free queue)
set(LFQ_SRC_DIR ${CMAKE_SOURCE_DIR}/liblfq/)
add_subdirectory(${LFQ_SRC_DIR})

#
# Lua modules
#
install(FILES
  lua/ubx.lua
  lua/umf.lua
  lua/cdata.lua
  lua/blockdiagram.lua
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/lua/${LUA_ABIVER}/
)

#
# Blocks
#

function(add_block_subdir option_name dir default_enabled)
  option(${option_name} "Build block ${dir}" ${default_enabled})
  if(${option_name})
    add_subdirectory(${CMAKE_SOURCE_DIR}/${dir})
  endif()
endfunction()

add_block_subdir(BLOCK_CONST       "std_blocks/const"       ON)
add_block_subdir(BLOCK_CPPDEMO     "std_blocks/cppdemo"     ON)
add_block_subdir(BLOCK_HEXDUMP     "std_blocks/hexdump"     ON)
if (NOT CMAKE_C_COMPILER_TARGET MATCHES "aarch64|arm64")
  add_block_subdir(BLOCK_LFDS_CYCLIC "std_blocks/lfds_cyclic" ON)
endif()
add_block_subdir(BLOCK_LFRB	   "std_blocks/lfrb"	    ON)
add_block_subdir(BLOCK_LUABLOCK    "std_blocks/luablock"    ON)
add_block_subdir(BLOCK_MATH_DOUBLE "std_blocks/math_double" ON)
add_block_subdir(BLOCK_MQUEUE      "std_blocks/mqueue"      ON)
add_block_subdir(BLOCK_PID         "std_blocks/pid"         ON)
add_block_subdir(BLOCK_RAMP        "std_blocks/ramp"        ON)
add_block_subdir(BLOCK_RAND        "std_blocks/rand"        ON)
add_block_subdir(BLOCK_SATURATION  "std_blocks/saturation"  ON)
add_block_subdir(BLOCK_SKELLETON   "std_blocks/skelleton"   ON)
add_block_subdir(BLOCK_THRESHOLD   "std_blocks/threshold"   ON)
add_block_subdir(BLOCK_TRIG        "std_blocks/trig"        ON)
add_block_subdir(BLOCK_WEBIF       "std_blocks/webif"       ON)
add_block_subdir(BLOCK_WEBGRAPH    "std_blocks/webgraph"    ON)
add_block_subdir(BLOCK_GPIO        "std_blocks/gpio"        ON)
add_block_subdir(BLOCK_LSDB_INTF   "std_blocks/lsdb-intf"   OFF)

# examples
add_block_subdir(BLOCK_EXAMPLE_RANDOM      "std_blocks/examples/random"      ON)
add_block_subdir(BLOCK_EXAMPLE_SIMPLE_FIFO "std_blocks/examples/simple_fifo" ON)

#
# Types
#

function(add_type_subdir option_name dir default_enabled)
  option(${option_name} "Build type module ${dir}" ${default_enabled})
  if(${option_name})
    add_subdirectory(${CMAKE_SOURCE_DIR}/${dir})
  endif()
endfunction()

add_type_subdir(TYPE_STDTYPES  "std_types/stdtypes" ON)
add_type_subdir(TYPE_TESTTYPES "std_types/testtypes" ON)

#
# Tools
#

add_executable(ubx-log tools/ubx-log.c)
target_link_libraries(ubx-log PRIVATE rt rtlog_client)
install(TARGETS ubx-log RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ubx-time-test tools/ubx-time-test.c)
target_link_libraries(ubx-time-test PRIVATE ubx)
install(TARGETS ubx-time-test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(PROGRAMS
  tools/ubx-ilaunch
  tools/ubx-launch
  tools/ubx-modinfo
  tools/ubx-tocarr
  tools/ubx-mq
  tools/ubx-genblock
  DESTINATION ${CMAKE_INSTALL_BINDIR})

#
# docs and examples
#

option(INSTALL_EXAMPLES "Install the examples" ON)

if(INSTALL_EXAMPLES)
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/examples DESTINATION ${CMAKE_INSTALL_DATADIR}/ubx)
endif()

option(BUILD_DOCS "Build documentation with Sphinx" OFF)

if(BUILD_DOCS)
  add_subdirectory(${CMAKE_SOURCE_DIR}/docs/user)
endif()

#
# tests
#
install(PROGRAMS
  tests/test_block_composition.lua
  tests/test_block_state.lua
  tests/test_block_state_errors.lua
  tests/test_blockdiagram.lua
  tests/test_cdata_tolua.lua
  tests/test_connection.lua
  tests/test_connection_errors.lua
  tests/test_data_errors.lua
  tests/test_data_init.lua
  tests/test_dyn_block_if.lua
  tests/test_lfrb.lua
  tests/test_math_double.lua
  tests/test_md5.lua
  tests/test_merge.lua
  tests/test_node_config.lua
  tests/test_pid.lua
  tests/test_ptrig.lua
  tests/test_saturation.lua
  tests/test_struct_comm.lua
  tests/test_threshold.lua
  tests/test_time_arith.lua
  tests/test_type_lookup.lua
  DESTINATION ${CMAKE_INSTALL_BINDIR})


#
# cppcheck
#
add_custom_target(cppcheck
  COMMAND cppcheck -q
		  --enable=all
		  --check-level=exhaustive
		  -Ilibubx/ -DRAMP_T=int -DRAND_DOUBLE_T=1
		  --suppress=missingInclude
		  --suppress=missingIncludeSystem
		  --suppress=unusedFunction
		  --suppress=constParameterCallback
		  --suppress=*:std_blocks/webif/mongoose.c
		  --suppress=variableScope:libubx/md5.c .
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running cppcheck..."
  VERBATIM
)
