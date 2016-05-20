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

# files to generate via autotools
AC_CONFIG_FILES([Makefile])

# generate
AC_OUTPUT
