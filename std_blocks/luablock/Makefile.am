pkglib_LTLIBRARIES = luablock.la

#======================================
# luablock : Lua block
luablock_la_includes = $(top_srcdir)/libubx/ubx.h
luablock_la_SOURCES = $(luablock_la_includes) \
                       luablock.c
luablock_la_LIBADD = $(top_srcdir)/libubx/libubx.la
luablock_la_CPPFLAGS = -I$(top_srcdir)/libubx
luablock_la_LDFLAGS = -module -avoid-version
