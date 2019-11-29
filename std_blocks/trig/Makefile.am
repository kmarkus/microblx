# trig: simple trigger block

AM_CFLAGS = -I$(top_srcdir)/libubx \
	-I$(top_srcdir)/std_types/stattypes/ $(UBX_CFLAGS)
ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = trig.la

pkginclude_HEADERS = types/trig_config.h  \
		     types/trig_config.h.hexarr \
 		     types/trig_period.h \
		     types/trig_period.h.hexarr

BUILT_SOURCES = types/trig_config.h.hexarr \
                types/trig_period.h.hexarr \
                $(top_srcdir)/std_types/stattypes/types/tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr -s $< -d $<.hexarr -a $(basename $(notdir $<))$(subst .,_,$(suffix $<))

trig_la_includes = $(top_srcdir)/libubx/ubx.h $(top_srcdir)/libubx/ubx_types.h
trig_la_SOURCES = $(trig_la_includes) trig.c
trig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
trig_la_LIBADD = $(top_builddir)/libubx/libubx.la
