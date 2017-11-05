lib_LTLIBRARIES = libmqueue.la

#======================================
# mqueue : Message queue block
libmqueue_la_includes = $(top_srcdir)/libubx/ubx.h
libmqueue_la_SOURCES = $(libmqueue_la_includes) \
                    mqueue.c

libmqueue_la_LDFLAGS = $(top_builddir)/libubx/libubx.la -lrt
libmqueue_la_CFLAGS = -I$(top_srcdir)/libubx
