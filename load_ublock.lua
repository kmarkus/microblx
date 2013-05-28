#!/usr/bin/luajit

local ffi = require("ffi")
local u5c_utils = require("u5c_utils")

-- local bit = require("bit")

ffi.cdef(u5c_utils.read_file("u5c_types.h"))
-- ffi.load("./libu5c.so")

-- create a node_info struct
ni=ffi.new("u5c_node_info_t")

comp = ffi.load("./random.so")

comp.__initialize_module(ni)
print(ni.components_len)
comp.__cleanup_module(ni)
print(ni.components_len)