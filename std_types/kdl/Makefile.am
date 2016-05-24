pkglib_LTLIBRARIES = kdl_types.la

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
kdl_types_la_includes = $(top_srcdir)/libubx/ubx.h
kdl_types_la_SOURCES = $(kdl_types_la_includes) \
                       kdl_types.c
kdl_types_la_LIBADD = $(top_srcdir)/libubx/libubx.la
kdl_types_la_CPPFLAGS = -I$(top_srcdir)/libubx
kdl_types_la_LDFLAGS = -module -avoid-version
