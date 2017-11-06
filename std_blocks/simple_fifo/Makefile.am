lib_LTLIBRARIES = simple_fifo.la

#======================================
# simple_fifo : Fifoed interaction block
simple_fifo_la_includes = $(top_srcdir)/libubx/ubx.h
simple_fifo_la_SOURCES = $(simple_fifo_la_includes) simple_fifo.c

simple_fifo_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
simple_fifo_la_CFLAGS = -I$(top_srcdir)/libubx
