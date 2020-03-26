# stdtypes : The standard types

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = stdtypes.la

BUILT_SOURCES = types/tstat.h.hexarr \
		types/trig_spec.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

pkginclude_HEADERS = types/tstat.h types/tstat.h.hexarr \
		     types/trig_spec.h types/trig_spec.h.hexarr

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx-tocarr -s $< -d $<.hexarr

stdtypes_la_includes = $(top_srcdir)/libubx/ubx.h
stdtypes_la_SOURCES = $(stdtypes_la_includes) stdtypes.c
stdtypes_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
stdtypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
stdtypes_la_CFLAGS = -I$(top_srcdir)/libubx

