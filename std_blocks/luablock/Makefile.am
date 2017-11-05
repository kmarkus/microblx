lib_LTLIBRARIES = libluablock.la

#======================================
# luablock : Lua block
libluablock_la_includes = $(top_srcdir)/libubx/ubx.h
libluablock_la_SOURCES = $(libluablock_la_includes) \
                       luablock.c

libluablock_la_CFLAGS = $(LUAJIT_CFLAGS) -I$(top_srcdir)/libubx
libluablock_la_LDFLAGS = $(LUAJIT_LIBS)
