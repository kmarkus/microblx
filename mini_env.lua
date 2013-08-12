#!/usr/bin/luajit

ffi = require("ffi")
ubx = require "lua/ubx"
ubx_utils = require("lua/ubx_utils")
cdata = require"cdata"
ts = tostring

-- prog starts here.
ni=ubx.node_create("testnode")

ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/testtypes/testtypes.so")
ubx.load_module(ni, "std_blocks/random/random.so")
ubx.load_module(ni, "std_blocks/hexdump/hexdump.so")
ubx.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
ubx.load_module(ni, "std_blocks/webif/webif.so")
ubx.load_module(ni, "std_triggers/ptrig/ptrig.so")
ubx.ffi_load_types(ni)


print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("running webif init", ubx.block_init(ni, webif1))
print("running webif start", ubx.block_start(ni, webif1))

i1=ubx.data_alloc(ni, "testtypes/struct Vector", 1)
print(ubx.data_tostr(i1))


print(ubx.block_tostr(webif1))