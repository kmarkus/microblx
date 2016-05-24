pkglib_LTLIBRARIES = libubx.la

#=======================================
# libubx : The microblx library
libubx_includes = ubx.h uthash.h uthash_ffi.h \
                  ubx_types.h ubx_proto.h
libubx_la_SOURCES = $(libubx_includes) \
                    ubx.c
libubx_la_LDFLAGS = -module -avoid-version
ubx_proto.h:
	cproto ubx.c > $@

ubxdir = $(datadir)/microblx
ubxincludedir = $(ubxdir)/include
ubxinclude_DATA = uthash_ffi.h \
                  ubx_types.h \
                  ubx_proto.h
