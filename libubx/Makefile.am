# libubx: the microblx library

AM_CFLAGS = $(UBX_CFLAGS)

lib_LTLIBRARIES = libubx.la

libubx_includes = ubx.h ubx_uthash_ffi.h \
                  ubx_types.h ubx_proto.h md5.h

include_HEADERS = ubx.h \
                  ubx_types.h \
                  ubx_types_cpp.h \
                  ubx_proto.h \
                  ubx_uthash_ffi.h \
                  md5.h

libubx_la_SOURCES = $(libubx_includes) \
                    md5.c ubx.c

libubx_la_LDFLAGS = -version-info $(UBX_LT_VER)

BUILT_SOURCES = ubx_proto.h

CLEANFILES = $(BUILT_SOURCES)

ubx_proto.h: md5.c ubx.c
	cproto $(srcdir)/md5.c $(srcdir)/ubx.c -I$(top_builddir)/ -I${top_srcdir}/ -I${top_srcdir}/libubx/ > $(srcdir)/$@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ubx0.pc
