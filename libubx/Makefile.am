lib_LTLIBRARIES = libubx.la

#=======================================
# libubx : The microblx library
libubx_includes = ubx.h uthash.h uthash_ffi.h \
                  ubx_types.h ubx_proto.h md5.h

include_HEADERS = ubx.h \
                  uthash.h \
                  ubx_types.h \
                  ubx_types_cpp.h \
                  ubx_proto.h \
                  uthash_ffi.h \
                  md5.h

libubx_la_SOURCES = $(libubx_includes) \
                    md5.c ubx.c

ubx_proto.h: md5.c ubx.c
	cproto md5.c ubx.c -I${top_srcdir}/ -I${top_srcdir}/libubx/ > $@
