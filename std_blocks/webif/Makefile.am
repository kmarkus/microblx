lib_LTLIBRARIES = libwebif.la

BUILT_SOURCES = webif.lua.hexarr
CLEANFILES = $(BUILT_SOURCES)

#======================================
# webif : A system monitoring web-interface function block.
libwebif_la_includes = $(top_srcdir)/libubx/ubx.h
libwebif_la_SOURCES = $(libwebif_la_includes) \
                   webif.c \
                   mongoose.c
libwebif_la_LIBADD = $(top_srcdir)/libubx/libubx.la
libwebif_la_CPPFLAGS = -I$(top_srcdir)/libubx
libwebif_la_LDFLAGS = -module -avoid-version

%.lua.hexarr: %.lua
	$(top_srcdir)/tools/file2carr.lua $<
