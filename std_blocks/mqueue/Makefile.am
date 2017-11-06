lib_LTLIBRARIES = mqueue.la

#======================================
# mqueue : Message queue block
mqueue_la_includes = $(top_srcdir)/libubx/ubx.h
mqueue_la_SOURCES = $(mqueue_la_includes) \
                    mqueue.c

mqueue_la_LIBADD = $(top_builddir)/libubx/libubx.la -lrt @UBX_MODULE_LDFLAGS@
mqueue_la_CFLAGS = -I$(top_srcdir)/libubx
