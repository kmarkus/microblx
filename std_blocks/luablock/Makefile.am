# luablock: luajit scriptable ubx block

ubxmoddir = $(libdir)/@PACKAGE@/@UBX_VER@
ubxmod_LTLIBRARIES = luablock.la

luablock_la_includes = $(top_srcdir)/libubx/ubx.h
luablock_la_SOURCES = $(luablock_la_includes) luablock.c
luablock_la_CFLAGS = $(LUAJIT_CFLAGS) -I$(top_srcdir)/libubx
luablock_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
luablock_la_LIBADD = $(LUAJIT_LIBS)
