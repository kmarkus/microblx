ubxmoddir = ${UBX_MODDIR}

ACLOCAL_AMFLAGS= -I m4
ubxmod_PROGRAMS = platform_main
platform_main_SOURCES = main.c
platform_main_CFLAGS = @UBX_CFLAGS@ \
		       -I${top_srcdir}/../../../std_blocks/trig/types/

platform_main_LDFLAGS = -module -avoid-version -shared -export-dynamic  @UBX_LIBS@ -ldl -lpthread
