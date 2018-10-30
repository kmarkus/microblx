ubxmoddir = $(ubx_moddir)

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmod_LTLIBRARIES = ramp_float.la ramp_double.la \
		     ramp_uint8.la ramp_uint16.la ramp_uint32.la ramp_uint64.la \
		     ramp_int8.la ramp_int16.la ramp_int32.la ramp_int64.la

ramp_float_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_float_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_float_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_float_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_float_la_CPPFLAGS = -DRAMP_T=float

ramp_double_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_double_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_double_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_double_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_double_la_CPPFLAGS = -DRAMP_T=double

ramp_uint8_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_uint8_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_uint8_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_uint8_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_uint8_la_CPPFLAGS = -DRAMP_T=uint8_t -DBLOCK_NAME=uint8

ramp_uint16_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_uint16_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_uint16_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_uint16_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_uint16_la_CPPFLAGS = -DRAMP_T=uint16_t -DBLOCK_NAME=uint16

ramp_uint32_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_uint32_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_uint32_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_uint32_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_uint32_la_CPPFLAGS = -DRAMP_T=uint32_t -DBLOCK_NAME=uint32

ramp_uint64_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_uint64_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_uint64_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_uint64_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_uint64_la_CPPFLAGS = -DRAMP_T=uint64_t -DBLOCK_NAME=uint64

ramp_int8_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_int8_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_int8_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_int8_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_int8_la_CPPFLAGS = -DRAMP_T=int8_t -DBLOCK_NAME=int8

ramp_int16_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_int16_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_int16_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_int16_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_int16_la_CPPFLAGS = -DRAMP_T=int16_t -DBLOCK_NAME=int16

ramp_int32_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_int32_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_int32_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_int32_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_int32_la_CPPFLAGS = -DRAMP_T=int32_t -DBLOCK_NAME=int32

ramp_int64_la_includes = $(top_srcdir)/libubx/ubx.h
ramp_int64_la_SOURCES = $(ramp_la_includes) ramp.c
ramp_int64_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
ramp_int64_la_LIBADD = $(top_builddir)/libubx/libubx.la
ramp_int64_la_CPPFLAGS = -DRAMP_T=int64_t -DBLOCK_NAME=int64


%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx_tocarr $<
