pkglib_LTLIBRARIES = ptrig.la

BUILT_SOURCES = types/ptrig_config.h.hexarr \
                types/ptrig_period.h.hexarr \
                types/ptrig_tstat.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

#======================================
# ptrig : Pthread based trigger block
ptrig_la_includes = $(top_srcdir)/libubx/ubx.h \
                    $(top_srcdir)/libubx/ubx_types.h
ptrig_la_SOURCES = $(ptrig_la_includes) \
                   ptrig.c
ptrig_la_LIBADD = $(top_srcdir)/libubx/libubx.la
ptrig_la_CPPFLAGS = -I$(top_srcdir)/libubx
ptrig_la_LDFLAGS = -module -avoid-version
