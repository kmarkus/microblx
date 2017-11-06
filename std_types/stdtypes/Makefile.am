lib_LTLIBRARIES = stdtypes.la

# stdtypes : The standard types
stdtypes_la_includes = $(top_srcdir)/libubx/ubx.h
stdtypes_la_SOURCES = $(stdtypes_la_includes) stdtypes.c

stdtypes_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
stdtypes_la_CFLAGS = -I$(top_srcdir)/libubx

