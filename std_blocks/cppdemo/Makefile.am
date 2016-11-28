lib_LTLIBRARIES = libcppdemo.la

# cppdemo : CPP Demo block
libcppdemo_la_includes = $(top_srcdir)/libubx/ubx.h
libcppdemo_la_SOURCES = $(libcppdemo_la_includes) \
                     cppdemo.cpp
libcppdemo_la_LIBADD = $(top_builddir)/libubx/libubx.la
libcppdemo_la_CPPFLAGS = -I$(top_srcdir)/libubx
