lib_LTLIBRARIES = libptrig.la

BUILT_SOURCES = types/ptrig_config.h.hexarr \
                types/ptrig_period.h.hexarr \
                types/ptrig_tstat.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

#======================================
# ptrig : Pthread based trigger block
libptrig_la_includes = $(top_srcdir)/libubx/ubx.h \
                    $(top_srcdir)/libubx/ubx_types.h
libptrig_la_SOURCES = $(libptrig_la_includes) \
                   ptrig.c
libptrig_la_LIBADD = $(top_srcdir)/libubx/libubx.la
libptrig_la_CPPFLAGS = -I$(top_srcdir)/libubx
libptrig_la_LDFLAGS = -module -avoid-version
