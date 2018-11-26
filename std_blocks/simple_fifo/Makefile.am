
AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = simple_fifo.la

simple_fifo_la_includes = $(top_srcdir)/libubx/ubx.h
simple_fifo_la_SOURCES = $(simple_fifo_la_includes) simple_fifo.c
simple_fifo_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
simple_fifo_la_LIBADD = $(top_builddir)/libubx/libubx.la
