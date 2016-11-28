lib_LTLIBRARIES = libtrig.la

BUILT_SOURCES = types/trig_config.h.hexarr \
                types/trig_period.h.hexarr \
                types/trig_tstat.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

#======================================
# trig : Pthread based trigger block
libtrig_la_includes = $(top_srcdir)/libubx/ubx.h \
                    $(top_srcdir)/libubx/ubx_types.h
libtrig_la_SOURCES = $(libtrig_la_includes) \
                   trig.c
libtrig_la_LIBADD = $(top_builddir)/libubx/libubx.la
libtrig_la_CPPFLAGS = -I$(top_srcdir)/libubx
