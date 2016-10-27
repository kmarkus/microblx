lib_LTLIBRARIES = liblfds_cyclic.la

#======================================
# lfds_cyclic : A lock free interaction block
liblfds_cyclic_la_includes = $(top_srcdir)/libubx/ubx.h
liblfds_cyclic_la_SOURCES = $(liblfds_cyclic_la_includes) \
                         lfds_cyclic.c
liblfds_cyclic_la_LIBADD = $(top_builddir)/libubx/libubx.la
liblfds_cyclic_la_CPPFLAGS = -I$(top_srcdir)/libubx
