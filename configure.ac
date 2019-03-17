m4_define([package_version_major],[0])
m4_define([package_version_minor],[6])
m4_define([package_version_micro],[2])

m4_define([libubx_if_current], [7])  dnl inc if IF added, removed or changed
m4_define([libubx_if_revision], [2]) dnl always inc if src changed, zero if current is inc'ed
m4_define([libubx_if_age], [1])      dnl inc if API changes are backwards compatible, zero otherwise

AC_INIT([microblx], [package_version_major.package_version_minor.package_version_micro])
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
AC_SUBST([UBX_LT_VER], [libubx_if_current:libubx_if_revision:libubx_if_age])
AC_SUBST([UBX_MODDIR], [$libdir/$PACKAGE/$UBX_VER])
AC_SUBST([UBX_CFLAGS], ["-Wall -Wextra -Werror"])

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

AC_ARG_ENABLE([tsc-timers],
  AS_HELP_STRING([--enable-tsc-timers], [Enable TSC timers (POSIX timers disabled)]),
  [enable_tsc_timers=yes],
  [])

if test "x${enable_tsc_timers}" = xyes; then
  AC_DEFINE([TSC_TIMERS], 1, [tsc timers enabled])
fi

AC_DEFINE([CPU_HZ], 1600000000, [cpu hz defined])

# check for pthreads
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

PKG_CHECK_MODULES(LUAJIT, luajit >= 2.0.4)
PKG_CHECK_MODULES(LFDS, lfds6 >= 6.1.1)

# files to generate via autotools
AC_CONFIG_FILES([Makefile
libubx/Makefile
libubx/ubx0.pc
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
std_blocks/const/Makefile
std_blocks/ramp/Makefile
std_blocks/simple_fifo/Makefile
std_blocks/webif/Makefile
std_types/Makefile
std_types/stdtypes/Makefile
std_types/stattypes/Makefile
std_types/testtypes/Makefile
examples/Makefile
lua/Makefile
tools/Makefile
])

# generate
AC_OUTPUT
