AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = hexdump.la

hexdump_la_SOURCES = hexdump.c
hexdump_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
hexdump_la_LIBADD = $(top_builddir)/libubx/libubx.la

