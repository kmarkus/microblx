# random : Random number generator block

lib_LTLIBRARIES = random.la

BUILT_SOURCES = types/random_config.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

random_la_includes = $(top_srcdir)/libubx/ubx.h
random_la_SOURCES = $(random_la_includes) random.c
random_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
random_la_LIBADD = $(top_builddir)/libubx/libubx.la
random_la_CPPFLAGS = -I$(top_srcdir)/libubx

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<
