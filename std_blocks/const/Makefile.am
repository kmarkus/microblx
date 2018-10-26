# const_int block

ubxmoddir = $(ubx_moddir)
ubxmod_LTLIBRARIES = const_int.la

const_int_la_includes = $(top_srcdir)/libubx/ubx.h
const_int_la_SOURCES = $(const_int_la_includes) \
                     const_int.c
const_int_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
const_int_la_LIBADD = $(top_builddir)/libubx/libubx.la
const_int_la_CPPFLAGS = -I$(top_srcdir)/libubx
