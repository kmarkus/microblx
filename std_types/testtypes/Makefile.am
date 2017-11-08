
ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@
ubxmod_LTLIBRARIES = testtypes.la

BUILT_SOURCES = types/test_trig_conf.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

# testtypes : The basic types
testtypes_la_includes = $(top_srcdir)/libubx/ubx.h
testtypes_la_SOURCES = $(testtypes_la_includes) testtypes.c
testtypes_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
testtypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
testtypes_la_CFLAGS = -I$(top_srcdir)/libubx

