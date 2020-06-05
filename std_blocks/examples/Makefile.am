AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden
AM_LDFLAGS = -module -avoid-version -shared -export-dynamic

ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = threshold.la skelleton.la random.la

BUILT_SOURCES = types/thres_event.h.hexarr \
		types/foo_type.h.hexarr \
		types/random_config.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

threshold_la_SOURCES = threshold.c
threshold_la_LIBADD = $(top_builddir)/libubx/libubx.la

skelleton_la_SOURCES = skelleton.c
skelleton_la_LIBADD = $(top_builddir)/libubx/libubx.la

random_la_SOURCES = random.c
random_la_LIBADD = $(top_builddir)/libubx/libubx.la

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx-tocarr -s $< -d $<.hexarr
