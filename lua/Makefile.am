luajitmoddir = $(datadir)/lua/5.1

luajitmod_SCRIPTS = ubx.lua \
		cdata.lua \
		reflect.lua \
		blockdiagram.lua \
		umf.lua

UBX_IF = $(shell expr $(UBX_IF_CURRENT) - $(UBX_IF_AGE))

ubx.lua: ubx.lua.source
	cat $(srcdir)/ubx.lua.source | sed -e "s/UBX_IF/$(UBX_IF)/g" > ubx.lua
