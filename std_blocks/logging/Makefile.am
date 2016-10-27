lib_LTLIBRARIES = liblogger.la

BUILT_SOURCES = file_logger.lua.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.lua.hexarr: %.lua
	$(top_srcdir)/tools/file2carr.lua $<

#======================================
# logger : Logger block
liblogger_la_includes = $(top_srcdir)/libubx/ubx.h \
                     $(top_srcdir)/libubx/ubx_types.h
liblogger_la_SOURCES = $(liblogger_la_includes) \
                    file_logger.lua.hexarr \
                    file_logger.c
liblogger_la_LIBADD = $(top_builddir)/libubx/libubx.la
liblogger_la_CPPFLAGS = -I$(top_srcdir)/libubx
