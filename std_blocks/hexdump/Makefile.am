lib_LTLIBRARIES = hexdump.la

#======================================
# hexdump : Hexdump block
hexdump_la_includes = $(top_srcdir)/libubx/ubx.h
hexdump_la_SOURCES = $(hexdump_la_includes) \
                     hexdump.c
hexdump_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
hexdump_la_CPPFLAGS = -I$(top_srcdir)/libubx
