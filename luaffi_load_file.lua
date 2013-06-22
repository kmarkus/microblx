#!/usr/bin/luajit

ffi = require "ffi"
u5c_utils = require "lua/u5c_utils"

cstr = u5c_utils.read_file(arg[1])

ffi.cdef(cstr)