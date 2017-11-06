lib_LTLIBRARIES = webif.la

BUILT_SOURCES = webif.lua.hexarr
CLEANFILES = $(BUILT_SOURCES)

#======================================
# webif : A system monitoring web-interface function block.
webif_la_includes = $(top_srcdir)/libubx/ubx.h
webif_la_SOURCES = $(webif_la_includes) \
                   webif.c \
                   mongoose.c

webif_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
webif_la_CPPFLAGS = -I$(top_srcdir)/libubx

%.lua.hexarr: %.lua
	$(top_srcdir)/tools/file2carr.lua $<
