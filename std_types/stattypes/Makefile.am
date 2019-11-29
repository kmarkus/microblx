
ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = stattypes.la

BUILT_SOURCES = types/tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

pkginclude_HEADERS = types/tstat.h \
		    types/tstat.h.hexarr

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr -s $< -d $<.hexarr

# stattypes : The basic types
stattypes_la_includes = $(top_srcdir)/libubx/ubx.h
stattypes_la_SOURCES = $(stattypes_la_includes) stattypes.c
stattypes_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
stattypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
stattypes_la_CFLAGS = -I$(top_srcdir)/libubx

