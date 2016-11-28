lib_LTLIBRARIES = libluablock.la

#======================================
# luablock : Lua block
libluablock_la_includes = $(top_srcdir)/libubx/ubx.h
libluablock_la_SOURCES = $(libluablock_la_includes) \
                       luablock.c
libluablock_la_LIBADD = $(top_builddir)/libubx/libubx.la
libluablock_la_CPPFLAGS = -I$(top_srcdir)/libubx
