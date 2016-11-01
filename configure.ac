AC_INIT([microblx],[1.0])
AC_CONFIG_SRCDIR([libubx/ubx.c])

# directories (relative to top-level) to look into for AutoConf/AutoMake files
#AC_CONFIG_AUX_DIR([build-aux])
#AC_CONFIG_MACRO_DIR([build-aux])

# initialisation
#AM_INIT_AUTOMAKE([1.10])
AM_INIT_AUTOMAKE([foreign -Wall])

# all defined C macros (HAVE_*) will be saved to this file
#AC_CONFIG_HEADERS([config.h])

# compilers
AC_PROG_CC
AC_PROG_CXX([clang++])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Check if the `install` program is present
AC_PROG_INSTALL

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT

AC_ARG_ENABLE([ptrig-tstat-block],
  AS_HELP_STRING([--disable-ptrig-tstat-block], [Disable ptrig block-level timing statistics]),
  [ptrig_tstat_block=${enableval}],
  [ptrig_tstat_block=yes])

AC_ARG_ENABLE([ptrig-tstat],
  AS_HELP_STRING([--disable-ptrig-tstat], [Disable ptrig timing statistics]),
  [ptrig_tstat=${enableval}],
  [ptrig_tstat=yes])

if test "x${ptrig_tstat}" = xno; then
  ptrig_tstat_block=no
fi

if test "x${ptrig_tstat_block}" = xyes; then
  AC_DEFINE([PTRIG_TSTAT_BLOCK], 1, [ptrig tstat block enabled])
fi

if test "x${ptrig_tstat}" = xyes; then
   AC_DEFINE([PTRIG_TSTAT], 1, [ptrig tstat enabled])
fi

have_pthreads=no
AC_SEARCH_LIBS([pthread_create], [pthread], [have_pthreads=yes])
if test "x${have_pthreads}" = xyes; then
  AC_CHECK_HEADERS([pthread.h], [], [have_pthreads=no])
fi
if test "x${have_pthreads}" = xno; then
  echo "---------------------------------------"
  echo "Unable to find pthreads on this system."
  echo "---------------------------------------"
  exit 1
fi

AC_SEARCH_LIBS([lua_pcall], [luajit luajit-5.1], [], [
  AC_MSG_ERROR([unable to find lua_pcall() function])
])

have_lfds=no
AC_SEARCH_LIBS([lfds611_ringbuffer_new], [lfds], [have_lfds=yes])
if test "x${have_lfds}" = xyes; then
  AC_CHECK_HEADERS([liblfds611.h], [], [have_lfds=no])
fi
if test "x${have_lfds}" = xno; then
  echo "--------------------------------------"
  echo "Unable to find liblfds on this system."
  echo "--------------------------------------"
  exit 1
fi

# files to generate via autotools
AC_CONFIG_FILES([Makefile
libubx/Makefile
std_blocks/Makefile
std_blocks/cppdemo/Makefile
std_blocks/hexdump/Makefile
std_blocks/lfds_buffers/Makefile
std_blocks/logging/Makefile
std_blocks/luablock/Makefile
std_blocks/mqueue/Makefile
std_blocks/ptrig/Makefile
std_blocks/trig/Makefile
std_blocks/random/Makefile
std_blocks/simple_fifo/Makefile
std_blocks/webif/Makefile
std_types/Makefile
std_types/kdl/Makefile
std_types/stdtypes/Makefile
std_types/testtypes/Makefile
examples/Makefile
lua/Makefile
tools/Makefile
])

# generate
AC_OUTPUT
