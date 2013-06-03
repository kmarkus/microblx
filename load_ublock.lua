#!/usr/bin/luajit

local ffi = require("ffi")
local u5c_utils = require("lua/u5c_utils")
-- local bit = require("bit")

-- load u5c_types and library
ffi.cdef(u5c_utils.read_file("src/uthash_ffi.h"))
ffi.cdef(u5c_utils.read_file("src/u5c_types.h"))
u5c=ffi.load("src/libu5c.so")

-- create a node_info struct
ni=ffi.new("u5c_node_info_t")

-- initalize the node
print("u5c_node_init:", u5c.u5c_node_init(ni))

comp = ffi.load("std_blocks/random/random.so")

print(u5c.u5c_num_cblocks(ni))
comp.__initialize_module(ni)

print("num components: ", u5c.u5c_num_cblocks(ni))

print("creating instance");
rnd_inst=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random", "random1")

print("num components: ", u5c.u5c_num_cblocks(ni))

print("running init")
rnd_inst.init(rnd_inst)

print("freeing")
print("freeing", u5c.u5c_block_destroy(ni, ffi.C.BLOCK_TYPE_COMPUTATION, rnd_inst.name))
print("freeing", u5c.u5c_block_destroy(ni, 3, ffi.new("char[?]", 20, "fruba1")))


comp.__cleanup_module(ni)
print(u5c.u5c_num_cblocks(ni))


