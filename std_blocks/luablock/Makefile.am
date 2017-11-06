lib_LTLIBRARIES = luablock.la

# luablock : Lua block
luablock_la_includes = $(top_srcdir)/libubx/ubx.h
luablock_la_SOURCES = $(luablock_la_includes) luablock.c

luablock_la_CFLAGS = $(LUAJIT_CFLAGS) -I$(top_srcdir)/libubx
luablock_la_LIBADD = $(LUAJIT_LIBS) @UBX_MODULE_LDFLAGS@
