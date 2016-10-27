lib_LTLIBRARIES = libtesttypes.la

BUILT_SOURCES = types/test_trig_conf.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

#======================================
# testtypes : The basic types
libtesttypes_la_includes = $(top_srcdir)/libubx/ubx.h
libtesttypes_la_SOURCES = $(libtesttypes_la_includes) \
                       testtypes.c
libtesttypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
libtesttypes_la_CPPFLAGS = -I$(top_srcdir)/libubx
#libtesttypes_la_LDFLAGS = -module -avoid-version
