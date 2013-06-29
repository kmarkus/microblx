#!/usr/bin/luajit

local ffi = require("ffi")
local u5c = require "lua/u5c"
local u5c_utils = require("lua/u5c_utils")
local ts = tostring

-- prog starts here.
ni=u5c.node_create("testnode")

u5c.load_module(ni, "std_types/stdtypes/stdtypes.so")
u5c.load_module(ni, "std_blocks/webif/webif.so")
u5c.load_module(ni, "std_triggers/ptrig/ptrig.so")
u5c.ffi_load_types(ni)

print("creating instance of 'webif/webif'")
webif1=u5c.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'std_triggers/ptrig'")
ptrig1=u5c.block_create(ni, "std_triggers/ptrig", "ptrig1")

print("running webif init", u5c.block_init(ni, webif1))
print("running webif start", u5c.block_start(ni, webif1))

io.read()

print("running ptrig1 unload", u5c.block_unload(ni, "ptrig1"))
print("running webif1 unload", u5c.block_unload(ni, "webif1"))

u5c.unload_modules(ni)



