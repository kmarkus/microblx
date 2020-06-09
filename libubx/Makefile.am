# libubx: the microblx library

AM_CFLAGS = -I$(top_srcdir)/std_types/stdtypes/types/ \
	    $(UBX_CFLAGS)

lib_LTLIBRARIES = libubx.la \
		  librtlog_client.la

libubx_includes = ubx.h ubx_uthash_ffi.h typemacros.h \
		  ubx_types.h ubx_proto.h ubx_utils.h \
		  md5.h trig_utils.h rtlog.h

internalincludedir = $(includedir)/ubx/internal
internalinclude_HEADERS = internal/rtlog_common.h \
			  internal/rtlog_client.h

pkginclude_HEADERS = ubx.h \
		  ubx_types.h \
		  typemacros.h \
		  ubx_proto.h \
		  ubx_uthash_ffi.h \
		  trig_utils.h \
		  rtlog.h \
		  rtlog_client.h

libubx_la_SOURCES = $(libubx_includes) \
		    md5.c ubx.c ubx_time.c ubx_utils.c trig_utils.c rtlog.c accessors.c

libubx_la_LDFLAGS = -lrt -lpthread -ldl

librtlog_client_la_SOURCES = rtlog_client.c
librtlog_client_la_LDFLAGS = -shared -lrt

BUILT_SOURCES = ubx_proto.h

CLEANFILES = $(BUILT_SOURCES)

ubx_proto.h: md5.c ubx.c ubx_time.c rtlog.c
	cproto $(srcdir)/md5.c $(srcdir)/ubx.c \
		$(srcdir)/ubx_utils.c $(srcdir)/accessors.c \
		$(srcdir)/rtlog.c $(srcdir)/ubx_time.c \
		-I$(top_builddir)/ -I${top_srcdir}/ -I${top_srcdir}/libubx/ \
		-I$(top_srcdir)/oe-workdir/recipe-sysroot/usr/include \
		-I$(top_srcdir)/../recipe-sysroot/usr/include > $(srcdir)/$@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ubx0.pc ubxrtl0.pc
