lib_LTLIBRARIES = libmqueue.la

#======================================
# mqueue : Message queue block
libmqueue_la_includes = $(top_srcdir)/libubx/ubx.h
libmqueue_la_SOURCES = $(libmqueue_la_includes) \
                    mqueue.c
libmqueue_la_LIBADD = $(top_builddir)/libubx/libubx.la
libmqueue_la_CPPFLAGS = -I$(top_srcdir)/libubx
