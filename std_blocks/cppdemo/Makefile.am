lib_LTLIBRARIES = cppdemo.la

# cppdemo : CPP Demo block
cppdemo_la_includes = $(top_srcdir)/libubx/ubx.h
cppdemo_la_SOURCES = $(cppdemo_la_includes) cppdemo.cpp
cppdemo_la_LIBADD = $(top_builddir)/libubx/libubx.la @UBX_MODULE_LDFLAGS@
cppdemo_la_CPPFLAGS = -I$(top_srcdir)/libubx
