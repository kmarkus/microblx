#!/usr/bin/luajit

local ffi = require("ffi")
local ubx_utils = require("lua/ubx_utils")
-- local bit = require("bit")

-- load ubx_types and library
ffi.cdef(ubx_utils.read_file("src/uthash_ffi.h"))
ffi.cdef(ubx_utils.read_file("src/ubx_types.h"))
ubx=ffi.load("src/libubx.so")

-- create a node_info struct
ni=ffi.new("ubx_node_info_t")

-- initalize the node
print("ubx_node_init:", ubx.ubx_node_init(ni))

comp = ffi.load("std_blocks/random/random.so")

print(ubx.ubx_num_cblocks(ni))
comp.__initialize_module(ni)

print("num components: ", ubx.ubx_num_cblocks(ni))

print("creating instance");
rnd_inst=ubx.ubx_block_create(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random", "random1")

print("num components: ", ubx.ubx_num_cblocks(ni))

print("running init")
rnd_inst.init(rnd_inst)

print("freeing")
print("freeing", ubx.ubx_block_destroy(ni, ffi.C.BLOCK_TYPE_COMPUTATION, rnd_inst.name))
print("freeing", ubx.ubx_block_destroy(ni, 3, ffi.new("char[?]", 20, "fruba1")))


comp.__cleanup_module(ni)
print(ubx.ubx_num_cblocks(ni))


