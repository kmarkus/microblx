ubxmoddir = ${UBX_MODDIR}


ACLOCAL_AMFLAGS= -I m4
ubxmod_PROGRAMS = platform_main
platform_main_SOURCES = $(top_srcdir)/libubx/ubx.h  main.c
platform_main_CFLAGS = -I${top_srcdir}/libubx  @UBX_CFLAGS@
platform_main_LDFLAGS = -module -avoid-version -shared -export-dynamic  @UBX_LIBS@ -ldl -lpthread
