#!/usr/bin/luajit

ffi = require "ffi"

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
local function read_file(file)
   local f = io.open(file, "rb")
   local data = f:read("*all")
   f:close()
   return data
end


cstr = read_file(arg[1])

ffi.cdef(cstr)