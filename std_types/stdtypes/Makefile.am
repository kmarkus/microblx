lib_LTLIBRARIES = libstdtypes.la

#======================================
# stdtypes : The standard types
libstdtypes_la_includes = $(top_srcdir)/libubx/ubx.h
libstdtypes_la_SOURCES = $(libstdtypes_la_includes) \
                      stdtypes.c
libstdtypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
libstdtypes_la_CPPFLAGS = -I$(top_srcdir)/libubx
#libstdtypes_la_LDFLAGS = -module -avoid-version
