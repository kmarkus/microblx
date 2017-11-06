lib_LTLIBRARIES = logger.la

BUILT_SOURCES = file_logger.lua.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.lua.hexarr: %.lua
	$(top_srcdir)/tools/file2carr.lua $<

# logger : Logger block
logger_la_includes = $(top_srcdir)/libubx/ubx.h \
                     $(top_srcdir)/libubx/ubx_types.h
logger_la_SOURCES = $(logger_la_includes) \
                    file_logger.lua.hexarr \
                    file_logger.c

logger_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
logger_la_CPPFLAGS = -I$(top_srcdir)/libubx
