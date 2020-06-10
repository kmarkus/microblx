# libubx: the microblx library

AM_CFLAGS = -I$(top_srcdir)/std_types/stdtypes/types/ \
	    $(UBX_CFLAGS)

lib_LTLIBRARIES = libubx.la \
		  librtlog_client.la

libubx_includes = ubx.h \
		ubx_types.h \
		typemacros.h \
		accessors.h \
		ubx_core.h \
		ubx_time.h \
		ubx_uthash_ffi.h \
		trig_utils.h \
		md5.h \
		ubx_utils.h \
		rtlog.h

internalincludedir = $(includedir)/ubx/internal
internalinclude_HEADERS = internal/rtlog_common.h \
			  internal/rtlog_client.h

pkginclude_HEADERS = $(libubx_includes) rtlog_client.h

libubx_la_SOURCES = $(libubx_includes) \
		    md5.c ubx.c ubx_time.c ubx_utils.c trig_utils.c rtlog.c accessors.c

libubx_la_LDFLAGS = -lrt -lpthread -ldl

librtlog_client_la_SOURCES = rtlog_client.c
librtlog_client_la_LDFLAGS = -shared -lrt

CLEANFILES = $(BUILT_SOURCES)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ubx0.pc ubxrtl0.pc
