# const_int block

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = const_int.la

const_int_la_includes = $(top_srcdir)/libubx/ubx.h
const_int_la_SOURCES = $(const_int_la_includes) \
                     const_int.c
const_int_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
const_int_la_LIBADD = $(top_builddir)/libubx/libubx.la
