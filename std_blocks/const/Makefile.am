AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden

ubxmoddir = $(UBX_MODDIR)

ubxmod_LTLIBRARIES = cconst.la iconst.la

# build the const c-block
cconst_la_SOURCES = const.c
cconst_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
cconst_la_LIBADD = $(top_builddir)/libubx/libubx.la

# build the const i-block
iconst_la_SOURCES = const.c
iconst_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
iconst_la_CFLAGS = $(AM_CFLAGS) -DBUILD_IBLOCK
iconst_la_LIBADD = $(top_builddir)/libubx/libubx.la
