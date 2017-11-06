lib_LTLIBRARIES = trig.la

BUILT_SOURCES = types/trig_config.h.hexarr \
                types/trig_period.h.hexarr \
                types/trig_tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

# trig : Pthread based trigger block
trig_la_includes = $(top_srcdir)/libubx/ubx.h $(top_srcdir)/libubx/ubx_types.h
trig_la_SOURCES = $(trig_la_includes) trig.c

trig_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
trig_la_CPPFLAGS = -I$(top_srcdir)/libubx
