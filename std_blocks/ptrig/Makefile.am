# ptrig: pthread based trigger block

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = ptrig.la

BUILT_SOURCES = types/ptrig_config.h.hexarr \
                types/ptrig_period.h.hexarr \
                ../../std_types/stattypes/types/ubx_tstat.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr $<


ptrig_la_includes = $(top_srcdir)/libubx/ubx.h \
                    $(top_srcdir)/libubx/ubx_types.h

ptrig_la_SOURCES = $(ptrig_la_includes) ptrig.c
ptrig_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ptrig_la_LIBADD = $(top_builddir)/libubx/libubx.la
