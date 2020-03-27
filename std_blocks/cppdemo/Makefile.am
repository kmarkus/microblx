# cppdemo : dummy block to illustrate C++ blocks

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = cppdemo.la

BUILT_SOURCES = types/cpp_demo_type.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

cppdemo_la_SOURCES = cppdemo.cpp
cppdemo_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
cppdemo_la_LIBADD = $(top_builddir)/libubx/libubx.la
cppdemo_la_CPPFLAGS = -I$(top_srcdir)/libubx

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx-tocarr -s $< -d $<.hexarr
