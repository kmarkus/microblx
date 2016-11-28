lib_LTLIBRARIES = libhexdump.la

#======================================
# hexdump : Hexdump block
libhexdump_la_includes = $(top_srcdir)/libubx/ubx.h
libhexdump_la_SOURCES = $(libhexdump_la_includes) \
                     hexdump.c
libhexdump_la_LIBADD = $(top_builddir)/libubx/libubx.la
libhexdump_la_CPPFLAGS = -I$(top_srcdir)/libubx
