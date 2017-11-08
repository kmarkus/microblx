
ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@
ubxmod_LTLIBRARIES = kdl_types.la

BUILT_SOURCES =  types/kdl_frame.h.hexarr \
                 types/kdl_rotation.h.hexarr \
                 types/kdl_vector.h.hexarr \
                 types/kdl.h.hexarr \
                 types/kdl_twist.h.hexarr \
                 types/kdl_wrench.h.hexarr

CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

kdl_types_la_includes = $(top_srcdir)/libubx/ubx.h
kdl_types_la_SOURCES = $(kdl_types_la_includes) kdl_types.c
kdl_types_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
kdl_types_la_LIBADD = $(top_builddir)/libubx/libubx.la
kdl_types_la_CFLAGS = -I$(top_srcdir)/libubx

