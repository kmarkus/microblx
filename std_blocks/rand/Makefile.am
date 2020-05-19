ubxmoddir = $(UBX_MODDIR)

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

ubxmod_LTLIBRARIES = rand_float.la rand_double.la rand_uint32.la rand_int32.la

rand_float_la_SOURCES = rand.c
rand_float_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
rand_float_la_LIBADD = $(top_builddir)/libubx/libubx.la
rand_float_la_CPPFLAGS = -DRAND_FLOAT_T=1

rand_double_la_SOURCES = rand.c
rand_double_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
rand_double_la_LIBADD = $(top_builddir)/libubx/libubx.la
rand_double_la_CPPFLAGS = -DRAND_DOUBLE_T=1

rand_uint32_la_SOURCES = rand.c
rand_uint32_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
rand_uint32_la_LIBADD = $(top_builddir)/libubx/libubx.la
rand_uint32_la_CPPFLAGS = -DRAND_UINT32_T=1

rand_int32_la_SOURCES = rand.c
rand_int32_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
rand_int32_la_LIBADD = $(top_builddir)/libubx/libubx.la
rand_int32_la_CPPFLAGS = -DRAND_INT32_T=1
