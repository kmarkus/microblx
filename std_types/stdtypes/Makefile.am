pkglib_LTLIBRARIES = stdtypes.la

#======================================
# stdtypes : The standard types
stdtypes_la_includes = $(top_srcdir)/libubx/ubx.h
stdtypes_la_SOURCES = $(stdtypes_la_includes) \
                      stdtypes.c
stdtypes_la_LIBADD = $(top_srcdir)/libubx/libubx.la
stdtypes_la_CPPFLAGS = -I$(top_srcdir)/libubx
stdtypes_la_LDFLAGS = -module -avoid-version
