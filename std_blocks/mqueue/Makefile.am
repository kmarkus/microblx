# mqueue: message queue iblock

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden

ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = mqueue.la
mqueue_la_SOURCES = mqueue.c
mqueue_la_LDFLAGS = -module -avoid-version -shared -export-dynamic -lrt
mqueue_la_LIBADD = $(top_builddir)/libubx/libubx.la
