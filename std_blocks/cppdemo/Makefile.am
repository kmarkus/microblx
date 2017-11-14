# cppdemo : dummy block to illustrate C++ blocks

ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@
ubxmod_LTLIBRARIES = cppdemo.la

BUILT_SOURCES = types/cpp_demo_type.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

cppdemo_la_includes = $(top_srcdir)/libubx/ubx.h
cppdemo_la_SOURCES = $(cppdemo_la_includes) cppdemo.cpp
cppdemo_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
cppdemo_la_LIBADD = $(top_builddir)/libubx/libubx.la
cppdemo_la_CPPFLAGS = -I$(top_srcdir)/libubx

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr $<
