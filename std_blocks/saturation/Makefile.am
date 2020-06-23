ubxmoddir = $(UBX_MODDIR)

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden
AM_LDFLAGS = -module -avoid-version -shared -export-dynamic

ubxmod_LTLIBRARIES = saturation_float.la saturation_double.la saturation_int32.la saturation_int64.la

saturation_float_la_SOURCES = saturation.c
saturation_float_la_LIBADD = $(top_builddir)/libubx/libubx.la
saturation_float_la_CFLAGS = $(AM_CFLAGS) -DSATURATION_FLOAT_T=1

saturation_double_la_SOURCES = saturation.c
saturation_double_la_LIBADD = $(top_builddir)/libubx/libubx.la
saturation_double_la_CFLAGS = $(AM_CFLAGS) -DSATURATION_DOUBLE_T=1

saturation_int32_la_SOURCES = saturation.c
saturation_int32_la_LIBADD = $(top_builddir)/libubx/libubx.la
saturation_int32_la_CFLAGS = $(AM_CFLAGS) -DSATURATION_INT32_T=1

saturation_int64_la_SOURCES = saturation.c
saturation_int64_la_LIBADD = $(top_builddir)/libubx/libubx.la
saturation_int64_la_CFLAGS = $(AM_CFLAGS) -DSATURATION_INT64_T=1
