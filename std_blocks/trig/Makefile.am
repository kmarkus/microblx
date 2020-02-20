# trig: simple trigger block

AM_CFLAGS = -I$(top_srcdir)/libubx \
	    -I$(top_srcdir)/std_types/stdtypes/types/ $(UBX_CFLAGS)

ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = trig.la ptrig.la

pkginclude_HEADERS = $(top_srcdir)/libubx/ubx.h $(top_srcdir)/libubx/ubx_types.h

BUILT_SOURCES = types/ptrig_period.h.hexarr \
                $(top_srcdir)/std_types/stdtypes/types/tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr -s $< -d $<.hexarr

trig_la_SOURCES = $(trig_la_includes) trig.c trig_common.c
trig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
trig_la_LIBADD = $(top_builddir)/libubx/libubx.la

# ptrig
ptrig_la_includes = types/ptrig_period.h \
		    types/ptrig_period.h.hexarr \
		    $(top_srcdir)/std_types/stdtypes/types/trig_spec.h

ptrig_la_SOURCES = $(ptrig_la_includes) ptrig.c trig_common.c
ptrig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ptrig_la_LIBADD = $(top_builddir)/libubx/libubx.la
