m4_define([package_version_major],[0])
m4_define([package_version_minor],[9])
m4_define([package_version_micro],[0])

AC_INIT([ubx], [package_version_major.package_version_minor.package_version_micro])
AM_INIT_AUTOMAKE([foreign -Wall])

# compilers
AC_PROG_CC
AC_PROG_CXX

PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Check if the `install` program is present
AC_PROG_INSTALL

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT(disable-static)

AC_SUBST([UBX_VER], [package_version_major.package_version_minor])
AC_SUBST([UBX_MODDIR], [$libdir/$PACKAGE/$UBX_VER])
AC_SUBST([UBX_CFLAGS], ["-Wall -Wextra -Werror"])

AC_ARG_ENABLE([timesrc-tsc],
  AS_HELP_STRING([--enable-timesrc-tsc], [use TSC as timesource instead of POSIX time]),
  [enable_timesrc_tsc=yes],
  [])

if test "x${enable_timesrc_tsc}" = xyes; then
  AC_DEFINE([TIMESRC_TSC], 1, [use TSC as timesource instead of POSIX time])
fi

# C++ demo block is optional
AC_ARG_ENABLE([cpp-demo],
  AS_HELP_STRING([--enable-cpp-demo],[build example C++ block]))
AM_CONDITIONAL([CPP_DEMO], [test x$enable_cpp_demo = xyes])

# required for TSC only
AC_DEFINE([CPU_HZ], 1600000000, [cpu hz defined])

PKG_CHECK_MODULES(LUAJIT, luajit >= 2.0.4)
PKG_CHECK_MODULES(LFDS, lfds6 >= 6.1.1)

# pthreads
AC_SEARCH_LIBS([pthread_create], [pthread], [have_pthreads=yes])
AS_IF([test "x$have_pthreads" != "xyes"],
    AC_MSG_ERROR(Unable to find pthreads))

# uthash
AC_CHECK_HEADERS([uthash.h], [uthash_found=yes; break;])
AS_IF([test "x$uthash_found" != "xyes"],
    AC_MSG_ERROR([Unable to find uthash headers (install uthash-dev)]))

# sphinx
AC_CHECK_PROGS([SPHINXBUILD], [sphinx-build sphinx-build3 sphinx-build2], [no])
AS_IF([test x"$SPHINXBUILD" = x"no"],
    AC_MSG_WARN(sphinx-build is required to build documentation))

# files to generate via autotools
AC_CONFIG_FILES([Makefile
libubx/Makefile
libubx/ubx0.pc
libubx/ubxrtl0.pc
std_blocks/Makefile
std_blocks/const/Makefile
std_blocks/cppdemo/Makefile
std_blocks/hexdump/Makefile
std_blocks/lfds_buffers/Makefile
std_blocks/logging/Makefile
std_blocks/luablock/Makefile
std_blocks/math/Makefile
std_blocks/mqueue/Makefile
std_blocks/examples/Makefile
std_blocks/pid/Makefile
std_blocks/ramp/Makefile
std_blocks/rand/Makefile
std_blocks/saturation/Makefile
std_blocks/trig/Makefile
std_blocks/webif/Makefile
std_types/Makefile
std_types/stdtypes/Makefile
std_types/testtypes/Makefile
examples/Makefile
lua/Makefile
tools/Makefile
])

# generate
AC_OUTPUT
