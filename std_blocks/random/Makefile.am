lib_LTLIBRARIES = librandom.la

BUILT_SOURCES = types/random_config.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

#======================================
# random : Random number generator block
librandom_la_includes = $(top_srcdir)/libubx/ubx.h
librandom_la_SOURCES = $(librandom_la_includes) \
                    random.c
librandom_la_LIBADD = $(top_srcdir)/libubx/libubx.la
librandom_la_CPPFLAGS = -I$(top_srcdir)/libubx
librandom_la_LDFLAGS = -module -avoid-version

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<
