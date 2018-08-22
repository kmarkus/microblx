ubxmoddir = $(ubx_moddir)
ubxmod_LTLIBRARIES = ramp.la

ramp_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_la_CPPFLAGS = -I$(top_srcdir)/libubx

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr $<
