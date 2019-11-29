# logger: logger block

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = logger.la

BUILT_SOURCES = file_logger.lua.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.lua.hexarr: %.lua
	$(top_srcdir)/tools/ubx_tocarr -s $< -d $<.hexarr

logger_la_includes = $(top_srcdir)/libubx/ubx.h \
                     $(top_srcdir)/libubx/ubx_types.h

logger_la_SOURCES = $(logger_la_includes) \
                    file_logger.lua.hexarr \
                    file_logger.c

logger_la_LDFLAGS = -module -avoid-version -shared -export-dynamic $(LUAJIT_LIBS)
logger_la_LIBADD = $(top_builddir)/libubx/libubx.la
logger_la_CFLAGS = -I$(top_srcdir)/libubx $(LUAJIT_CFLAGS) @UBX_CFLAGS@
