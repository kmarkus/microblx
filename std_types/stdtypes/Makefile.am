# stdtypes : The standard types

ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@
ubxmod_LTLIBRARIES = stdtypes.la

stdtypes_la_includes = $(top_srcdir)/libubx/ubx.h
stdtypes_la_SOURCES = $(stdtypes_la_includes) stdtypes.c
stdtypes_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
stdtypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
stdtypes_la_CFLAGS = -I$(top_srcdir)/libubx

