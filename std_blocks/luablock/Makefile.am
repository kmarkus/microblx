# luablock: luajit scriptable ubx block

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = luablock.la

luablock_la_SOURCES = luablock.c
luablock_la_CFLAGS = -I$(top_srcdir)/libubx $(LUAJIT_CFLAGS) @UBX_CFLAGS@ -fvisibility=hidden
luablock_la_LDFLAGS = -module -avoid-version -shared -export-dynamic $(LUAJIT_LIBS)
luablock_la_LIBADD = $(top_builddir)/libubx/libubx.la
