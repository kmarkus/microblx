cmake_minimum_required(VERSION 3.10)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 9)
set(VERSION_PATCH 2)

project(microblx VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" LANGUAGES C CXX)

# Default to an optimized build (-O2 -g) when no build type was
# selected. Without this a plain 'cmake ..' compiles at -O0. Users can
# still override, e.g. -DCMAKE_BUILD_TYPE=Debug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type (default: RelWithDebInfo)" FORCE)
endif()

# git-describe version string embedded at configure time
execute_process(
  COMMAND git describe --tags --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT GIT_VERSION)
  set(GIT_VERSION "unknown")
endif()

include(GNUInstallDirs)

# module install path
set(UBX_MOD_DIR ${CMAKE_INSTALL_PREFIX}/lib/ubx/${VERSION_MAJOR}.${VERSION_MINOR}/)
set(UBX_LUABLOCK_DIR ${CMAKE_INSTALL_DATADIR}/ubx/blocks/${VERSION_MAJOR}.${VERSION_MINOR}/)
set(UBX_MOD_CFLAGS -fvisibility=hidden)
set(UBX_TOCARR ${CMAKE_SOURCE_DIR}/tools/ubx-tocarr)

add_compile_options(-W -Wextra -Wall -Werror -Wshadow -Wformat-security -Wnull-dereference)

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

# tracing instrumentation (see docs in libubx/ubx_trace.h)
set(TRACING "OFF" CACHE STRING "Tracing backend: OFF (default), SDT (USDT probes), MARKER (ftrace trace_marker)")
set_property(CACHE TRACING PROPERTY STRINGS OFF SDT MARKER)

if(TRACING STREQUAL "SDT")
  include(CheckIncludeFile)
  check_include_file("sys/sdt.h" HAVE_SYS_SDT_H)
  if(NOT HAVE_SYS_SDT_H)
    message(FATAL_ERROR "TRACING=SDT requires sys/sdt.h (install systemtap-sdt-dev)")
  endif()
  add_compile_definitions(CONFIG_TRACE_SDT)
elseif(TRACING STREQUAL "MARKER")
  add_compile_definitions(CONFIG_TRACE_MARKER)
elseif(NOT TRACING STREQUAL "OFF")
  message(FATAL_ERROR "invalid TRACING value '${TRACING}' (must be OFF, SDT or MARKER)")
endif()

# pthread_setname_np (GNU extension) for naming block threads
include(CheckSymbolExists)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
set(CMAKE_REQUIRED_LIBRARIES pthread)
check_symbol_exists(pthread_setname_np "pthread.h" HAVE_PTHREAD_SETNAME_NP)
unset(CMAKE_REQUIRED_DEFINITIONS)
unset(CMAKE_REQUIRED_LIBRARIES)

#
# libubx
#
set(UBX_SRC_DIR ${CMAKE_SOURCE_DIR}/libubx/)

configure_file("${UBX_SRC_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/config.h")

# Max length of a log message. The default (115) sizes struct
# ubx_log_msg to 192 bytes = 3 cache lines; keep it 64*k - 77 (51,
# 115, 179, ...) so the frame stays a cache-line multiple (asserted in
# rtlog.c). ubx_types.h is generated from ubx_types.h.in so the value
# is visible to both the C build and the luajit FFI (which reads the
# installed header).
set(UBX_LOG_MSG_MAXLEN 115 CACHE STRING "max ubx log message length")
configure_file("${UBX_SRC_DIR}/ubx_types.h.in"
  "${CMAKE_BINARY_DIR}/libubx/ubx_types.h" @ONLY)

set(UBX_SRCS
  ${UBX_SRC_DIR}/md5.c
  ${UBX_SRC_DIR}/ubx.c
  ${UBX_SRC_DIR}/ubx_time.c
  ${UBX_SRC_DIR}/ubx_utils.c
  ${UBX_SRC_DIR}/trig_utils.c
  ${UBX_SRC_DIR}/rtlog.c
  ${UBX_SRC_DIR}/accessors.c
)

if(TRACING STREQUAL "MARKER")
  list(APPEND UBX_SRCS ${UBX_SRC_DIR}/ubx_trace.c)
endif()

include_directories(
  ${CMAKE_BINARY_DIR}/libubx        # generated ubx_types.h
  ${CMAKE_SOURCE_DIR}/std_types/stdtypes/types
  ${CMAKE_SOURCE_DIR}/libubx/
  ${CMAKE_SOURCE_DIR}/liblfb/
)

set(UBX_PUBLIC_HDRS
  ${UBX_SRC_DIR}/ubx.h
  ${UBX_SRC_DIR}/typemacros.h
  ${UBX_SRC_DIR}/accessors.h
  ${UBX_SRC_DIR}/ubx_core.h
  ${CMAKE_BINARY_DIR}/libubx/ubx_types.h
  ${UBX_SRC_DIR}/ubx_time.h
  ${UBX_SRC_DIR}/ubx_uthash_ffi.h
  ${UBX_SRC_DIR}/trig_utils.h
  ${UBX_SRC_DIR}/ubx_trace.h
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

# liblfb (lock free broadcast buffer)
set(LFB_SRC_DIR ${CMAKE_SOURCE_DIR}/liblfb/)
add_subdirectory(${LFB_SRC_DIR})

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
add_block_subdir(BLOCK_EWMA        "std_blocks/ewma"        ON)
add_block_subdir(BLOCK_HEXDUMP     "std_blocks/hexdump"     ON)
if (NOT CMAKE_C_COMPILER_TARGET MATCHES "aarch64|arm64")
  add_block_subdir(BLOCK_LFDS_CYCLIC "std_blocks/lfds_cyclic" OFF)
endif()
add_block_subdir(BLOCK_GPIO        "std_blocks/gpio"        ON)
add_block_subdir(BLOCK_GPS         "std_blocks/gps"         ON)
add_block_subdir(BLOCK_IIO         "std_blocks/iio"         ON)
add_block_subdir(BLOCK_LFRB	   "std_blocks/lfrb"	    ON)
add_block_subdir(BLOCK_LSDB_INTF   "std_blocks/lsdb-intf"   OFF)
add_block_subdir(BLOCK_LUABLOCK    "std_blocks/luablock"    ON)
add_block_subdir(BLOCK_MATH_DOUBLE "std_blocks/math_double" ON)
add_block_subdir(BLOCK_MOVAVG      "std_blocks/movavg"      ON)
add_block_subdir(BLOCK_MQUEUE      "std_blocks/mqueue"      ON)
add_block_subdir(BLOCK_MUX         "std_blocks/mux"         ON)
add_block_subdir(BLOCK_NETSINK     "std_blocks/netsink"     ON)
add_block_subdir(BLOCK_PID         "std_blocks/pid"         ON)
add_block_subdir(BLOCK_RAMP        "std_blocks/ramp"        ON)
add_block_subdir(BLOCK_RAND        "std_blocks/rand"        ON)
add_block_subdir(BLOCK_SATURATION  "std_blocks/saturation"  ON)
add_block_subdir(BLOCK_SKELLETON   "std_blocks/skelleton"   ON)
add_block_subdir(BLOCK_STATS       "std_blocks/stats"       ON)
add_block_subdir(BLOCK_THRESHOLD   "std_blocks/threshold"   ON)
add_block_subdir(BLOCK_TRIG        "std_blocks/trig"        ON)
add_block_subdir(BLOCK_WEBGRAPH    "std_blocks/webgraph"    ON)


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
pkg_check_modules(LIBDAEMON libdaemon)
if(LIBDAEMON_FOUND)
  target_compile_definitions(ubx-log PRIVATE HAVE_LIBDAEMON)
  target_compile_options(ubx-log PRIVATE ${LIBDAEMON_CFLAGS_OTHER})
  target_include_directories(ubx-log PRIVATE ${LIBDAEMON_INCLUDE_DIRS})
  target_link_libraries(ubx-log PRIVATE ${LIBDAEMON_LIBRARIES})
endif()
install(TARGETS ubx-log RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ubx-time-test tools/ubx-time-test.c)
target_link_libraries(ubx-time-test PRIVATE ubx)
install(TARGETS ubx-time-test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(test-rtlog tools/test-rtlog.c)
target_link_libraries(test-rtlog PRIVATE ubx rtlog_client rt pthread)
install(TARGETS test-rtlog RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

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

option(BUILD_LUA_DOCS "Build Lua API documentation with ldoc" OFF)

if(BUILD_LUA_DOCS)
  find_program(LDOC_EXECUTABLE ldoc)
  find_program(LUAJIT_EXECUTABLE luajit)
  if(NOT LDOC_EXECUTABLE)
    message(FATAL_ERROR "ldoc not found. Install lua-ldoc (apt install lua-ldoc lua-discount).")
  endif()
  if(NOT LUAJIT_EXECUTABLE)
    message(FATAL_ERROR "luajit not found. Required to run ldoc with discount markdown.")
  endif()
  set(LUA_DOC_DIR ${CMAKE_CURRENT_BINARY_DIR}/lua-docs)
  add_custom_target(doc-lua ALL
    COMMAND ${LUAJIT_EXECUTABLE} ${LDOC_EXECUTABLE} --config config.ld --dir ${LUA_DOC_DIR} .
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs/lua
    COMMENT "Building Lua API docs with ldoc into ${LUA_DOC_DIR}"
    VERBATIM
  )
endif()

#
# tests
#
install(DIRECTORY ${CMAKE_SOURCE_DIR}/tests
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ubx
  USE_SOURCE_PERMISSIONS)


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
		  --suppress=variableScope:libubx/md5.c .
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running cppcheck..."
  VERBATIM
)
