lib_LTLIBRARIES = libubx.la

#=======================================
# libubx : The microblx library
libubx_includes = ubx.h uthash.h uthash_ffi.h \
                  ubx_types.h ubx_proto.h
include_HEADERS = ubx.h \
                  uthash.h \
                  ubx_types.h \
                  ubx_proto.h \
                  uthash_ffi.h
libubx_la_SOURCES = $(libubx_includes) \
                    ubx.c

ubx_proto.h:
	cproto ubx.c > $@
