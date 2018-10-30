# luablock: luajit scriptable ubx block

ubxmoddir = $(ubx_moddir)
ubxmod_LTLIBRARIES = luablock.la

luablock_la_includes = $(top_srcdir)/libubx/ubx.h
luablock_la_SOURCES = $(luablock_la_includes) luablock.c
luablock_la_CFLAGS = -I$(top_srcdir)/libubx $(LUAJIT_CFLAGS) @UBX_CFLAGS@
luablock_la_LDFLAGS = -module -avoid-version -shared -export-dynamic $(LUAJIT_LIBS)
luablock_la_LIBADD = $(top_builddir)/libubx/libubx.la
