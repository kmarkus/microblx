# trig: simple trigger block

ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@

ubxmod_LTLIBRARIES = trig.la

BUILT_SOURCES = types/trig_config.h.hexarr \
                types/trig_period.h.hexarr \
                types/trig_tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr $<

trig_la_includes = $(top_srcdir)/libubx/ubx.h $(top_srcdir)/libubx/ubx_types.h
trig_la_SOURCES = $(trig_la_includes) trig.c
trig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
trig_la_LIBADD = $(top_builddir)/libubx/libubx.la
trig_la_CPPFLAGS = -I$(top_srcdir)/libubx
