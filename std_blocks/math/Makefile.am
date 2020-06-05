ubxmoddir = $(UBX_MODDIR)

AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden
AM_LDFLAGS = -module -avoid-version -shared -export-dynamic

ubxmod_LTLIBRARIES = math_double.la

math_double_la_SOURCES = math_double.c
math_double_la_LIBADD = $(top_builddir)/libubx/libubx.la
