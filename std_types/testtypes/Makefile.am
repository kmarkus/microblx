
ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = testtypes.la

BUILT_SOURCES = types/test_trig_conf.h.hexarr \
		types/kdl_vector.h.hexarr \
		types/kdl_rotation.h.hexarr \
		types/kdl_frame.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr -s $< -d $<.hexarr

# testtypes : The basic types
testtypes_la_includes = $(top_srcdir)/libubx/ubx.h
testtypes_la_SOURCES = $(testtypes_la_includes) testtypes.c
testtypes_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
testtypes_la_LIBADD = $(top_builddir)/libubx/libubx.la
testtypes_la_CFLAGS = -I$(top_srcdir)/libubx

