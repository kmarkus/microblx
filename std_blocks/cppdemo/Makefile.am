# cppdemo : dummy block to illustrate C++ blocks

lib_LTLIBRARIES = cppdemo.la

cppdemo_la_includes = $(top_srcdir)/libubx/ubx.h
cppdemo_la_SOURCES = $(cppdemo_la_includes) cppdemo.cpp
cppdemo_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
cppdemo_la_LIBADD = $(top_builddir)/libubx/libubx.la
cppdemo_la_CPPFLAGS = -I$(top_srcdir)/libubx
