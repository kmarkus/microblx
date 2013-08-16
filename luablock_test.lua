#!/usr/bin/luajit

local ffi = require("ffi")
local ubx = require "lua/ubx"
local ubx_utils = require("lua/ubx_utils")
local ts = tostring

-- prog starts here.
ni=ubx.node_create("testnode")

ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_blocks/webif/webif.so")
ubx.load_module(ni, "std_blocks/luablock/luablock.so")
ubx.load_module(ni, "std_triggers/ptrig/ptrig.so")
ubx.ffi_load_types(ni)

print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'std_triggers/ptrig'")
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1")

print("creating instance of 'lua/luablock'")
lb1=ubx.block_create(ni, "lua/luablock", "luablock1", 
		     { lua_file="/home/mk/prog/c/microblx/std_blocks/luablock/luablock.lua"})

print("running luablock init", ubx.block_init(lb1))
print("running webif init", ubx.block_init(webif1))
print("running webif start", ubx.block_start(webif1))

io.read()

-- print("running ptrig1 unload", ubx.block_unload(ni, "ptrig1"))
-- print("running webif1 unload", ubx.block_unload(ni, "webif1"))
-- print("running luablock unload", ubx.block_unload(ni, "luablock1"))


-- ubx.unload_modules(ni)
-- ubx.ubx.ubx_node_cleanup(ni)
ubx.node_cleanup(ni)