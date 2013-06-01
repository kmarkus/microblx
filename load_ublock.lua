#!/usr/bin/luajit

local ffi = require("ffi")
local u5c_utils = require("u5c_utils")
-- local bit = require("bit")

-- load u5c_types and library
ffi.cdef(u5c_utils.read_file("uthash_ffi.h"))
ffi.cdef(u5c_utils.read_file("u5c_types.h"))
u5c=ffi.load("./libu5c.so")

-- create a node_info struct
ni=ffi.new("u5c_node_info_t")

-- initalize the node
print("u5c_node_init:", u5c.u5c_node_init(ni))

comp = ffi.load("./random.so")

print(u5c.u5c_num_components(ni))
comp.__initialize_module(ni)

print("num components: ", u5c.u5c_num_components(ni))

print("creating instance");
u5c.u5c_component_create(ni, "random", "random1")

print("num components: ", u5c.u5c_num_components(ni))

print("running init")
rnd_inst.init(rnd_inst)

print("freeing", u5c.u5c_component_destroy(ni, rnd_inst.name))
print("freeing", u5c.u5c_component_destroy(ni, ffi.new("char[?]", 20, "fruba1")))


comp.__cleanup_module(ni)
print(u5c.u5c_num_components(ni))


