# trig: simple trigger block

AM_CFLAGS = -I$(top_srcdir)/libubx \
	-I$(top_srcdir)/std_types/stdtypes/ $(UBX_CFLAGS)
ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = trig.la ptrig.la

pkginclude_HEADERS = $(top_srcdir)/libubx/ubx.h $(top_srcdir)/libubx/ubx_types.h

pkginclude_HEADERS =

BUILT_SOURCES = types/trig_config.h.hexarr \
                types/trig_period.h.hexarr \
		types/ptrig_config.h.hexarr \
                types/ptrig_period.h.hexarr \
                $(top_srcdir)/std_types/stdtypes/types/tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr -s $< -d $<.hexarr

# trig
trig_la_includes = types/trig_config.h \
		   types/trig_config.h.hexarr \
		   types/trig_period.h \
		   types/trig_period.h.hexarr

trig_la_SOURCES = $(trig_la_includes) trig.c
trig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
trig_la_LIBADD = $(top_builddir)/libubx/libubx.la

# ptrig
ptrig_la_includes = types/ptrig_config.h \
		    types/ptrig_config.h.hexarr \
		    types/ptrig_period.h \
		    types/ptrig_period.h.hexarr

ptrig_la_SOURCES = $(ptrig_la_includes) ptrig.c
ptrig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ptrig_la_LIBADD = $(top_builddir)/libubx/libubx.la
