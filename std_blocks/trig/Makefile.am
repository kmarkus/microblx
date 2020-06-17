# trig: simple trigger block

AM_CFLAGS = -I$(top_srcdir)/libubx \
	    -I$(top_srcdir)/std_types/stdtypes/types/ \
	     $(UBX_CFLAGS) -fvisibility=hidden

AM_LDFLAGS = -module -avoid-version -shared -export-dynamic

ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = trig.la ptrig.la

BUILT_SOURCES = types/ptrig_period.h.hexarr \
                $(top_srcdir)/std_types/stdtypes/types/tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

trig_la_SOURCES = trig.c common.c
trig_la_LIBADD = $(top_builddir)/libubx/libubx.la

ptrig_la_SOURCES = ptrig.c common.c
ptrig_la_LIBADD = $(top_builddir)/libubx/libubx.la

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx-tocarr -s $< -d $<.hexarr
