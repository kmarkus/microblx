lib_LTLIBRARIES = libsimple_fifo.la

#======================================
# simple_fifo : Fifoed interaction block
libsimple_fifo_la_includes = $(top_srcdir)/libubx/ubx.h
libsimple_fifo_la_SOURCES = $(libsimple_fifo_la_includes) \
                         simple_fifo.c
libsimple_fifo_la_LIBADD = $(top_builddir)/libubx/libubx.la
libsimple_fifo_la_CPPFLAGS = -I$(top_srcdir)/libubx
