# libubx: the microblx library

AM_CFLAGS = -I$(top_srcdir)/std_types/stattypes/ \
	    -I$(top_srcdir)/std_types/stdtypes/ \
	    $(UBX_CFLAGS)

lib_LTLIBRARIES = libubx.la \
		  librtlog_client.la

libubx_includes = ubx.h ubx_uthash_ffi.h \
		  ubx_types.h ubx_proto.h md5.h \
		  tstat_utils.h rtlog.h

internalincludedir = $(includedir)/ubx/internal
internalinclude_HEADERS = internal/rtlog_common.h \
			  internal/rtlog_client.h

pkginclude_HEADERS = ubx.h \
		  ubx_types.h \
		  ubx_types_cpp.h \
		  ubx_proto.h \
		  ubx_uthash_ffi.h \
		  tstat_utils.h \
		  rtlog.h \
		  rtlog_client.h

libubx_la_SOURCES = $(libubx_includes) \
		    md5.c ubx.c tstat_utils.c rtlog.c

libubx_la_LDFLAGS = -version-info $(UBX_LT_VER) -lrt -lpthread

librtlog_client_la_SOURCES = rtlog_client.c
librtlog_client_la_LDFLAGS = -shared -lrt

BUILT_SOURCES = ubx_proto.h

CLEANFILES = $(BUILT_SOURCES)

ubx_proto.h: md5.c ubx.c rtlog.c
	cproto $(srcdir)/md5.c $(srcdir)/ubx.c $(srcdir)/rtlog.c \
		-I$(top_builddir)/ -I${top_srcdir}/ -I${top_srcdir}/libubx/ \
		-I$(top_srcdir)/oe-workdir/recipe-sysroot/usr/include \
		-I$(top_srcdir)/../recipe-sysroot/usr/include > $(srcdir)/$@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ubx0.pc ubxrtl0.pc
