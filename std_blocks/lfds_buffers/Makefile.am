pkglib_LTLIBRARIES = lfds_cyclic.la

#======================================
# lfds_cyclic : A lock free interaction block
lfds_cyclic_la_includes = $(top_srcdir)/libubx/ubx.h
lfds_cyclic_la_SOURCES = $(lfds_cyclic_la_includes) \
                         lfds_cyclic.c
lfds_cyclic_la_LIBADD = liblfds611/bin/liblfds611.a \
                        $(top_srcdir)/libubx/libubx.la
lfds_cyclic_la_CPPFLAGS = -I$(top_srcdir)/libubx
lfds_cyclic_la_LDFLAGS = -module -avoid-version

liblfds611/bin/liblfds611.a:
	$(shell mkdir -p liblfds611/obj liblfds611/bin)
	make -C liblfds611
