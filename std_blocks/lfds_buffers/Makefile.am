
ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = lfds_cyclic.la

lfds_cyclic_la_SOURCES = lfds_cyclic.c
lfds_cyclic_la_CFLAGS = $(LFDS_CFLAGS) -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden
lfds_cyclic_la_LDFLAGS = $(LFDS_LIBS) -module -avoid-version -shared -export-dynamic
lfds_cyclic_la_LIBADD = $(top_builddir)/libubx/libubx.la
