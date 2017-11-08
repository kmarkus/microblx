
ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@
ubxmod_LTLIBRARIES = lfds_cyclic.la

# lfds_cyclic : A lock free interaction block
lfds_cyclic_la_includes = $(top_srcdir)/libubx/ubx.h
lfds_cyclic_la_SOURCES = $(lfds_cyclic_la_includes) lfds_cyclic.c
lfds_cyclic_la_CFLAGS = $(LFDS_CFLAGS) -I$(top_srcdir)/libubx
lfds_cyclic_la_LDFLAGS = $(LFDS_LIBS) -module -avoid-version -shared -export-dynamic
lfds_cyclic_la_LIBADD = $(top_builddir)/libubx/libubx.la
