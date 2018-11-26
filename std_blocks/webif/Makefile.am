
ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = webif.la

BUILT_SOURCES = webif.lua.hexarr
CLEANFILES = $(BUILT_SOURCES)

webif_la_includes = $(top_srcdir)/libubx/ubx.h

webif_la_SOURCES = $(webif_la_includes) \
                   webif.c \
                   mongoose.c

webif_la_LDFLAGS = -module -avoid-version -shared -export-dynamic $(LUAJIT_LIBS)
webif_la_LIBADD = $(top_builddir)/libubx/libubx.la
webif_la_CFLAGS = -I$(top_srcdir)/libubx $(LUAJIT_CFLAGS) @UBX_CFLAGS@

%.lua.hexarr: %.lua
	$(top_srcdir)/tools/ubx_tocarr $<
