# mqueue: message queue iblock

lib_LTLIBRARIES = mqueue.la

mqueue_la_includes = $(top_srcdir)/libubx/ubx.h
mqueue_la_SOURCES = $(mqueue_la_includes) \
                    mqueue.c

mqueue_la_LDFLAGS = -module -avoid-version -shared -export-dynamic -lrt
mqueue_la_LIBADD = $(top_builddir)/libubx/libubx.la
mqueue_la_CFLAGS = -I$(top_srcdir)/libubx
