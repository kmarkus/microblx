pkglib_LTLIBRARIES = mqueue.la

#======================================
# mqueue : Message queue block
mqueue_la_includes = $(top_srcdir)/libubx/ubx.h
mqueue_la_SOURCES = $(mqueue_la_includes) \
                    mqueue.c
mqueue_la_LIBADD = $(top_srcdir)/libubx/libubx.la
mqueue_la_CPPFLAGS = -I$(top_srcdir)/libubx
mqueue_la_LDFLAGS = -module -avoid-version
