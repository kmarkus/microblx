# hexdump : hexdump block

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = hexdump.la

hexdump_la_includes = $(top_srcdir)/libubx/ubx.h
hexdump_la_SOURCES = $(hexdump_la_includes) \
                     hexdump.c
hexdump_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
hexdump_la_LIBADD = $(top_builddir)/libubx/libubx.la

