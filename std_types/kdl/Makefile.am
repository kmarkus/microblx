lib_LTLIBRARIES = libkdl_types.la

BUILT_SOURCES =  types/kdl_frame.h.hexarr \
                 types/kdl_rotation.h.hexarr \
                 types/kdl_vector.h.hexarr \
                 types/kdl.h.hexarr \
                 types/kdl_twist.h.hexarr \
                 types/kdl_wrench.h.hexarr
CLEANFILES = $(BUILT_SOURCES)

%.h.hexarr: %.h
	$(top_srcdir)/tools/file2carr.lua $<

#======================================
# kdl_types : The basic types
libkdl_types_la_includes = $(top_srcdir)/libubx/ubx.h
libkdl_types_la_SOURCES = $(libkdl_types_la_includes) \
                       kdl_types.c
libkdl_types_la_LIBADD = $(top_builddir)/libubx/libubx.la
libkdl_types_la_CPPFLAGS = -I$(top_srcdir)/libubx
#libkdl_types_la_LDFLAGS = -module -avoid-version
