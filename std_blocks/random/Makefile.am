# random : Random number generator block

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = random.la

BUILT_SOURCES = types/random_config.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

random_la_SOURCES = random.c
random_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
random_la_LIBADD = $(top_builddir)/libubx/libubx.la

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx-tocarr -s $< -d $<.hexarr
